/****************************************************************************
* Memory Card
*
* Supporting functions.
*
* MAXFILEBUFFER is set to 2MB as it's the largest file I've seen.
*
* CARDBACKUP FILE
*
*   0	- Copy of CardDir
*  64   - Copy of CardStatus
* 192   - Memory Card File Data
****************************************************************************/
/**
 * @file mcard.c
 * @brief High-level GameCube memory-card save management.
 *
 * FileBuffer is a 2 MiB aligned shared buffer. A loaded save starts with a
 * 64-byte GCI directory entry followed by a block-aligned payload. Keep size
 * and alignment unchanged: GameCube memory is constrained and CARD uses DMA.
 * Each operation mounts a card only while needed, then unmounts before return.
 */
#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zlib.h>
#include <ogc/libversion.h>
#if (_V_MAJOR_ <= 2) && (_V_MINOR_ <= 2)
/* gci.h's card_direntry and card.c's struct card_direntry describe the same
   64 byte directory entry under two naming conventions, so the cast renames
   fields rather than reinterpreting bytes. gcc 14 promoted the mismatch from
   a warning to an error. */
#ifndef CARD_SetStatusEx
#define CARD_SetStatusEx(chn,fileno,entry) \
	__card_setstatusex((chn),(fileno),(struct card_direntry *)(entry))
#endif
#ifndef CARD_GetStatusEx
#define CARD_GetStatusEx(chn,fileno,entry) \
	__card_getstatusex((chn),(fileno),(struct card_direntry *)(entry))
#endif
#include "card.h"
extern s32 __card_setstatusex(s32 chn,s32 fileno,struct card_direntry *entry);
extern s32 __card_getstatusex(s32 chn,s32 fileno,struct card_direntry *entry);
s32 CARD_GetFreeBlocks(s32 chn, u16* freeblocks);
s32 CARD_GetSerialNo(s32 chn,u32 *serial1,u32 *serial2);
#else
#include <ogc/card.h>
#endif

#include "mcard.h"
#include "gci.h"
#include "freetype.h"

/*** Memory Card Work Area ***/
static u8 SysArea[CARD_WORKAREA] ATTRIBUTE_ALIGN (32);

/*** Memory File Buffer ***/
#define MAXFILEBUFFER (1024 * 2048)	/*** 2MB Buffer ***/
u8 FileBuffer[MAXFILEBUFFER] ATTRIBUTE_ALIGN (32);
u8 CommentBuffer[64] ATTRIBUTE_ALIGN (32);

u16 tlut[9][256] ATTRIBUTE_ALIGN (32);
u16 tlutbanner[256] ATTRIBUTE_ALIGN (32);
u8 icondata[8][1024] ATTRIBUTE_ALIGN (32);
u16 icondataRGB[8][1024] ATTRIBUTE_ALIGN (32);
/*** This array holds the 16-bit banner data for the current save
	 Needs decoding by bannerloadRGB function before we can show it ***/
u16 bannerdata[CARD_BANNER_W*CARD_BANNER_H] ATTRIBUTE_ALIGN (32);
/*** This array holds the 8-bit banner data for the current save
	 Needs decoding by bannerloadCI function before we can show it ***/
u8 bannerdataCI[CARD_BANNER_W*CARD_BANNER_H] ATTRIBUTE_ALIGN (32);
int numicons;
int frametable[2*CARD_MAXICONS - 2];
int iconindex[2*CARD_MAXICONS - 2];
int lastframe;
int lasticon;
/*** This matrix will serve as our array of filenames for each file on the card
     We add 10 to filenamelen since we add on game company info***/
u8 filelist[1024][1024];
u8 currFolder[260];

/*** Card lib ***/
card_dir CardList[CARD_MAXFILES];	/*** Directory listing ***/
static card_dir CardDir;
static card_file CardFile;
static card_stat CardStatus;
static int cardcount = 0;
static u8 permission;
s32 memsize, sectsize;

card_direntry gci;

static int copy_save_data(void *destination, size_t destination_size,
	size_t *data_offset, size_t length, int file_size)
{
	if (!destination || !data_offset || length > destination_size || file_size < 0 ||
		*data_offset > (size_t)file_size ||
		length > (size_t)file_size - *data_offset)
		return 0;
	memcpy(destination, FileBuffer + MCDATAOFFSET + *data_offset, length);
	*data_offset += length;
	return 1;
}

//The following code is made by Ralf at GSCentral forums (gscentral.org)
//http://board.gscentral.org/retro-hacking/53093.htm#post188949
s32 FZEROGX_MakeSaveGameValid(s32 chn);
s32 PSO_MakeSaveGameValid(s32 chn);

/*---------------------------------------------------------------------------------
	This function is called if a card is physically removed
---------------------------------------------------------------------------------*/
static void card_removed(s32 chn,s32 result)
{
	if (chn == CARD_SLOTA){
		//printf("Card was removed from slot A");
	}
	else{
		//printf("Card was removed from slot B");
	}
	CARD_Unmount(chn);
}

