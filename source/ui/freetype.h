/**
 * @file freetype.h
 * @brief FreeType text rendering and framebuffer drawing primitives.
 *
 * Initialize this module after video initialization and before drawing text.
 * Coordinates use the active linear framebuffer's pixel space.
 */
#ifndef GCMM_FREETYPE_H
#define GCMM_FREETYPE_H

#include <gccore.h>

#ifdef HW_RVL
/** Register console and Wii Remote power-button callbacks. */
void initialise_power(void);
#endif

#define COL_BG2         getcolour(64, 64, 64)
#define COL_FONT        255, 255, 255
#define COL_FONT_STATUS COL_FONT
#define COL_BLACK       getcolour(0, 0, 0)

/** Initialize bundled TrueType font. Returns zero on success. */
int FT_Init(void);
/** Set text pixel height for subsequent DrawText calls. */
void setfontsize(int pixelsize);
/** Set RGB color for subsequent DrawText calls. */
void setfontcolour(u8 r, u8 g, u8 b);
/** Draw a NUL-terminated string; x == -1 centers it horizontally. */
void DrawText(int x, int y, char *text);
/** Convert an RGB triplet to the framebuffer color representation. */
unsigned int getcolour(u8 r1, u8 g1, u8 b1);
/** Present next framebuffer and wait for vertical retrace. */
void ShowScreen(void);
/** Display a non-blocking status message. */
void ShowAction(char *msg);
/** Display a message and wait until user confirms it. */
void WaitPrompt(char *msg);
/** Draw two status lines in bottom screen area. */
void writeStatusBar(char *line1, char *line2);

/** Draw horizontal or vertical lines and outlined or filled rectangles. */
void DrawHLine(int x1, int x2, int y, int color);
void DrawVLine(int x, int y1, int y2, int color);
void DrawBox(int x1, int y1, int x2, int y2, int color);
void DrawBoxFilled(int x1, int y1, int x2, int y2, int color);
/** Draw vertical gradient; location is transition point from 0.0 to 1.0. */
void DrawBoxFilledGradient(int x1, int y1, int x2, int y2,
	u32 color1, u32 color2, float location);

#endif /* GCMM_FREETYPE_H */
