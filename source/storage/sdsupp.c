/****************************************************************************
 * SD Card Support Functions
 ****************************************************************************/
/**
 * @file sdsupp.c
 * @brief Save-image file I/O on the selected mounted storage device.
 *
 * Historical SD names apply to every supported mounted device. Directory names
 * are treated as untrusted, file ranges are bounded, and GCI/GCS/SAV inputs
 * are normalized into FileBuffer before card writes.
 */
#include <gccore.h>
#include <network.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/dir.h>
#include <dirent.h>

#include "sdsupp.h"
#include "freetype.h"
#include "ui.h"
#include "gci.h"
#include "mcard.h"
#include "raw.h"

#define PAGESIZE 20
#define PADCAL 80

#define DIRENT_T_FILE 0
#define DIRENT_T_DIR 1

/*** Memory Card FileBuffer ***/
#define MAXFILEBUFFER (1024 * 2048)	/*** 2MB Buffer ***/
extern u8 FileBuffer[MAXFILEBUFFER] ATTRIBUTE_ALIGN (32);
extern Header cardheader;
extern u8 CommentBuffer[64] ATTRIBUTE_ALIGN (32);

extern u16 bannerdata[CARD_BANNER_W*CARD_BANNER_H] ATTRIBUTE_ALIGN (32);
extern u8 bannerdataCI[CARD_BANNER_W*CARD_BANNER_H] ATTRIBUTE_ALIGN (32);
extern u8 icondata[8][1024] ATTRIBUTE_ALIGN (32);
extern u16 icondataRGB[8][1024] ATTRIBUTE_ALIGN (32);
extern u16 tlut[9][256] ATTRIBUTE_ALIGN (32);
extern u16 tlutbanner[256] ATTRIBUTE_ALIGN (32);
extern int numicons;
extern int frametable[2*CARD_MAXICONS - 2];
extern int iconindex[2*CARD_MAXICONS - 2];
extern int lastframe;
extern int lasticon;

extern u8 filelist[1024][1024];
extern card_direntry gci;
int OFFSET = 0;

extern u8 currFolder[260];
extern char fatpath[8];

static int get_file_size(FILE *handle, long *size)
{
	long result;

	if (!handle || !size || fseek(handle, 0, SEEK_END) != 0)
		return 0;
	result = ftell(handle);
	if (result < 0 || fseek(handle, 0, SEEK_SET) != 0)
		return 0;
	*size = result;
	return 1;
}

static int read_file_range(FILE *handle, long file_size, long offset,
	void *destination, size_t length)
{
	if (!handle || !destination || file_size < 0 || offset < 0 ||
		offset > file_size || length > (size_t)(file_size - offset) ||
		fseek(handle, offset, SEEK_SET) != 0)
		return 0;
	return fread(destination, 1, length, handle) == length;
}

static int read_file_range_advance(FILE *handle, long file_size, long *offset,
	void *destination, size_t length)
{
	if (!offset || !read_file_range(handle, file_size, *offset, destination, length))
		return 0;
	*offset += (long)length;
	return 1;
}

static int storage_entry_name_is_safe(const char *name)
{
	if (!name || !name[0] || strnlen(name, 1024) >= 1024)
		return 0;
	return strcmp(name, ".") != 0 && strcmp(name, "..") != 0 &&
		strchr(name, '/') == NULL && strchr(name, '\\') == NULL;
}

static int storage_folder_is_safe(void)
{
	return storage_entry_name_is_safe((const char *)currFolder);
}

bool file_exists(const char * filename)
{
	FILE * file = fopen(filename, "r");
	if (file)
	{
		fclose(file);
		return true;
	}
	return false;
}