/****************************************************************************
 * MountCard
 *
 * Mounts the memory card in the given slot.
 * CARD_Mount is called for a maximum of 30 tries. libogc2 debounces a newly
 * detected EXI device for roughly 300 ms, so ten video frames are not enough.
 * Returns the result of the last attempted CARD_Mount command.
 ***************************************************************************/
int MountCard(int cslot)
{
	s32 ret = -1;
	int tries = 0;
	int isMounted;

	// Mount the card, try several times as they are tricky
	while ( (tries < 30) && (ret < 0))
	{
		/*** Do not call EXI_ProbeReset() here. Current libogc2 requires an EXI
		device to remain present for roughly 300 ms before EXI_Attach() accepts
		it. Resetting the probe on every attempt restarts that timer forever and
		makes CARD_Mount() report CARD_ERROR_NOCARD. ***/
		CARD_Init (NULL, NULL);
		//Ensure we start in show all files mode
		CARD_SetCompany(NULL);
		CARD_SetGamecode(NULL);		

		/*** Mount the card ***/
		ret = CARD_Mount (cslot, SysArea, card_removed);
		if (ret >= 0) break;

		VIDEO_WaitVSync ();
		tries++;
	}
			/*** Make sure the card is really mounted ***/
	isMounted = CARD_ProbeEx(cslot, &memsize, &sectsize);
	if (memsize > 0 && sectsize > 0)//then we really mounted de card
	{
		return isMounted;
	}
	/*** If this point is reached, something went wrong ***/
	CARD_Unmount(cslot);
	return ret;
}

u16 FreeBlocks(s32 chn)
{
	s32 err;
	u16 freeblocks = 0;
	/*** Try to mount the card ***/
	err = MountCard(chn);
	if (err < 0)
	{
		//We want to allow browsing saves in sd even with no card
		if (err == CARD_ERROR_NOCARD)
		{
			if(chn) ShowAction("No card inserted in slot B!");
			else ShowAction("No card inserted in slot A!");
			return 0;
		}else
		{
			WaitCardError("CardMount", err);
			return 0;			/*** Unable to mount the card ***/
		}
	}else{
		err = CARD_GetFreeBlocks(chn, &freeblocks);
		if (err < 0)
		{
			CARD_Unmount(chn);
            //We want to allow browsing saves in sd even with no card
            if (err == CARD_ERROR_NOCARD)
            {
				if(chn) ShowAction("No card inserted in slot B!");
                else ShowAction("No card inserted in slot A!");
                return 0;
            }else
            {
				WaitCardError("FreeBlocks", err);
				return 0;
            }
		}
	}
	CARD_Unmount(chn);
	return freeblocks;
}

/****************************************************************************
* GCIMakeHeader
*
* Create a GCI compatible header from libOGC card_stat
****************************************************************************/
void GCIMakeHeader()
{
	/*** Clear out the cgi header ***/
	memset(&gci, 0xff, sizeof(card_direntry) );

	/*** Populate ***/
	memcpy(&gci.gamecode, &CardStatus.gamecode, 4);
	memcpy(&gci.company, &CardStatus.company, 2);
	gci.banner_fmt = CardStatus.banner_fmt;
	memcpy(&gci.filename, &CardStatus.filename, CARD_FILENAMELEN);
	gci.last_modified = CardStatus.time;
	gci.icon_addr = CardStatus.icon_addr;
	gci.icon_fmt = CardStatus.icon_fmt;
	gci.icon_speed = CardStatus.icon_speed;

	/*** Permission key has to be gotten separately. Make it 0 for normal privileges ***/
	gci.permission = permission;
	/*** Who cares about copy counter ***/
	gci.copy_times = 0;

	/*** Block index does not matter, it won't be restored at the same spot ***/
	gci.block = 32;

	gci.length = (CardStatus.len / 8192);
	gci.comment_addr = CardStatus.comment_addr;

	/*** Copy to head of buffer ***/
	memcpy(FileBuffer, &gci, sizeof(card_direntry));
}

/****************************************************************************
* ExtractGCIHeader
*
* Extract a GCI Header to libOGC card_stat
****************************************************************************/
void ExtractGCIHeader()
{
	/*** Clear out the status ***/
	memset(&CardStatus, 0, sizeof(card_stat));
	memcpy(&gci, FileBuffer, sizeof(card_direntry));

	memcpy(&CardStatus.gamecode, &gci.gamecode, 4);
	memcpy(&CardStatus.company, &gci.company, 2);
	CardStatus.banner_fmt = gci.banner_fmt;
	memcpy(&CardStatus.filename, &gci.filename, CARD_FILENAMELEN);
	CardStatus.time = gci.last_modified;
	CardStatus.icon_addr = gci.icon_addr;
	CardStatus.icon_fmt = gci.icon_fmt;
	CardStatus.icon_speed = gci.icon_speed;
	permission = gci.permission;
	CardStatus.len = gci.length * 8192;
	CardStatus.comment_addr = gci.comment_addr;
}

