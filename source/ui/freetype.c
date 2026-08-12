/****************************************************************************
* libFreeType
*
* Needed to show the user what's the hell's going on !!
* These functions are generic, and do not contain any Memory Card specific
* routines.
****************************************************************************/
/**
 * @file freetype.c
 * @brief FreeType text renderer and low-level framebuffer drawing helpers.
 *
 * Rasterizes bundled TrueType font directly into active linear framebuffer.
 * Independent of save and storage workflows.
 */
#include <gccore.h>
#include <ogcsys.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ogc/lwp_watchdog.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include "bannerload.h"
#include "freetype.h"
#include "gci.h"
#include "mcard.h"
#include "sdsupp.h"
#include "bitmap.h"
#include "raw.h"

#ifdef HW_RVL
#include <wiiuse/wpad.h>
#endif

/*** The actual TrueType Font ***/
#include "font_ttf.h"

#define MCDATAOFFSET 64
#define FONT_SIZE 16 //pixels

/*** Globals ***/
FT_Library ftlibrary;
FT_Face face;
FT_GlyphSlot slot;
FT_UInt glyph_index;
static u8 font_y, font_cb, font_cr;

/*** From video subsystem ***/
extern int screenheight;
extern u32 *xfb[2];
extern int whichfb;
extern GXRModeObj *vmode;
extern int vmode_60hz;

#ifdef HW_RVL
// POWER BUTTON
bool power=false;

void PowerOff()
{
	//LWP_MutexDestroy(m_audioMutex);
	SYS_ResetSystem(SYS_POWEROFF, 0, 0);
	exit(0);
}

static void set_power_var()
{
	power = true;
}
static void set_wiimote_power_var(s32 chan)
{
	power = true;
}
void initialise_power()
{
	SYS_SetPowerCallback(set_power_var);
	WPAD_SetPowerButtonCallback(set_wiimote_power_var);
}
// END POWER BUTTON
#endif

/****************************************************************************
* FT_Init
*
* Initialise freetype library
****************************************************************************/
int FT_Init ()
{

	int err;

	err = FT_Init_FreeType (&ftlibrary);
	if (err)
		return 1;

	err =
	    FT_New_Memory_Face (ftlibrary, (FT_Byte *) font_ttf, font_ttf_size, 0, &face);
	if (err)
		return 1;

	setfontsize(FONT_SIZE);
	setfontcolour(COL_FONT);

	slot = face->glyph;

	return 0;

}

/****************************************************************************
* setfontsize
****************************************************************************/
void setfontsize (int pixelsize)
{
	int err;

	err = FT_Set_Pixel_Sizes (face, 0, pixelsize);

	if (err)
		printf ("Error setting pixel sizes!");
}

static u8 blend_font_component(u8 background, u8 foreground, u8 coverage)
{
	return (u8)((background * (255 - coverage) + foreground * coverage + 127) / 255);
}

static void DrawCharacter (FT_Bitmap * bmp, FT_Int x, FT_Int y)
{
	FT_Int i, j, p, q;
	FT_Int x_max = x + bmp->width;
	FT_Int y_max = y + bmp->rows;
	int spos;
	unsigned int pixel;
	int c;

	for (i = x, p = 0; i < x_max; i++, p++)
	{
		for (j = y, q = 0; j < y_max; j++, q++)
		{
			if (i < 0 || j < 0 || i >= 640 || j >= screenheight)
				continue;

			/*** Convert pixel position to GC int sizes ***/
			spos = (j * 320) + (i >> 1);

			pixel = xfb[whichfb][spos];
			c = bmp->buffer[q * bmp->pitch + p];

			/* FreeType gives an 8-bit coverage value. Preserve it instead of
			 * discarding half of each glyph with a hard threshold: small text
			 * stays fuller and diagonal strokes no longer look jagged. */
			if (c) {
				u8 y_component;
				u8 cb_component;
				u8 cr_component;

				if (i & 1) {
					y_component = blend_font_component((pixel >> 8) & 0xff, font_y, c);
					pixel = (pixel & 0xffff00ff) | ((u32)y_component << 8);
				} else {
					y_component = blend_font_component((pixel >> 24) & 0xff, font_y, c);
					pixel = (pixel & 0x00ffffff) | ((u32)y_component << 24);
				}
				cb_component = blend_font_component((pixel >> 16) & 0xff, font_cb, c);
				cr_component = blend_font_component(pixel & 0xff, font_cr, c);
				pixel = (pixel & 0xff00ff00) | ((u32)cb_component << 16) | cr_component;
				xfb[whichfb][spos] = pixel;
			}
		}
	}
}