int SDSaveMCImage ()
{

	char filename[1024];
	char tfile[40];
	char company[4];
	char gamecode[6];
	int bytesToWrite = 0;
	//sd_file *handle;
	FILE *handle;
	card_direntry thisgci;
	long check;
	size_t written;
	int filenumber = 0;
	int retries = 0;
	int filename_length;

	/*** Make a copy of the Card Dir ***/
	memcpy (&thisgci, FileBuffer, sizeof (card_direntry));
	memset( tfile, 0, 40 );
	company[2] = 0;
	gamecode[4] = 0;
	memcpy (company, &thisgci.company, 2);
	memcpy (gamecode, &thisgci.gamecode, 4);
	memcpy (tfile, &thisgci.filename, CARD_FILENAMELEN);

	filename_length = snprintf(filename, sizeof(filename), "%s:/%s", fatpath,
		MCSAVES);
	if (filename_length < 0 || filename_length >= (int)sizeof(filename))
		return 0;

	mkdir(filename, S_IREAD | S_IWRITE);

	filename_length = snprintf(filename, sizeof(filename),
		"%s:/%s/%s-%s-%s_%02d.gci", fatpath, MCSAVES, company, gamecode,
		tfile, filenumber);
	if (filename_length < 0 || filename_length >= (int)sizeof(filename))
		return 0;

	//Lets try if there's already a savegame (if it exists its name is legal, so theoretically the illegal name check will pass
	//Illegal savegame should report as nonexisting...
	//We will number the files
	while (file_exists(filename)){
		filenumber++;
		filename_length = snprintf(filename, sizeof(filename),
			"%s:/%s/%s-%s-%s_%02d.gci", fatpath, MCSAVES, company, gamecode,
			tfile, filenumber);
		if (filename_length < 0 || filename_length >= (int)sizeof(filename))
			return 0;
	}

	//filename[128] = 0; // limit filename length, there's a bug where a file is exported as DOS-type shortname ("8P-GPO~2.GCI") which I assume happens if the name is too long...? Nope that's not it.

	while ( 1 ) // loop the writing because it sometimes fails (see above); I think this is an error in libfat but I dunno.
	{
		/*** Open SD Card file ***/
		//handle = SDCARD_OpenFile (filename, "wb");
		handle = fopen ( filename , "wb" );
		if (handle <= 0)
		{
			// couldn't open file, probably either card full or filename illegal; let's assume illegal filename and retry
			filename_length = snprintf(filename, sizeof(filename),
				"%s:/%s/%s-%s-%s_%02d.gci", fatpath, MCSAVES, company,
				gamecode, "illegal_name", filenumber);
			if (filename_length < 0 || filename_length >= (int)sizeof(filename))
				return 0;
			//let's see again if there aren't any saves already...
			filenumber = 0;
			while (file_exists(filename)){
				filenumber++;
				filename_length = snprintf(filename, sizeof(filename),
					"%s:/%s/%s-%s-%s_%02d.gci", fatpath, MCSAVES, company,
					gamecode, "illegal_name", filenumber);
				if (filename_length < 0 || filename_length >= (int)sizeof(filename))
					return 0;
			}
			//filename[128] = 0;
			handle = fopen ( filename , "wb" );
			if (handle <= 0)
			{
				char msg[100];
				snprintf(msg, sizeof(msg), "Couldn't open %.84s", filename);
				WaitPrompt (msg);
				return 0;
			}
		}

		if (thisgci.length == 0 ||
			thisgci.length > (MAXFILEBUFFER - MCDATAOFFSET) / 8192) {
			fclose(handle);
			return 0;
		}
		bytesToWrite = (int)thisgci.length * 8192 + MCDATAOFFSET;
		//SDCARD_WriteFile (handle, FileBuffer, bytesToWrite);
		//SDCARD_CloseFile (handle);
		written = fwrite(FileBuffer, 1, bytesToWrite, handle);
		if (written != bytesToWrite || fflush(handle) != 0)
		{
			fclose(handle);
			return 0;
		}
		if (fclose(handle) != 0)
			return 0;

		// check if file actually wrote correctly
		handle = fopen ( filename , "rb" );
		if (handle <= 0)
		{
			// file failed to open, so something failed in the write; retry
			retries ++;
			if (retries > 9)
			{
				return 0;
			}
			continue;
		}
		if (!get_file_size(handle, &check)) {
			fclose(handle);
			return 0;
		}
		fclose(handle);
		if (check == bytesToWrite) {
			/* A backup is valid only when its on-disk size matches the GCI header. */
			break;
		}
		retries++;

		//Try 10 times if needed, then give up. Better to avoid endless looping
		if (retries > 9)
		{
			return 0;
		}

	}

	return 1;
}