/****************************************************************************
* CardGetDirectory
*
* Returns number of files found on a card.
****************************************************************************/
int CardGetDirectory (int slot)
{
	int err;
	char company[4];
	char gamecode[6];

	//add null char
	company[2] = gamecode[4] = 0;

	/*** Clear the work area ***/
	memset (SysArea, 0, CARD_WORKAREA);

	/*** Initialise the Card system, show all ***/
	CARD_Init(NULL, NULL);

	/*** Try to mount the card ***/
	err = MountCard(slot);
	if (err < 0)
	{
		WaitCardError("CardMount", err);
		return 0;			/*** Unable to mount the card ***/
	}

	//Ensure we are in show all mode
	CARD_SetCompany(NULL);
	CARD_SetGamecode(NULL);

	/*** Retrieve the directory listing ***/
	cardcount = 0;
	err = CARD_FindFirst (slot, &CardDir, true); //true means we want to showall
	while (err >= 0 && cardcount < CARD_MAXFILES)
	{
		memcpy (&CardList[cardcount], &CardDir, sizeof (card_dir));
		memset (filelist[cardcount], 0, 1024);
		memcpy (company, &CardDir.company, 2);
		memcpy (gamecode, &CardDir.gamecode, 4);
		//This array will store what will show in left window
		snprintf((char *)filelist[cardcount], 1024, "%s-%s-%.32s", company,
			gamecode, CardDir.filename);
		cardcount++;
		err = CARD_FindNext (&CardDir);
	}
	if (err < 0 && err != CARD_ERROR_NOFILE)
		WaitCardError("CardDirectory", err);

	/*** Release as soon as possible ***/
	CARD_Unmount (slot);

	return cardcount;
}

int MCardIsValidSaveIndex(int id)
{
	return id >= 0 && id < cardcount && id < CARD_MAXFILES;
}

int MCardGetUsage(int slot, int *save_count, u16 *free_blocks)
{
	int err;

	if (!save_count || !free_blocks)
		return 0;
	*save_count = CardGetDirectory(slot);
	err = MountCard(slot);
	if (err < 0)
		return 0;
	err = CARD_GetFreeBlocks(slot, free_blocks);
	CARD_Unmount(slot);
	return err >= 0;
}