/**
 * DrawText
 *
 * Place the font bitmap on the screen
 */
void DrawText (int x, int y, char *text)
{
	int px, n;
	int i;
	int err;
	int value, count;

	n = strlen (text);
	if (n == 0)
		return;

	/*** x == -1, auto centre ***/
	if (x == -1)
	{
		value = 0;
		px = 0;
	}
	else
	{
		value = 1;
		px = x;
	}

	for (count = value; count < 2; count++)
	{
		/*** Draw the string ***/
		for (i = 0; i < n; i++)
		{
			err = FT_Load_Char (face, text[i], FT_LOAD_RENDER);

			if (err)
			{
				printf ("Error %c %d\n", text[i], err);
				continue;				/*** Skip unprintable characters ***/
			}

			if (count)
				DrawCharacter (&slot->bitmap, px + slot->bitmap_left,
				               y - slot->bitmap_top);

			px += slot->advance.x >> 6;
		}

		px = (640 - px) >> 1;

	}
}

/**
 * getcolour
 *
 * Simply converts RGB to Y1CbY2Cr format
 *
 * I got this from a pastebin, so thanks to whoever originally wrote it!
 */

unsigned int getcolour(u8 r1, u8 g1, u8 b1)
{
	int y1, cb1, cr1, y2, cb2, cr2, cb, cr;
	u8 r2, g2, b2;

	r2 = r1;
	g2 = g1;
	b2 = b1;

	y1 = (299 * r1 + 587 * g1 + 114 * b1) / 1000;
	cb1 = (-16874 * r1 - 33126 * g1 + 50000 * b1 + 12800000) / 100000;
	cr1 = (50000 * r1 - 41869 * g1 - 8131 * b1 + 12800000) / 100000;

	y2 = (299 * r2 + 587 * g2 + 114 * b2) / 1000;
	cb2 = (-16874 * r2 - 33126 * g2 + 50000 * b2 + 12800000) / 100000;
	cr2 = (50000 * r2 - 41869 * g2 - 8131 * b2 + 12800000) / 100000;

	cb = (cb1 + cb2) >> 1;
	cr = (cr1 + cr2) >> 1;

	return ((y1 << 24) | (cb << 16) | (y2 << 8) | cr);
}

/**
 * setfontcolour
 *
 * Uses RGB triple values.
 */
void setfontcolour (u8 r, u8 g, u8 b)
{
	u32 fontcolour;

	fontcolour = getcolour(r, g, b);
	font_y = fontcolour >> 24;
	font_cb = (fontcolour >> 16) & 0xff;
	font_cr = fontcolour & 0xff;
}

/****************************************************************************
* ClearScreen
****************************************************************************/
/*void ClearScreen (){
  whichfb ^= 1;
  VIDEO_ClearFrameBuffer (vmode, xfb[whichfb], COL_BLACK);
}
*/
/****************************************************************************
* ShowScreen
****************************************************************************/
void ShowScreen ()
{
	VIDEO_SetNextFramebuffer (xfb[whichfb]);
	VIDEO_Flush ();
	VIDEO_WaitVSync ();
}

/**
 * Show an action in progress
 */
void ShowAction (char *msg)
{
	//int ypos = screenheight >> 1;
	//ypos += 16;

	//ClearScreen ();
	//DrawText (-1, ypos, msg);
	writeStatusBar(msg,"");
	ShowScreen ();
}

void WaitRelease()
{
	//Wait for user to release the button
	while (PAD_ButtonsHeld (0)
#ifdef HW_RVL
	        | WPAD_ButtonsHeld(0)
#endif
	      )
		VIDEO_WaitVSync ();
}

/**
 * Wait for user to press A
 */