int SDLoadMCImage(char *sdfilename)
{

	//sd_file *handle;
	FILE *handle;
	char filename[1024];
	char msg[256];
	//int offset = 0;
	//int bytesToRead = 0;
	long bytesToRead = 0;
	long normalized_size;
	u16 block_count;
	int path_length;

	/*** Clear the work buffers ***/
	memset (FileBuffer, 0, MAXFILEBUFFER);
	memset (CommentBuffer, 0, 64);

	/*** Make fullpath filename ***/
	//sprintf (filename, "dev0:\\%s\\%s", MCSAVES, sdfilename);
	if (!storage_entry_name_is_safe(sdfilename) || !storage_folder_is_safe())
		return 0;
	path_length = snprintf(filename, sizeof(filename), "%s:/%s/%s", fatpath,
		currFolder, sdfilename);
	if (path_length < 0 || path_length >= (int)sizeof(filename))
		return 0;

	//SDCARD_Init ();

	/*** Does this file exist ? ***/
	/*if (SDCARD_SeekFile(filename, 0, SDCARD_SEEK_SET) != SDCARD_ERROR_READY){
	   return 0;
	}*/

	/*** Open the SD Card file ***/
	//handle = SDCARD_OpenFile (filename, "rb");
	handle = fopen ( filename , "rb" );
	if (!handle)
	{
		snprintf(msg, sizeof(msg), "Couldn't open %.230s", filename);
		WaitPrompt (msg);
		return 0;
	}


	/*    if (handle == NULL){
	      WaitPrompt ("Unable to open file!");
	      return 0;
	    }*/

	// obtain file size:
	if (!get_file_size(handle, &bytesToRead)) {
		fclose(handle);
		WaitPrompt("Could not determine backup file size.");
		return 0;
	}

	//bytesToRead = SDCARD_GetFileSize(handle);
	if (bytesToRead <= 0)
	{
		sprintf(msg, "Incorrect file size %ld .", bytesToRead);
		WaitPrompt (msg);
		fclose(handle);
		return 0;
	}
	if (OFFSET < 0 || OFFSET >= bytesToRead ||
		bytesToRead - OFFSET < MCDATAOFFSET ||
		bytesToRead - OFFSET > MAXFILEBUFFER) {
		WaitPrompt("Backup file is too large or has an invalid header offset.");
		fclose(handle);
		return 0;
	}
	/*** Read the file ***/
	//SDCARD_ReadFile (handle, FileBuffer, bytesToRead);
	if (!read_file_range(handle, bytesToRead, OFFSET, FileBuffer,
		(size_t)(bytesToRead - OFFSET))) {
		fclose(handle);
		WaitPrompt("Could not read complete backup file.");
		return 0;
	}
	if(OFFSET == 0x80)
	{
		// swap byte pairs
		// 0x06 and 0x07
		u8 temp = FileBuffer[0x06];
		FileBuffer[0x06] = FileBuffer[0x07];
		FileBuffer[0x07] = temp;

		// swap byte pairs
		// 0x2C and 0x2D, 0x2E and 0x2F, 0x30 and 0x31, 0x32 and 0x33,
		// 0x34 and 0x35, 0x36 and 0x37, 0x38 and 0x39, 0x3A and 0x3B,
		// 0x3C and 0x3D,0x3E and 0x3F.
		int i = 0;
		while(i<10)
		{
			u8 temp = FileBuffer[0x2C + i*2];
			FileBuffer[0x2C + i*2] = FileBuffer[0x2C + i*2+1];
			FileBuffer[0x2C + i*2+1] = temp;
			i++;
		}
	}
	normalized_size = bytesToRead - OFFSET;
	block_count = (u16)(FileBuffer[0x38] << 8) | FileBuffer[0x39];
	if (block_count == 0 ||
		block_count > (MAXFILEBUFFER - MCDATAOFFSET) / 8192 ||
		normalized_size != MCDATAOFFSET + (long)block_count * 8192) {
		fclose(handle);
		WaitPrompt("Backup file length does not match its header.");
		return 0;
	}
	/* FileBuffer now always begins with a normalized GCI header. */
	OFFSET = 0;
	//sprintf(msg, "Offset: %d", bytesToRead);
	//WaitPrompt (msg);
	/*** Close the file ***/
	//SDCARD_CloseFile (handle);
	fclose (handle);

	return bytesToRead;
}

