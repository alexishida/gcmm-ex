/****************************************************************************
 * bitmap.c
 *
 * libogc example to display a Windows bitmap.
 *
 * NOTE: Windows BMP must comply to the following:
 *	24-bit uncompressed (99.99% of Windows Editor output)
 *	Be less than or equal to 640 x 480(MPAL/NTSC) 640x528(PAL)
 *
 * This example decodes a BMP file onto the linear framebuffer.
 ****************************************************************************/
/**
 * @file bitmap.c
 * @brief Linear-framebuffer BMP, banner, and icon rendering.
 *
 * BMP inputs must be 24-bit, uncompressed, and fit active video mode. Save
 * previews are already decoded by bannerload.c and use the same framebuffer.
 */
#include <gccore.h>
#include <ogcsys.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bitmap.h"
#include "freetype.h"
#include "gci.h"
#include "sdsupp.h"

#include "ui_bg_bmp.h"
extern u8 selector_flag;

extern card_direntry gci;
//*extern GXRModeObj *vmode; /*** Graphics Mode Object ***/
//extern u32 *xfb[2] = { NULL, NULL }; /*** Framebuffers ***/
//extern int whichfb = 0; /*** Frame buffer toggle ***/

/****************************************************************************
 * Supporting Functions
 *
 * Intel to PowerPC
 *	FLIP32		Convert little to big endian ints.
 *	FLIP16		Convert little to big endian shorts.
 *	CvtRGB		Convert RGB pixels to Y1CbY2Cr.
 ****************************************************************************/
static u32 FLIP32(u32 value) {
    u32 b;

    b = (value & 0xff) << 24;
    b |= (value & 0xff00) << 8;
    b |= (value & 0xff0000) >> 8;
    b |= (value & 0xff000000) >> 24;

    return b;
}

static u16 FLIP16(u16 value) {
    return ((value & 0x00ff) << 8) | ((value & 0xff00) >> 8);
}

/****************************************************************************
 * CvtRGB
 *
 * This function simply returns two RGB pixels as one Y1CbY2Cr.
 *****************************************************************************/
u32 CvtRGB(u8 r1, u8 g1, u8 b1, u8 r2, u8 g2, u8 b2) {
    int y1, cb1, cr1, y2, cb2, cr2, cb, cr;

    y1 = (299 * r1 + 587 * g1 + 114 * b1) / 1000;
    cb1 = (-16874 * r1 - 33126 * g1 + 50000 * b1 + 12800000) / 100000;
    cr1 = (50000 * r1 - 41869 * g1 - 8131 * b1 + 12800000) / 100000;

    y2 = (299 * r2 + 587 * g2 + 114 * b2) / 1000;
    cb2 = (-16874 * r2 - 33126 * g2 + 50000 * b2 + 12800000) / 100000;
    cr2 = (50000 * r2 - 41869 * g2 - 8131 * b2 + 12800000) / 100000;

    cb = (cb1 + cb2) >> 1;
    cr = (cr1 + cr2) >> 1;

    return (y1 << 24) | (cb << 16)| (y2 << 8)| cr;
}

/****************************************************************************
 * Windows 24-Bit Bitmap Header
 *
 * These are only the values which are important for GC.
 ****************************************************************************/
typedef struct {
    u16 bfMagic; /*** Always BM ***/
    u32 bfSize; /*** Size of file ***/
    u32 bfReserved; /*** Always 0 ***/
    u32 bfOffset; /*** Offset to pixel data ***/
    u32 biImageHdrSize; /*** Size of Image info header ***/
    u32 biWidth; /*** Width of bitmap ***/
    u32 biHeight; /*** Height of bitmap ***/
    u16 biPlanes; /*** Number of planes ***/
    u16 biBitsPerPixel; /*** Bits per pixel ***/
    u32 biCompression; /*** Is this compressed? ***/
} __attribute__ ((__packed__)) WINBITMAP;

/****************************************************************************
 * ShowBMP
 *
 * This function does some basic checks on the bitmap, to make sure it's the
 * format expected.
 *
 * It will display the bitmap data centred on the NEXT framebuffer.
 ****************************************************************************/
