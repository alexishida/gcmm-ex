/**
 * @file raw.h
 * @brief Full GameCube memory-card image operations.
 *
 * RAW restoration and formatting-adjacent operations can overwrite an entire
 * card.  Callers must obtain explicit user confirmation before invoking a
 * restore and must keep the card and storage device connected until it ends.
 */
#ifndef GCMM_RAW_H
#define GCMM_RAW_H

#include <gccore.h>

/** On-card system-area header. Its packed layout is part of RAW compatibility. */
typedef struct {
	u8 serial[12];       /**< Encrypted serial representation used by libogc. */
	u64 formatTime;      /**< Formatting time used to derive the card serial. */
	u32 SramBias;        /**< SRAM bias captured during formatting. */
	u32 SramLang;        /**< SRAM language captured during formatting. */
	u8 Unk2[4];          /**< Observed reserved bytes. */
	u8 deviceID[2];      /**< Formatting slot identifier. */
	u8 SizeMb[2];        /**< Card capacity in megabits. */
	u16 Encoding;        /**< Character encoding identifier. */
	u8 Unused1[468];     /**< Reserved area, normally 0xFF. */
	u16 UpdateCounter;   /**< System-header generation counter. */
	u16 Checksum;        /**< Additive checksum of system-header data. */
	u16 Checksum_Inv;    /**< Inverse additive checksum. */
	u8 Unused2[7680];    /**< Remaining card system area. */
} __attribute__((__packed__)) Header;

/** Receives completed and total RAW card blocks during a full-card transfer. */
typedef void (*RawProgressCallback)(void *context, u32 completed, u32 total);

/** Release temporary RAW-image buffers allocated by the subsystem. */
void freecardbuf(void);
/** Set or clear the optional RAW transfer-progress callback. */
void RawSetProgressCallback(RawProgressCallback callback, void *context);
/** Derive the plain card serial from a loaded RAW system header. */
void getserial(u8 *serial);
/** Return a card serial suitable for source/destination identity checks. */
u64 Card_SerialNo(s32 slot);
/** Back up an entire mounted card. Returns nonzero on success. */
s8 BackupRawImage(s32 slot, s32 *bytes_written);
/** Restore a validated RAW image. Returns nonzero on success. */
s8 RestoreRawImage(s32 slot, char *sdfilename, s32 *bytes_written);
/** Validate RAW image type, size, header, capacity, and target-card identity. */
int ValidateRawImage(s32 slot, const char *sdfilename, u32 *image_size);

#endif /* GCMM_RAW_H */