int SDLoadMCImageHeader(char *sdfilename)
{

	FILE *handle;
	char filename[1024];
	char msg[256];
	//int offset = 0;
	//int bytesToRead = 0;
	long bytesToRead = 0;
	int i;
	int path_length;

	/*** Clear the work buffers ***/
	memset (FileBuffer, 0, MAXFILEBUFFER);
	memset (CommentBuffer, 0, 64);

	/*** Make fullpath filename ***/
	if (!storage_entry_name_is_safe(sdfilename) || !storage_folder_is_safe())
		return 0;
	path_length = snprintf(filename, sizeof(filename), "%s:/%s/%s", fatpath,
		currFolder, sdfilename);
	if (path_length < 0 || path_length >= (int)sizeof(filename))
		return 0;


	/*** Open the SD Card file ***/
	//handle = SDCARD_OpenFile (filename, "rb");
	handle = fopen ( filename , "rb" );
	if (!handle)
	{
		snprintf(msg, sizeof(msg), "Couldn't open %.230s", filename);
		WaitPrompt (msg);
		return 0;
	}

	// obtain file size:
	if (!get_file_size(handle, &bytesToRead)) {
		fclose(handle);
		WaitPrompt("Could not determine backup file size.");
		return 0;
	}
	if (bytesToRead < 64) //We don't want to read something smaller than the header
	{
		snprintf(msg, sizeof(msg), "Incorrect file size %ld. Not GCI file.", bytesToRead);
		WaitPrompt (msg);
		fclose(handle);
		return 0;
	}

	OFFSET = 0;
	char tmp[0xD];
	char fileType[4];
	char *dot;
	dot = strrchr(filename,'.');
	if (!dot || !dot[1]) {
		fclose(handle);
		WaitPrompt("Backup file has no supported extension.");
		return 0;
	}
	if (strlen(dot + 1) != 3) {
		fclose(handle);
		WaitPrompt("Backup file has an unsupported extension.");
		return 0;
	}
	memcpy(fileType, dot + 1, 3);
	fileType[3] = '\0';
	if (!strcasecmp(fileType, "gci")) {
		OFFSET = 0;
	} else {
		if (!read_file_range(handle, bytesToRead, 0, tmp, sizeof(tmp))) {
			fclose(handle);
			WaitPrompt("Backup file header is truncated.");
			return 0;
		}
		if (!strcasecmp(fileType, "gcs") && !memcmp(tmp, "GCSAVE", 6)) {
			OFFSET = 0x110;
		} else if (!strcasecmp(fileType, "sav") &&
			!memcmp(tmp, "DATELGC_SAVE", 0xC)) {
			OFFSET = 0x80;
		} else {
			fclose(handle);
			WaitPrompt("Backup type or header is not supported.");
			return 0;
		}
	}
	if (OFFSET < 0 || bytesToRead < OFFSET + (long)sizeof(card_direntry) ||
		bytesToRead - OFFSET > MAXFILEBUFFER ||
		(bytesToRead - OFFSET - sizeof(card_direntry)) % 0x2000 != 0 ||
		!read_file_range(handle, bytesToRead, OFFSET, FileBuffer,
			sizeof(card_direntry))) {
		fclose(handle);
		WaitPrompt("Backup has an invalid size or truncated header.");
		return 0;
	}

	/*** Read the file header ***/

	u16 length = (bytesToRead - OFFSET - sizeof(card_direntry)) / 0x2000;
	switch(OFFSET)
	{
	case 0x110:
	{
		// field containing the Block count as displayed within
		// the GameSaves software is not stored in the GCS file.
		// It is stored only within the corresponding GSV file.
		// If the GCS file is added without using the GameSaves software,
		// the value stored is always "1"
		FileBuffer[0x38] = (u8)(length >> 8);
		FileBuffer[0x39] = (u8)length;
	}
	break;
	case 0x80:
	{
		// swap byte pairs
		// 0x06 and 0x07

		u8 temp = FileBuffer[0x06];
		FileBuffer[0x06] = FileBuffer[0x07];
		FileBuffer[0x07] = temp;

		// swap byte pairs
		// 0x2C and 0x2D, 0x2E and 0x2F, 0x30 and 0x31, 0x32 and 0x33,
		// 0x34 and 0x35, 0x36 and 0x37, 0x38 and 0x39, 0x3A and 0x3B,
		// 0x3C and 0x3D,0x3E and 0x3F.
		int i = 0;
		while(i<10)
		{
			u8 temp = FileBuffer[0x2C + i*2];
			FileBuffer[0x2C + i*2] = FileBuffer[0x2C + i*2+1];
			FileBuffer[0x2C + i*2+1] = temp;
			i++;
		}
		break;
	}
	default:
		break;
	}
	u16 l2 =(u16)(FileBuffer[0x38] << 8) | FileBuffer[0x39];
	if (length !=  l2)
	{
		snprintf(msg, sizeof(msg), "File length mismatch (file %x, header %x).", length, l2);
		WaitPrompt (msg);//"File Length does not equal filesize");
		fclose(handle);
		return 0;
	}

	ExtractGCIHeader();
#ifdef STATUSOGC
	GCIMakeHeader();
#else
	//Let's get the full header as is, instead of populating it...
	memset(&gci, 0xff, sizeof(card_direntry)); /*** Clear out the cgi header ***/
	memcpy (&gci, FileBuffer, sizeof (card_direntry));
#endif

	/***
		Get the Banner/Icon Data from the SD save file.
		Very specific if/else setup to avoid rewinds
	***/
	long data_start = MCDATAOFFSET + OFFSET;
	long visual_offset;
	if (data_start > bytesToRead || gci.icon_addr > (u32)(bytesToRead - data_start)) {
		fclose(handle);
		WaitPrompt("Backup icon offset is outside the file.");
		return 0;
	}
	visual_offset = data_start + (long)gci.icon_addr;

	/*** Get the Banner/Icon Data from the save file ***/
	if (SDCARD_GetBannerFmt(gci.banner_fmt) == CARD_BANNER_RGB) {
		//RGB banners are 96*32*2 in size
		if (!read_file_range_advance(handle, bytesToRead, &visual_offset,
			bannerdata, 6144))
			goto invalid_visual_data;
	}
	else if (SDCARD_GetBannerFmt(gci.banner_fmt) == CARD_BANNER_CI) {
		if (!read_file_range_advance(handle, bytesToRead, &visual_offset,
			bannerdataCI, 3072) ||
			!read_file_range_advance(handle, bytesToRead, &visual_offset,
				tlutbanner, 512))
			goto invalid_visual_data;
	}
	//Icon data
	int shared_pal = 0;
	lastframe = 0;
	numicons = 0;
	int j =0;
    i=0;
	int current_icon = 0;
	for (current_icon = 0; current_icon < CARD_MAXICONS; ++current_icon) {

		//no need to clear all values since we will only use the ones until lasticon
		frametable[current_icon] = 0;
		iconindex[current_icon] = 0;

		//Animation speed is mandatory to be set even for a single icon
		//When a speed is 0 there are no more icons
		//Some games may have bits set after the "blank icon" both in
		//speed (Baten Kaitos) and format (Wario Ware Inc.) bytes, which are just garbage
		if (!SDCARD_GetIconSpeed(gci.icon_speed,current_icon)){
			break;
		}else
		{//We've got a frame
			lastframe+=SDCARD_GetIconSpeed(gci.icon_speed,current_icon)*4;//Count the total frames
			frametable[current_icon]=lastframe; //Store the end frame of the icon

			if (SDCARD_GetIconFmt(gci.icon_fmt,current_icon) != 0)
			{
				//count the number of real icons
				numicons++;
				iconindex[current_icon]=current_icon; //Map the icon

				//CI with shared palette
				if (SDCARD_GetIconFmt(gci.icon_fmt,current_icon) == 1) {
					if (!read_file_range_advance(handle, bytesToRead, &visual_offset,
						icondata[current_icon], 1024))
						goto invalid_visual_data;
					shared_pal = 1;
				}
				//CI with palette after the icon
				else if (SDCARD_GetIconFmt(gci.icon_fmt,current_icon) == 3)
				{
					if (!read_file_range_advance(handle, bytesToRead, &visual_offset,
						icondata[current_icon], 1024) ||
						!read_file_range_advance(handle, bytesToRead, &visual_offset,
							tlut[current_icon], 512))
						goto invalid_visual_data;
				}
				//RGB 16 bit icon
				else if (SDCARD_GetIconFmt(gci.icon_fmt,current_icon) == 2)
				{
					if (!read_file_range_advance(handle, bytesToRead, &visual_offset,
						icondataRGB[current_icon], 2048))
						goto invalid_visual_data;
				}
			}else
			{       //Get next real icon
                    for(j=current_icon;j<CARD_MAXICONS;++j){

                        if (SDCARD_GetIconFmt(gci.icon_fmt,j) != 0)
                        {
                            iconindex[current_icon]=j; //Map blank frame to next real icon
                            break;
                        }

                    }
			}
		}
	}

    lasticon = current_icon-1;
    //Now get icon indexes for ping-pong style icons
    if (SDCARD_GetIconAnim(gci.banner_fmt) == CARD_ANIM_BOUNCE && current_icon>1) //We need at least 3 icons
    {
        j=current_icon;
        for (i = current_icon-2; 0 < i; --i, ++j)
        {
            lastframe += SDCARD_GetIconSpeed(gci.icon_speed,i)*4;
            frametable[j] = lastframe;
            iconindex[j] = iconindex[i];
        }
        lasticon = j-1;
    }
	//Get the shared palette
	if (shared_pal && !read_file_range_advance(handle, bytesToRead, &visual_offset,
		tlut[8], 512))
		goto invalid_visual_data;

	//Get the comment
	if (data_start > bytesToRead || gci.comment_addr > (u32)(bytesToRead - data_start) ||
		!read_file_range(handle, bytesToRead, data_start + (long)gci.comment_addr,
			CommentBuffer, sizeof(CommentBuffer)))
		goto invalid_visual_data;

	/*** Close the file ***/
	fclose (handle);

	return bytesToRead;

invalid_visual_data:
	fclose(handle);
	WaitPrompt("Backup banner, icon, or comment data is truncated.");
	return 0;
}

