/**
 * @file gci.h
 * @brief Binary GameCube save-directory entry and GCI metadata helpers.
 *
 * A GCI file starts with one 64-byte GameCube directory entry, followed by
 * the save payload in 8 KiB allocation blocks.  Keep this layout byte-for-
 * byte compatible: it is written directly to storage and consumed by console
 * software.  Do not add fields, remove packing, or change field order.
 */
#ifndef GCMM_GCI_H
#define GCMM_GCI_H

#include <gccore.h>
#include <ogc/libversion.h>

#if (_V_MAJOR_ <= 2) && (_V_MINOR_ <= 2)
/** Raw 64-byte directory entry used by GameCube memory cards and GCI files. */
typedef struct _card_direntry {
	u8 gamecode[4];                 /**< Four-byte game identifier. */
	u8 company[2];                  /**< Two-byte publisher identifier. */
	u8 pad_00;                      /**< Reserved byte; retain as stored. */
	u8 banner_fmt;                  /**< Banner format and icon animation mode. */
	u8 filename[CARD_FILENAMELEN];  /**< Fixed-width save name, not necessarily NUL-terminated. */
	u32 last_modified;              /**< GameCube epoch timestamp. */
	u32 icon_addr;                  /**< Offset from save payload to visual data. */
	u16 icon_fmt;                   /**< Two-bit format for each icon frame. */
	u16 icon_speed;                 /**< Two-bit duration for each icon frame. */
	u8 permission;                  /**< Copy/move permissions. */
	u8 copy_times;                  /**< Remaining copy count. */
	u16 block;                      /**< First physical block; informational in GCI files. */
	u16 length;                     /**< Payload length in 8 KiB blocks. */
	u16 pad_01;                     /**< Reserved bytes; retain as stored. */
	u32 comment_addr;               /**< Offset from payload to two 32-byte comments. */
} card_direntry;
#else
#include <ogc/card.h>
#endif

/*
 * Define STATUSOGC only when callers intentionally prefer CARD_GetStatus()
 * and CARD_SetStatus().  Default extended APIs preserve original timestamps,
 * permissions, and copy counts during save restoration.
 */
/* #define STATUSOGC */

/** Extract encoded banner and icon properties from a directory entry. */
#define SDCARD_GetBannerFmt(banner_fmt) ((banner_fmt) & 0x03)
#define SDCARD_GetIconFmt(icon_fmt, n) (((icon_fmt) >> (2 * (n))) & 0x03)
#define SDCARD_GetIconSpeed(icon_speed, n) (((icon_speed) >> (2 * (n))) & 0x03)
#define SDCARD_GetIconSpeedBounce(icon_speed, n, i) \
	((n) < (i) ? (((icon_speed) >> (2 * (n))) & 0x03) : \
	(((icon_speed) >> (2 * ((i) * 2 - 2 - (n)))) & 0x03))
#define SDCARD_GetIconAnim(banner_fmt) ((banner_fmt) & 0x04)

/** Offset from a loaded GCI buffer to its save payload. */
#define MCDATAOFFSET 64

#endif /* GCMM_GCI_H */
