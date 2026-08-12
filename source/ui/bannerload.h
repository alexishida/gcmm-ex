/**
 * @file bannerload.h
 * @brief Decode GameCube RGB5A3 and indexed banner/icon textures for previews.
 */
#ifndef GCMM_BANNERLOAD_H
#define GCMM_BANNERLOAD_H

#include <gccore.h>

/* RGB values are stored in BGR order to match the framebuffer conversion path. */
#define BLUECOL   ((170 << 16) | (30 << 8) | 30)
#define PURPLECOL ((130 << 16) | (30 << 8) | 100)
#define LOCATION  0.5f

/** Decode a tiled RGB5A3 banner into the preview bitmap buffer. */
void bannerloadRGB(u16 *gamebanner);
/** Decode an indexed banner and palette into the preview bitmap buffer. */
void bannerloadCI(u8 *gamebanner, u16 *tlutbanner);
/** Decode and draw a RGB5A3 save banner at a UI position. */
void DrawBannerRGBAt(const u16 *gamebanner, int x, int y, int scale);
/** Decode and draw an indexed save banner at a UI position. */
void DrawBannerCIAt(const u8 *gamebanner, const u16 *tlutbanner,
	int x, int y, int scale);
/** Decode a tiled RGB5A3 icon into the preview bitmap buffer. */
void iconloadRGB(u16 *gameicon);
/** Decode an indexed icon and palette into the preview bitmap buffer. */
void iconloadCI(u8 *gameicon, u16 *tluticon);

#endif /* GCMM_BANNERLOAD_H */