int SDLoadCardImageHeader(char *sdfilename)
{

	FILE *handle;
	char filename[1024];
	char msg[256];
	long bytesToRead = 0;
	long header_offset = 0;
	int path_length;

	/*** Clear the work buffers ***/
	memset (&cardheader, 0, sizeof(Header));

	/*** Make fullpath filename ***/
	if (!storage_entry_name_is_safe(sdfilename) || !storage_folder_is_safe())
		return 0;
	path_length = snprintf(filename, sizeof(filename), "%s:/%s/%s", fatpath,
		currFolder, sdfilename);
	if (path_length < 0 || path_length >= (int)sizeof(filename))
		return 0;

	/*** Open the SD Card file ***/
	handle = fopen ( filename , "rb" );
	if (!handle)
	{
		snprintf(msg, sizeof(msg), "Couldn't open %.230s", filename);
		WaitPrompt (msg);
		return 0;
	}

	// obtain file size:
	if (!get_file_size(handle, &bytesToRead)) {
		fclose(handle);
		return 0;
	}
	if (bytesToRead < 8192) //We don't want to read something smaller than the card header
	{
		sprintf(msg, "Incorrect file size %ld . Not raw image file or header", bytesToRead);
		WaitPrompt (msg);
		fclose(handle);
		return 0;
	}

	char fileType[5];
	char * dot;
	dot = strrchr(filename,'.');
	if (!dot || strlen(dot + 1) != 3) {
		fclose(handle);
		return 0;
	}
	snprintf(fileType, sizeof(fileType), "%s", dot + 1);

	if(!strcasecmp(fileType, "mci"))
	{
		//MCI files have a 64 byte header
		header_offset = 64;
	}
	memset(&cardheader, 0, sizeof(cardheader));
	/*** Read the file header ***/
	if (!read_file_range(handle, bytesToRead, header_offset, &cardheader,
		sizeof(cardheader))) {
		fclose(handle);
		return 0;
	}

	/*** Close the file ***/
	fclose (handle);

	return bytesToRead;
}