void WaitButtonA ()
{
#ifdef HW_DOL
	while (PAD_ButtonsDown (0) & PAD_BUTTON_A );
	while (!(PAD_ButtonsDown (0) & PAD_BUTTON_A));
#else
	while (1)
	{
		if (PAD_ButtonsDown (0) & PAD_BUTTON_A )
			break;
		if (WPAD_ButtonsDown (0) & WPAD_BUTTON_A )
			break;
		if (power)
		{
			PowerOff();
		}
	}
	while (1)
	{
		if (!(PAD_ButtonsDown (0) & PAD_BUTTON_A))
			break;
		if (!(WPAD_ButtonsDown (0) & WPAD_BUTTON_A))
			break;
		if (power)
		{
			PowerOff();
		}
	}
#endif
	WaitRelease();
}

/**
 * Show a prompt
 */
void WaitPrompt (char *msg)
{
	//int ypos = (screenheight) >> 1;
	//ClearScreen ();
	//DrawText (-1, ypos, msg);
	//ypos += 20;
	//DrawText (-1, ypos, "Press A to continue");
	writeStatusBar(msg,"Press A to continue");
	ShowScreen ();
	WaitButtonA ();
	//clear the text
	writeStatusBar("","");

}

void writeStatusBar( char *line1, char *line2)
{
	DrawBoxFilled(0, 404, vmode->fbWidth-1, vmode->xfbHeight-1, COL_BG2);
	//setfontcolour(84,174,211);
	setfontcolour(COL_FONT_STATUS);
	DrawText(40, 425, line1);
	DrawText(40, 450, line2);
}

void DrawHLine (int x1, int x2, int y, int color)
{
	int i;
	y = 320 * y;
	x1 >>= 1;
	x2 >>= 1;
	for (i = x1; i <= x2; i++)
	{
		u32 *tmpfb = xfb[whichfb];
		tmpfb[y+i] = color;
	}
}

void DrawVLine (int x, int y1, int y2, int color)
{
	int i;
	x >>= 1;
	for (i = y1; i <= y2; i++)
	{
		u32 *tmpfb = xfb[whichfb];
		tmpfb[x + (640 * i) / 2] = color;
	}
}

void DrawBox (int x1, int y1, int x2, int y2, int color)
{
	DrawHLine (x1, x2, y1, color);
	DrawHLine (x1, x2, y2, color);
	DrawVLine (x1, y1, y2, color);
	DrawVLine (x2, y1, y2, color);
}

void DrawBoxFilled (int x1, int y1, int x2, int y2, int color)
{
	int h;
	for (h = y1; h <= y2; h++)
		DrawHLine (x1, x2, h, color);
}

void DrawBoxFilledGradient (int x1, int y1, int x2, int y2, u32 color1, u32 color2, float location)
{
	int r1 = (color1&0x0000FF) >> 0;	int g1 = (color1&0x00FF00) >> 8;	int b1 = (color1&0xFF0000) >> 16;
	int r2 = (color2&0x0000FF) >> 0;	int g2 = (color2&0x00FF00) >> 8;	int b2 = (color2&0xFF0000) >> 16;

	int r,g,b, midr, midg, midb;
	midr = (r1 * 0.5) + (r2 * (1 - 0.5));
	midg = (g1 * 0.5) + (g2 * (1 - 0.5));
	midb = (b1 * 0.5) + (b2 * (1 - 0.5));	
	float p;
	int h;
	for (h = y1; h <= y2; h++){
		if ((y2-h) > (y2-y1)*location)
		{	
			p = ((float)(y2-h)-((y2-y1)*location))/((float)(y2-y1)-((y2-y1)*location));
			r = (r1 * p) + (midr * (1 - p));
			g = (g1 * p) + (midg * (1 - p));
			b = (b1 * p) + (midb * (1 - p));
		}
		else
		{
			p = ((float)(y2-h))/((float)(y2-y1)*location);
			r = (midr * p) + (r2 * (1 - p));
			g = (midg * p) + (g2 * (1 - p));
			b = (midb * p) + (b2 * (1 - p));		
		}
		DrawHLine (x1, x2, h, getcolour(r,g,b));
	}
}