/****************************************************************************
* CardListFiles
****************************************************************************/
void CardListFiles ()
{
	int i;
	char company[4];
	char gamecode[6];
	char filename[CARD_FILENAMELEN + 1];

	//add null char
	company[2] = gamecode[4] = 0;

	for (i = 0; i < cardcount; i++)
	{
		memcpy (company, &CardList[i].company, 2);
		memcpy (gamecode, &CardList[i].gamecode, 4);
		memcpy(filename, CardList[i].filename, CARD_FILENAMELEN);
		filename[CARD_FILENAMELEN] = '\0';
		printf ("%s %s %s\n", company, gamecode, filename);
	}
}
/****************************************************************************
* CardReadFileHeader
*
* Retrieve a file header from the previously populated list.
* Reads in banner and icon data now
****************************************************************************/
//TODO: get icon and banner settings
int CardReadFileHeader (int slot, int id)
{
	int bytesdone = 0;
	int err;
	u32 SectorSize;
	char company[4];
	char gamecode[6];
	char filename[CARD_FILENAMELEN + 1];
	int filesize;
	int i;
	size_t data_offset;

	if (!MCardIsValidSaveIndex(id))
	{
		WaitPrompt("Bad id");
		return 0;			/*** Bad id ***/
	}

	/*** Clear the work buffers ***/
	memset (FileBuffer, 0, MAXFILEBUFFER);
	memset (CommentBuffer, 0, 64);
	memset (SysArea, 0, CARD_WORKAREA);
	//add null char
	company[2] = gamecode[4] = 0;
	memcpy(filename, CardList[id].filename, CARD_FILENAMELEN);
	filename[CARD_FILENAMELEN] = '\0';

	memcpy (company, &CardList[id].company, 2);
	memcpy (gamecode, &CardList[id].gamecode, 4);

	/*** Mount the card ***/
	err = MountCard(slot);
	if (err < 0)
	{
		WaitCardError("CardMount", err);
		return 0;			/*** Unable to mount the card ***/
	}

	/*** Retrieve sector size ***/
	err = CARD_GetSectorSize(slot, &SectorSize);
	if (err < 0 || SectorSize == 0 || SectorSize % 32 != 0) {
		CARD_Unmount(slot);
		WaitCardError("CardSectorSize", err);
		return 0;
	}

	/*** Initialise for this company & gamecode ***/
	CARD_SetCompany((const char*)company);
	CARD_SetGamecode((const char*)gamecode);

	/*** Open the file ***/
	err = CARD_Open(slot, filename, &CardFile);
	if (err < 0)
	{
		CARD_Unmount (slot);
		WaitCardError("CardOpen", err);
		return 0;
	}

#ifdef STATUSOGC
	/*** Get card status info ***/
	err = CARD_GetStatus (slot, CardFile.filenum, &CardStatus);
	if (err >= 0)
		err = CARD_GetAttributes(slot,CardFile.filenum, &permission);
	if (err < 0) {
		CARD_Close(&CardFile);
		CARD_Unmount(slot);
		WaitCardError("CardStatus", err);
		return 0;
	}

	GCIMakeHeader();
#else
	//get directory entry (same as gci header, but with all the data)
	memset(&gci,0,sizeof(card_direntry));
	err = CARD_GetStatusEx(slot,CardFile.filenum,&gci);
	if (err < 0) {
		CARD_Close(&CardFile);
		CARD_Unmount(slot);
		WaitCardError("CardStatus", err);
		return 0;
	}
	/*** Copy to head of buffer ***/
	memcpy(FileBuffer, &gci, sizeof(card_direntry));
#endif

	/*** Copy the file contents to the buffer ***/
	filesize = CardFile.len;
	if (filesize < 0 || filesize > MAXFILEBUFFER - MCDATAOFFSET ||
		filesize % SectorSize != 0) {
		CARD_Close(&CardFile);
		CARD_Unmount(slot);
		WaitPrompt("Save file is too large or has an invalid size.");
		return 0;
	}

	while (bytesdone < filesize)
	{
		err = CARD_Read(&CardFile, FileBuffer + MCDATAOFFSET + bytesdone,
			SectorSize, bytesdone);
		if (err < 0) {
			CARD_Close(&CardFile);
			CARD_Unmount(slot);
			WaitCardError("CardRead", err);
			return 0;
		}
		bytesdone += SectorSize;
	}

	/***
		Get the Banner/Icon Data from the memory card file.
		Very specific if/else setup to minimize data copies.
	***/
	data_offset = gci.icon_addr;

	/*** Get the Banner/Icon Data from the save file ***/
	if ((gci.banner_fmt&CARD_BANNER_MASK) == CARD_BANNER_RGB) {
		//RGB banners are 96*32*2 in size
		if (!copy_save_data(bannerdata, sizeof(bannerdata), &data_offset, 6144,
			filesize))
			goto invalid_metadata;
	}
	else if ((gci.banner_fmt&CARD_BANNER_MASK) == CARD_BANNER_CI) {
		if (!copy_save_data(bannerdataCI, sizeof(bannerdataCI), &data_offset, 3072,
			filesize) ||
			!copy_save_data(tlutbanner, sizeof(tlutbanner), &data_offset, 512,
				filesize))
			goto invalid_metadata;
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
					if (!copy_save_data(icondata[current_icon],
						sizeof(icondata[current_icon]), &data_offset, 1024, filesize))
						goto invalid_metadata;
					shared_pal = 1;
				}
				//CI with palette after the icon
				else if (SDCARD_GetIconFmt(gci.icon_fmt,current_icon) == 3)
				{
					if (!copy_save_data(icondata[current_icon],
						sizeof(icondata[current_icon]), &data_offset, 1024, filesize) ||
						!copy_save_data(tlut[current_icon], sizeof(tlut[current_icon]),
							&data_offset, 512, filesize))
						goto invalid_metadata;
				}
				//RGB 16 bit icon
				else if (SDCARD_GetIconFmt(gci.icon_fmt,current_icon) == 2)
				{
					if (!copy_save_data(icondataRGB[current_icon],
						sizeof(icondataRGB[current_icon]), &data_offset, 2048, filesize))
						goto invalid_metadata;
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
	if (shared_pal && !copy_save_data(tlut[8], sizeof(tlut[8]), &data_offset, 512,
		filesize))
		goto invalid_metadata;

	/*** Get the comment (two 32 byte strings) into buffer ***/
	data_offset = gci.comment_addr;
	if (!copy_save_data(CommentBuffer, sizeof(CommentBuffer), &data_offset,
		sizeof(CommentBuffer), filesize))
		goto invalid_metadata;

	/*** Close the file ***/
	CARD_Close (&CardFile);

	/*** Unmount the card ***/
	CARD_Unmount (slot);

	return filesize + MCDATAOFFSET;

invalid_metadata:
	CARD_Close(&CardFile);
	CARD_Unmount(slot);
	WaitPrompt("Save metadata points outside the file.");
	return 0;
}

/****************************************************************************
 * MCardGetSaveDetails
 *
 * Reads metadata for one selected save. The directory remains the authority
 * for list rendering; this only runs after the user opens details.
 ****************************************************************************/
int MCardGetSaveDetails(int slot, int id, card_direntry *entry, char comments[65])
{
	if (!entry || !comments || !CardReadFileHeader(slot, id))
		return 0;

	memcpy(entry, &gci, sizeof(*entry));
	memcpy(comments, CommentBuffer, 64);
	comments[64] = '\0';
	return 1;
}

/****************************************************************************
* CardReadFile
*
* Retrieve a file from the previously populated list.
* Place in filebuffer space, for collection by SMB write.
****************************************************************************/
int CardReadFile (int slot, int id)
{
	int bytesdone = 0;
	int err;
	u32 SectorSize;
	char company[4];
	char gamecode[6];
	char filename[CARD_FILENAMELEN + 1];
	int filesize;

	if (!MCardIsValidSaveIndex(id))
	{
		WaitPrompt("Bad id");
		return 0;			/*** Bad id ***/
	}

	/*** Clear the work buffers ***/
	memset (FileBuffer, 0, MAXFILEBUFFER);
	memset (SysArea, 0, CARD_WORKAREA);
	//add null char
	company[2] = gamecode[4] = 0;
	memcpy(filename, CardList[id].filename, CARD_FILENAMELEN);
	filename[CARD_FILENAMELEN] = '\0';

	memcpy (company, &CardList[id].company, 2);
	memcpy (gamecode, &CardList[id].gamecode, 4);

	/*** Mount the card ***/
	err = MountCard(slot);
	if (err < 0)
	{
		WaitCardError("CardMount", err);
		return 0;			/*** Unable to mount the card ***/
	}

	/*** Retrieve sector size ***/
	err = CARD_GetSectorSize(slot, &SectorSize);
	if (err < 0 || SectorSize == 0 || SectorSize % 32 != 0) {
		CARD_Unmount(slot);
		WaitCardError("CardSectorSize", err);
		return 0;
	}

	/*** Initialise for this company & gamecode ***/
	CARD_SetCompany(company);
	CARD_SetGamecode(gamecode);

	/*** Open the file ***/
	err = CARD_Open(slot, filename, &CardFile);
	if (err < 0)
	{
		CARD_Unmount (slot);
		WaitCardError("CardOpen", err);
		return 0;
	}

#ifdef STATUSOGC
	/*** Get card status info ***/
	err = CARD_GetStatus(slot, CardFile.filenum, &CardStatus);
	if (err >= 0)
		err = CARD_GetAttributes(slot, CardFile.filenum, &permission);
	if (err < 0) {
		CARD_Close(&CardFile);
		CARD_Unmount(slot);
		WaitCardError("CardStatus", err);
		return 0;
	}

	GCIMakeHeader();
#else
	//get directory entry (same as gci header, but with all the data)
	memset(&gci,0,sizeof(card_direntry));
	err = CARD_GetStatusEx(slot, CardFile.filenum, &gci);
	if (err < 0) {
		CARD_Close(&CardFile);
		CARD_Unmount(slot);
		WaitCardError("CardStatus", err);
		return 0;
	}
	/*** Copy to head of buffer ***/
	memcpy(FileBuffer, &gci, sizeof(card_direntry));
#endif

	/*** Copy the file contents to the buffer ***/
	filesize = CardFile.len;
	if (filesize < 0 || filesize > MAXFILEBUFFER - MCDATAOFFSET ||
		filesize % SectorSize != 0) {
		CARD_Close(&CardFile);
		CARD_Unmount(slot);
		WaitPrompt("Save file is too large or has an invalid size.");
		return 0;
	}

	while (bytesdone < filesize)
	{
		err = CARD_Read(&CardFile, FileBuffer + MCDATAOFFSET + bytesdone,
			SectorSize, bytesdone);
		if (err < 0) {
			CARD_Close(&CardFile);
			CARD_Unmount(slot);
			WaitCardError("CardRead", err);
			return 0;
		}
		bytesdone += SectorSize;
	}

	/*** Close the file ***/
	CARD_Close (&CardFile);

	/*** Unmount the card ***/
	CARD_Unmount (slot);

	return filesize + MCDATAOFFSET;
}

/****************************************************************************
* CardWriteFile
*
* Relies on *GOOD* data being placed in the FileBuffer prior to calling.
* See ReadSMBImage
****************************************************************************/
int CardWriteFile(int slot, int overwrite_allowed)
{
	char company[4];
	char gamecode[6];
	char filename[CARD_FILENAMELEN + 1];
	int err;
	u32 SectorSize;
	int offset;
	int written;
	int filelen;

	//add null char
	company[2] = gamecode[4] = 0;

	memset (SysArea, 0, CARD_WORKAREA);
	memset(filename, 0, sizeof(filename));
	ExtractGCIHeader();
	memcpy(company, &gci.company, 2);
	memcpy(gamecode, &gci.gamecode, 4);
	memcpy(filename, &gci.filename, CARD_FILENAMELEN);
	if (gci.length == 0 ||
		gci.length > (MAXFILEBUFFER - MCDATAOFFSET) / 8192) {
		WaitPrompt("Save file is too large or has an invalid header.");
		return 0;
	}
	filelen = (int)gci.length * 8192;
	if (OFFSET < 0 || OFFSET > MCDATAOFFSET || OFFSET % 32 != 0 ||
		filelen > MAXFILEBUFFER - MCDATAOFFSET - OFFSET) {
		WaitPrompt("Save file is too large or has an invalid header.");
		return 0;
	}

	/*** Mount the card ***/
	err = MountCard(slot);
	if (err < 0)
	{
		WaitCardError("CardMount", err);
		return 0;			/*** Unable to mount the card ***/
	}

	err = CARD_GetSectorSize(slot, &SectorSize);
	if (err < 0 || SectorSize == 0 || SectorSize % 32 != 0 ||
		filelen % SectorSize != 0) {
		CARD_Unmount(slot);
		if (err < 0)
			WaitCardError("CardSectorSize", err);
		else
			WaitPrompt("Save size is not aligned to the card sector size.");
		return MCARD_WRITE_FAILED;
	}

	/*** Initialise for this company & gamecode ***/
	CARD_SetCompany(company);
	CARD_SetGamecode(gamecode);
	
	/*** If this file exists, abort ***/
	err = CARD_FindFirst (slot, &CardDir, false);
	while (err != CARD_ERROR_NOFILE)
	{
		if (memcmp(CardDir.gamecode, gamecode, 4) == 0 &&
			memcmp(CardDir.company, company, 2) == 0 &&
			memcmp(CardDir.filename, filename, CARD_FILENAMELEN) == 0)
		{
			if (!overwrite_allowed) {
				CARD_Unmount(slot);
				return MCARD_WRITE_EXISTS;
			}
				err = CARD_Delete(slot, filename);
			if (err < 0) {
				WaitCardError("MCDel", err);
				CARD_Unmount(slot);
				return MCARD_WRITE_FAILED;
			}
			err = CARD_FindFirst(slot, &CardDir, false);
			continue;
		}

		err = CARD_FindNext (&CardDir);
	}

tryagain:
	/*** Initialise for this company & gamecode ***/
	//Again just in case, as this is very important for propper restoring
	CARD_SetCompany(company);
	CARD_SetGamecode(gamecode);
	
	/*** Now restore the file from backup ***/
	err = CARD_Create (slot, (char *) filename, filelen, &CardFile);
	if (err < 0)
	{
		if (err == CARD_ERROR_EXIST) {
			if (!overwrite_allowed) {
				CARD_Unmount(slot);
				return MCARD_WRITE_EXISTS;
			}
			err = CARD_Delete(slot, filename);
			if (err >= 0)
				goto tryagain;
			WaitCardError("MCDel", err);
		}
		CARD_Unmount (slot);
		WaitCardError("CardCreate", err);
		return 0;
	}

//Thanks to Ralf, validate F-zero and PSO savegames
	FZEROGX_MakeSaveGameValid(slot);
	PSO_MakeSaveGameValid(slot);

	/*** Now write the file data, in sector sized chunks ***/
	offset = 0;
	while (offset < filelen)
	{
		written = CARD_Write(&CardFile,
			FileBuffer + MCDATAOFFSET + offset + OFFSET, SectorSize, offset);
		if (written < 0) {
			OFFSET = 0;
			CARD_Close(&CardFile);
			CARD_Unmount(slot);
			WaitCardError("CardWrite", written);
			return 0;
		}

		offset += SectorSize;
	}

	OFFSET = 0;

#ifdef STATUSOGC
	/*** Finally, update the status ***/
	err = CARD_SetStatus(slot, CardFile.filenum, &CardStatus);
	//For some reason this sets the file to Move->allowed, Copy->not allowed, Public file instead of the actual permission value
	if (err >= 0)
		err = CARD_SetAttributes(slot, CardFile.filenum, &permission);
	if (err < 0) {
		CARD_Close(&CardFile);
		CARD_Unmount(slot);
		WaitCardError("CardSetStatus", err);
		return MCARD_WRITE_FAILED;
	}
#else
	err = CARD_SetStatusEx(slot, CardFile.filenum, &gci);
	if (err < 0) {
		CARD_Close(&CardFile);
		CARD_Unmount(slot);
		WaitCardError("CardSetStatus", err);
		return MCARD_WRITE_FAILED;
	}
#endif

	CARD_Close (&CardFile);
	CARD_Unmount (slot);

	return 1;
}

/****************************************************************************
 * MCardVerifyLastWrite
 *
 * Reopens the save described by gci and compares its complete payload against
 * the payload most recently written from FileBuffer. Move uses this before it
 * is allowed to delete its source.
 ****************************************************************************/
int MCardVerifyLastWrite(int slot)
{
	char company[3];
	char gamecode[5];
	char filename[CARD_FILENAMELEN + 1];
	u32 expected_crc;
	u32 actual_crc;
	u32 sector_size;
	u32 offset;
	u32 filelen;
	int err;

	if (gci.length == 0 ||
		gci.length > (MAXFILEBUFFER - MCDATAOFFSET) / 8192)
		return 0;
	filelen = gci.length * 8192;
	expected_crc = crc32(0L, Z_NULL, 0);
	expected_crc = crc32(expected_crc, FileBuffer + MCDATAOFFSET, filelen);
	memcpy(company, gci.company, 2);
	company[2] = '\0';
	memcpy(gamecode, gci.gamecode, 4);
	gamecode[4] = '\0';
	memcpy(filename, gci.filename, CARD_FILENAMELEN);
	filename[CARD_FILENAMELEN] = '\0';

	err = MountCard(slot);
	if (err < 0) {
		WaitCardError("CardVerify Mount", err);
		return 0;
	}
	err = CARD_GetSectorSize(slot, &sector_size);
	if (err < 0 || sector_size == 0 || sector_size % 32 != 0 ||
		filelen % sector_size != 0) {
		CARD_Unmount(slot);
		if (err < 0)
			WaitCardError("CardVerify SectorSize", err);
		else
			WaitPrompt("Save size is not aligned to the card sector size.");
		return 0;
	}

	CARD_SetCompany(company);
	CARD_SetGamecode(gamecode);
	err = CARD_Open(slot, filename, &CardFile);
	if (err < 0) {
		CARD_Unmount(slot);
		WaitCardError("CardVerify Open", err);
		return 0;
	}
	if (CardFile.len != (int)filelen) {
		CARD_Close(&CardFile);
		CARD_Unmount(slot);
		WaitPrompt("Destination save length does not match source.");
		return 0;
	}

	actual_crc = crc32(0L, Z_NULL, 0);
	for (offset = 0; offset < filelen; offset += sector_size) {
		err = CARD_Read(&CardFile, FileBuffer + MCDATAOFFSET, sector_size, offset);
		if (err < 0) {
			CARD_Close(&CardFile);
			CARD_Unmount(slot);
			WaitCardError("CardVerify Read", err);
			return 0;
		}
		actual_crc = crc32(actual_crc, FileBuffer + MCDATAOFFSET, sector_size);
	}
	CARD_Close(&CardFile);
	CARD_Unmount(slot);
	if (actual_crc != expected_crc) {
		WaitPrompt("Destination verification failed. Source was not deleted.");
		return 0;
	}
	return 1;
}

void WaitCardError(char *src, int error)
{
	char msg[1024], err[256];

	//Error message possibilites
	switch (error)
	{
	case CARD_ERROR_BUSY:
		sprintf(err, "Busy");
		break;
	case CARD_ERROR_WRONGDEVICE:
		sprintf(err, "Wrong device in slot");
		break;
	case CARD_ERROR_NOCARD:
		sprintf(err, "No Card");
		break;
	case CARD_ERROR_NOFILE:
		sprintf(err, "No File");
		break;
	case CARD_ERROR_IOERROR:
		sprintf(err, "Internal EXI I/O error");
		break;
	case CARD_ERROR_BROKEN:
		sprintf(err, "File/Dir entry broken");
		break;
	case CARD_ERROR_EXIST:
		sprintf(err, "File already exists");
		break;
	case CARD_ERROR_NOENT:
		sprintf(err, "No empty blocks");
		break;
	case CARD_ERROR_INSSPACE:
		sprintf(err, "Not enough space");
		break;
	case CARD_ERROR_NOPERM:
		sprintf(err, "No permission");
		break;
	case CARD_ERROR_LIMIT:
		sprintf(err, "Card size limit reached");
		break;
	case CARD_ERROR_NAMETOOLONG:
		sprintf(err, "Filename too long");
		break;
	case CARD_ERROR_ENCODING:
		sprintf(err, "Font encoding mismatch");
		break;
	case CARD_ERROR_CANCELED:
		sprintf(err, "Card operation cancelled");
		break;
	case CARD_ERROR_FATAL_ERROR:
		sprintf(err, "Fatal error");
		break;
	case CARD_ERROR_READY:
		sprintf(err, "Ready");
		break;
	default:
		sprintf(err, "Unknown");
		break;
	}
	//Here we build the full error message
	sprintf(msg, "MemCard Error: %s - %d %s", src,error, err);

	WaitPrompt(msg);
}

/****************************************************************************
 * MCardDeleteFile
 *
 * Deletes one entry from the directory most recently loaded by
 * CardGetDirectory(). Confirmation belongs to the task UI so this routine can
 * also be used safely by multi-selection and move workflows.
 ****************************************************************************/
int MCardDeleteFile(int slot, int id)
{
	int err;
	char company[3];
	char gamecode[5];
	char filename[CARD_FILENAMELEN + 1];

	if (!MCardIsValidSaveIndex(id))
		return 0;
	memcpy(company, CardList[id].company, 2);
	company[2] = '\0';
	memcpy(gamecode, CardList[id].gamecode, 4);
	gamecode[4] = '\0';
	memcpy(filename, CardList[id].filename, CARD_FILENAMELEN);
	filename[CARD_FILENAMELEN] = '\0';

	err = MountCard(slot);
	if (err < 0) {
		WaitCardError("MCDel Mount", err);
		return 0;
	}

	CARD_SetCompany(company);
	CARD_SetGamecode(gamecode);
	err = CARD_Delete(slot, filename);
	CARD_Unmount(slot);

	if (err < 0) {
		WaitCardError("MCDel", err);
		return 0;
	}
	return 1;
}

/****************************************************************************
 * MCardFormat
 *
 * Performs only the validated format operation. Callers must show the two
 * explicit confirmations before entering this routine.
 ****************************************************************************/
int MCardFormat(int slot)
{
	int err;

	err = MountCard(slot);
	if (err < 0) {
		WaitCardError("MCFormat Mount", err);
		return 0;
	}

	err = CARD_Format(slot);
	CARD_Unmount(slot);
	if (err < 0) {
		WaitCardError("MCFormat", err);
		return 0;
	}

	usleep(1000 * 1000);
	err = MountCard(slot);
	if (err < 0) {
		WaitCardError("MCFormat Verify", err);
		return 0;
	}
	CARD_Unmount(slot);
	return 1;
}

//The following code is made by Ralf at GSCentral forums (gscentral.org)
//http://board.gscentral.org/retro-hacking/53093.htm#post188949

// u8 FileBuffer[MAXFILEBUFFER]: .gci file buffer
// MCDATAOFFSET: .gci header size (0x40 bytes)

/*************************************************************/
/* FZEROGX_MakeSaveGameValid                                 */
/* (use just before writing a F-Zero GX system .gci file)    */
/*                                                           */
/* chn: Destination memory card port                         */
/* ret: Error code                                           */
/*************************************************************/

s32 FZEROGX_MakeSaveGameValid(s32 chn)
{
	s32 ret;
	u32 i,j;
	u32 serial1,serial2;
	u16 chksum = 0xFFFF;

	if(strcasecmp((const char *)&FileBuffer[0x08],"f_zero.dat")!=0) return CARD_ERROR_READY;		// check for F-Zero GX system file
	if((ret=CARD_GetSerialNo(chn,&serial1,&serial2))<0) return ret;			// get encrypted destination memory card serial numbers

	*(u16*)&FileBuffer[0x2066+MCDATAOFFSET] = serial1 >> 16;			// set new serial numbers
	*(u16*)&FileBuffer[0x7580+MCDATAOFFSET] = serial2 >> 16;
	*(u16*)&FileBuffer[0x2060+MCDATAOFFSET] = serial1 & 0xFFFF;
	*(u16*)&FileBuffer[0x2200+MCDATAOFFSET] = serial2 & 0xFFFF;

	for(i=0x02+MCDATAOFFSET;i<0x8000+MCDATAOFFSET;i++) {				// calc 16-bit checksum
		chksum ^= (FileBuffer[i]&0xFF);

		for(j=8;j>0;j--) {
			if (chksum&1) chksum = (chksum>>1)^0x8408;
			else chksum >>= 1;
		}
	}

	*(u16*)&FileBuffer[0x00+MCDATAOFFSET] = ~chksum;				// set new checksum

	return ret;
}

/***********************************************************/
/* PSO_MakeSaveGameValid	                           */
/* (use just before writing a PSO system .gci file)        */
/*                                                         */
/* chn: Destination memory card port                       */
/* ret: Error code                                         */
/***********************************************************/

s32 PSO_MakeSaveGameValid(s32 chn)
{
	s32 ret;
	u32 i,j;
	u32 chksum;
	u32 crc32LUT[256];
	u32 serial1,serial2;
	u32 pso3offset;

	if(strcasecmp((const char *)&FileBuffer[0x08],"PSO_SYSTEM")==0) {				// check for PSO1&2 system file
		pso3offset = 0x00;
		goto exit;
	}

	if(strcasecmp((const char *)&FileBuffer[0x08],"PSO3_SYSTEM")==0) {				// check for PSO3 system file
		pso3offset = 0x10;							// PSO3 data block size adjustment
		goto exit;
	}

	return CARD_ERROR_READY;							// nothing to do
exit:
	if((ret=CARD_GetSerialNo(chn,&serial1,&serial2))<0) return ret;			// get encrypted destination memory card serial numbers

	*(u32*)&FileBuffer[0x2158+MCDATAOFFSET] = serial1;				// set new serial numbers
	*(u32*)&FileBuffer[0x215C+MCDATAOFFSET] = serial2;

	for(i=0;i<256;i++) {								// generate crc32 LUT
	        chksum = i;

        	for(j=8;j>0;j--) {
			if (chksum&1) chksum = (chksum>>1)^0xEDB88320;
             		else chksum >>= 1;
		}

        	crc32LUT[i] = chksum;
	}

	chksum = 0xDEBB20E3;								// PSO initial crc32 value

	for (i=0x204C+MCDATAOFFSET;i<0x2164+pso3offset+MCDATAOFFSET;i++) {		// calc 32-bit checksum
		chksum = ((chksum>>8)&0xFFFFFF)^crc32LUT[(chksum^FileBuffer[i])&0xFF];
	}

	*(u32*)&FileBuffer[0x2048+MCDATAOFFSET] = chksum^0xFFFFFFFF;			// set new checksum

	return ret;
}