u32 ShowBMP(u8 * bmpfile) {
    WINBITMAP *bitmap;
    u32 fbwidth, width, height;
    u8 *bgr;
    u32 fboffset;
    u32 rows, cols;

    bitmap = (WINBITMAP *) bmpfile;

    /*** First check, is this a windows bitmap ? ***/
    if (memcmp (&bitmap->bfMagic, "BM", 2))
        return 0;

    /*** Only support single plane ***/
    if (FLIP16 (bitmap->biPlanes) != 1)
        return 0;

    /*** Only support non-compressed ***/
    if (FLIP32 (bitmap->biCompression) != 0)
        return 0;

    /*** 24 bits per pixel ***/
    if (FLIP16 (bitmap->biBitsPerPixel) != 24)
        return 0;

    width = FLIP32 (bitmap->biWidth);
    height = FLIP32 (bitmap->biHeight);

    /*** Will the bitmap fit in the framebuffer? ***/
    if ((width > 640) || (height > vmode->xfbHeight))
        return 0;

    /*** Looks like decoding this is ok ***/
    bgr = (u8 *) bmpfile + FLIP32 (bitmap->bfOffset);

    fbwidth = width;
    if (fbwidth & 1)
        fbwidth++;

    /***
     * Windows BMP files are stored left-to-right, bottom-to-top
     ***/

    /*** Centre width ***/
    fboffset = ((640 - fbwidth) >> 1) >> 1;

    /*** Centre height ***/
    fboffset += (((vmode->xfbHeight - height) >> 1) * 320);
    fboffset += (height * 320);

    /*** Move to NEXT framebuffer ***/
    whichfb ^= 1;
	
	//Move background in the "device selection" screen to center the left pane with the GC memory card logo
	if (selector_flag)
	{
		DrawBoxFilled(0,0,vmode->fbWidth-1,vmode->xfbHeight-1, COL_BG2);	
		for (rows = 0; rows < height; rows++) {
			for (cols = 0; cols < (fbwidth >> 1); cols++) {
				if (rows < 64 || rows > 398 || cols < (31>>1) || cols > (341>>1))
				{
					//xfb[whichfb][fboffset + cols] =CvtRGB (255,255,255,255,255,255);
				}
				else
				{
					xfb[whichfb][fboffset-(320*20) + (66)+cols] =CvtRGB (bgr[2], bgr[1], bgr[0],
							bgr[5], bgr[4], bgr[3]);		
				}

				bgr += 6;
			}

			fboffset -= 320; /*** Go up one row ***/

		} /*** Outer row loop ***/
	}
	else
	{
		for (rows = 0; rows < height; rows++) {
			for (cols = 0; cols < (fbwidth >> 1); cols++) {
				xfb[whichfb][fboffset + cols] =CvtRGB (bgr[2], bgr[1], bgr[0],
						bgr[5], bgr[4], bgr[3]);
				bgr += 6;
			}

			fboffset -= 320; /*** Go up one row ***/

		} /*** Outer row loop ***/
	}

    /*** Setup the video to display this picture ***/
    /*VIDEO_SetNextFramebuffer (xfb[whichfb]);
    VIDEO_Flush ();
    VIDEO_WaitVSync ();*/

    return 1;
}

/* Draw an even-width 24-bit BMP onto the current framebuffer without
 * changing framebuffers. Used for small UI artwork over the shared backdrop. */
u32 DrawBMPAt(u8 *bmpfile, int x, int y)
{
	WINBITMAP *bitmap = (WINBITMAP *)bmpfile;
	u32 width;
	u32 height;
	u32 row_stride;
	u32 rows;
	u32 cols;
	u8 *bgr;
	u32 fboffset;

	if (!bmpfile || memcmp(&bitmap->bfMagic, "BM", 2) ||
		FLIP16(bitmap->biPlanes) != 1 ||
		FLIP16(bitmap->biBitsPerPixel) != 24 ||
		FLIP32(bitmap->biCompression) != 0)
		return 0;
	width = FLIP32(bitmap->biWidth);
	height = FLIP32(bitmap->biHeight);
	if ((width & 1) || x < 0 || y < 0 || width > 640 ||
		height > vmode->xfbHeight || x > 640 - (int)width ||
		y > (int)vmode->xfbHeight - (int)height)
		return 0;

	row_stride = (width * 3 + 3) & ~3U;
	bgr = bmpfile + FLIP32(bitmap->bfOffset);
	fboffset = (u32)y * 320 + (u32)x / 2 + (height - 1) * 320;
	for (rows = 0; rows < height; rows++) {
		u8 *row = bgr + rows * row_stride;
		for (cols = 0; cols < width / 2; cols++) {
			xfb[whichfb][fboffset + cols] = CvtRGB(row[2], row[1], row[0],
				row[5], row[4], row[3]);
			row += 6;
		}
		fboffset -= 320;
	}
	return 1;
}

