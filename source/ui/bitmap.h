/**
 * @file bitmap.h
 * @brief Linear-framebuffer bitmap and save-preview rendering.
 */
#ifndef GCMM_BITMAP_H
#define GCMM_BITMAP_H

#include <gccore.h>

/** Active video mode and double-buffered framebuffers initialized by main.c. */
extern GXRModeObj *vmode;
extern u32 *xfb[2];
extern int whichfb;

/** Pack two RGB pixels into one GameCube Y1CbY2Cr framebuffer word. */
u32 CvtRGB(u8 r1, u8 g1, u8 b1, u8 r2, u8 g2, u8 b2);
/** Validate and draw a 24-bit uncompressed BMP centered on next framebuffer. */
u32 ShowBMP(u8 *bmpfile);
/** Validate and draw a 24-bit uncompressed BMP at framebuffer coordinates. */
u32 DrawBMPAt(u8 *bmpfile, int x, int y);
/** Draw decoded BGR banner preview supplied by bannerload.c. */
void ShowBanner(u8 *banner);
/** Draw decoded BGR icon preview supplied by bannerload.c. */
void ShowIcon(u8 *icon);
/** Clear next framebuffer and make it displayable. */
void ClearScreen(void);

#endif /* GCMM_BITMAP_H */