int isdir_sd(char *path)
{
	DIR* dir = opendir(path);
	if(dir == NULL)
		return 0;

	closedir(dir);

	return 1;
}

//Code from Kobie. Returns true if extension matches (also works with paths), should check only the last '.' in the string.
static bool compare_extension(char *filename, char *extension)
{
    /* Sanity checks */

    if(filename == NULL || extension == NULL)
        return false;

    if(strlen(filename) == 0 || strlen(extension) == 0)
        return false;

    if(strchr(filename, '.') == NULL || strchr(extension, '.') == NULL)
        return false;

    /* Iterate backwards through respective strings and compare each char one at a time */
	int i;
    for(i = 0; i < strlen(filename); i++)
    {
        if(tolower(filename[strlen(filename) - i - 1]) == tolower(extension[strlen(extension) - i - 1]))
        {
            if(i == strlen(extension) - 1)
                return true;
        } else
            break;
    }

    return false;
}

/****************************************************************************
* SDGetFileList
*
* Get the directory listing from SD Card
* Mode 1: retrieves .gci, .sav and .gcs
* Mode 0: retrieves .raw, .gcp and .mci
****************************************************************************/
int SDGetFileList(int mode)
{
	int filecount = 0;
	int length;
	DIR *dir;
	struct dirent *dit;
	char namefile[1280]; // path plus a maximum-length directory entry
	//static struct stat filestat;

	int dirCount = 0;

	char filename[1024];
	if (!storage_folder_is_safe())
		return -1;
	length = snprintf(filename, sizeof(filename), "%s:/%s/", fatpath, currFolder);
	if (length < 0 || length >= (int)sizeof(filename))
		return -1;


	//Add Folders
	if ((dir = opendir(filename)) == NULL)
	{
		return -1;
	}
	
	while ((dit = readdir(dir)) != NULL)
	{
		if (filecount >= 1024)
			break;
		if (storage_entry_name_is_safe(dit->d_name))
		{
			length = snprintf(namefile, sizeof(namefile), "%s%s", filename,
				dit->d_name);
			if (length < 0 || length >= (int)sizeof(namefile))
				continue;

			if(isdir_sd(namefile) == 1)
			{
				length = snprintf((char *)filelist[filecount], 1024, "%s",
					dit->d_name);
				if (length < 0 || length >= 1024)
					continue;
				dirCount++;
				filecount++;
			}
		}
	}

	closedir(dir);


	//Add Files
	if ((dir = opendir(filename)) == NULL)
	{
		return -1;
	}

	while ((dit = readdir(dir)) != NULL)
	{
		if (filecount >= 1024)
			break;
		if (storage_entry_name_is_safe(dit->d_name))
		{
			if (mode)
			{
				if (compare_extension(dit->d_name, ".gci") || compare_extension(dit->d_name, ".sav") || compare_extension(dit->d_name, ".gcs"))
				{
					length = snprintf((char *)filelist[filecount], 1024, "%s",
						dit->d_name);
					if (length < 0 || length >= 1024)
						continue;

					filecount++;
				}
			}
			else if (!mode)
			{
				if (compare_extension(dit->d_name, ".raw") || compare_extension(dit->d_name, ".gcp") || compare_extension(dit->d_name, ".mci"))
				{
					length = snprintf((char *)filelist[filecount], 1024, "%s",
						dit->d_name);
					if (length < 0 || length >= 1024)
						continue;

					filecount++;
				}
			}

		}
	}
	
	
	//Dragonbane: Sort folders alphabetically
	int i=0;
	int j=0;
	
	for (i = 0; i < dirCount; i++) 
	{
      for (j = i+1; j < dirCount; j++)
         if (strcmp((char*)filelist[i], (char*)filelist[j]) > 0) 
		 {
            char temp[1024];
			
			strncpy(temp, (char*)filelist[i], sizeof(temp) - 1);
			temp[sizeof(temp) - 1] = '\0';
			snprintf((char*)filelist[i], 1024, "%s", (char*)filelist[j]);
			snprintf((char*)filelist[j], 1024, "%s", temp);
         }
    }
	
	
	//Dragonbane: Sort files alphabetically
	
    for (i = dirCount; i < filecount; i++) 
	{
      for (j = i+1; j < filecount; j++)
         if (strcmp((char*)filelist[i], (char*)filelist[j]) > 0) 
		 {
            char temp[1024];
			
			strncpy(temp, (char*)filelist[i], sizeof(temp) - 1);
			temp[sizeof(temp) - 1] = '\0';
			snprintf((char*)filelist[i], 1024, "%s", (char*)filelist[j]);
			snprintf((char*)filelist[j], 1024, "%s", temp);
         }
    }

	/* while(1)
	{
		if(readdir(dir)!=0) break; // si no hay mas entradas en el directorio, sal

		if(filestat.st_mode & S_IFDIR) // es el nombre de un directorio
		{
			// namefile contiene el nombre del directorio en formato UTF-8,que puede ser "." o ".." tambien
		} else {
			// namefile contiene el nombre del fichero en formato UTF-8
			strcpy ((char *)filelist[filecount], namefile);
			filecount++;
		}
	}*/

	closedir(dir); // cierra el directorio
	return filecount;

	/*  int entries = 0;
	  int filecount = 0;
	  char filename[1024];
	  //DIR *sddir = NULL;
	  DIR_ITER *sddir = NULL;

	  SDCARD_Init ();

	  sprintf (filename, "dev0:\\%s\\", MCSAVES);

	  entries = SDCARD_ReadDir (filename, &sddir);

	  while (entries){
	      strcpy (filelist[filecount], sddir[filecount].fname);
	      filecount++;
	      entries--;
	  }

	  free(sddir);

	  return filecount;
	  */
}