/***
	ShowBanner is ShowBMP but modified to be simpler. We don't need to
	check for a header since we generated the bmp BGR values directly for each save game.
***/
void ShowBanner(u8 *banner) {
    u32 fbwidth, width, height;
    u8 *bgr;
    u32 fboffset;
    u32 rows, cols;

    width = CARD_BANNER_W;
    height = CARD_BANNER_H;

    bgr = banner;

    fbwidth = width;
    if (fbwidth & 1)
        fbwidth++;

    /*** Position banner on screen ***/
    fboffset = ((640 - fbwidth) >> 1) >> 1;
    fboffset += (((vmode->xfbHeight - height) >> 1) * 320);
	fboffset -= (height * 320 * 2);
	fboffset += (height * 10);
	fboffset += width/2+3; //at this point the banner is perfectly aligned with right window
	fboffset += ((height *320)>>1)+3;

	//fboffset += 24; //force banner to display to the right of icon
	fboffset += 43; //force banner to display to the right of icon

    for (rows = 0; rows < height; rows++) {
        for (cols = 0; cols < (fbwidth >> 1); cols++) {
            xfb[whichfb][fboffset + cols] =CvtRGB (bgr[2], bgr[1], bgr[0],
                    bgr[5], bgr[4], bgr[3]);
            bgr += 6;
        }
		/*
			Go down a row here, unlike what was used in ShowBMP.
			bannerload actually stores the image normally instead of
			adhering to bottom-to-top format.
		*/
        fboffset += 320;

    }
}

void DrawBannerAt(const u8 *banner, int x, int y, int scale)
{
	int row;
	int column;
	int output_width;
	int output_height;
	int framebuffer_words;

	if (!banner || scale < 1)
		return;
	output_width = CARD_BANNER_W * scale;
	output_height = CARD_BANNER_H * scale;
	if (x < 0 || y < 0 || (x & 1) || x + output_width > (int)vmode->fbWidth ||
		y + output_height > (int)vmode->xfbHeight)
		return;
	framebuffer_words = vmode->fbWidth / 2;
	for (row = 0; row < output_height; row++) {
		const u8 *source_row = banner + (row / scale) * CARD_BANNER_W * 3;
		u32 *destination = xfb[whichfb] + (y + row) * framebuffer_words + x / 2;
		for (column = 0; column < output_width; column += 2) {
			const u8 *first = source_row + (column / scale) * 3;
			const u8 *second = source_row + ((column + 1) / scale) * 3;
			*destination++ = CvtRGB(first[2], first[1], first[0],
				second[2], second[1], second[0]);
		}
	}
}

void ShowIcon(u8 *icon) {
    u32 fbwidth, width, height;
    u8 *bgr;
    u32 fboffset;
    u32 rows, cols;

    width = CARD_BANNER_W;
    height = CARD_ICON_H;

    bgr = icon;

    fbwidth = width;
    if (fbwidth & 1)
        fbwidth++;

    /*** Position icon on screen ***/
    fboffset = ((640 - fbwidth) >> 1) >> 1;
	fbwidth = CARD_ICON_W;
    fboffset += (((vmode->xfbHeight - height) >> 1) * 320);
	fboffset -= (height * 320 * 2);
	fboffset += (height * 10);
	fboffset += width/2+3;
	fboffset += ((height *320)>>1)+3;

	if (SDCARD_GetBannerFmt(gci.banner_fmt) == 1 || SDCARD_GetBannerFmt(gci.banner_fmt) == 2 )
	{
		fboffset += 20;
	}else
	{
		fboffset += 47; //center icon
	}


    for (rows = 0; rows < height; rows++) {
        for (cols = 0; cols < (fbwidth >> 1); cols++) {
            xfb[whichfb][fboffset + cols] =CvtRGB (bgr[2], bgr[1], bgr[0],
                    bgr[5], bgr[4], bgr[3]);
            bgr += 6;
        }
        fboffset += 320;

    }
}

void ClearScreen() {
	ShowBMP((u8*) ui_bg_bmp);

}
