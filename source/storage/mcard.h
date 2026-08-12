/**
 * @file mcard.h
 * @brief High-level GameCube memory-card save operations.
 *
 * Functions in this module own shared card work areas and FileBuffer. They are
 * synchronous and show user-facing errors themselves; callers should not use
 * the card buffer concurrently or assume a card remains mounted afterward.
 */
#ifndef GCMM_MCARD_H
#define GCMM_MCARD_H

#include <gccore.h>
#include "gci.h"

/** GCI header currently loaded from a card or storage file. */
extern card_direntry gci;
/** Extra payload offset used while converting GCS/SAV inputs; reset after write. */
extern int OFFSET;

/** Build gci and the leading FileBuffer header from the current card status. */
void GCIMakeHeader(void);
/** Copy the leading FileBuffer GCI header into the current card status. */
void ExtractGCIHeader(void);
/** Mount a card after libogc's required device-detection delay. */
int MountCard(int cslot);
/** Return free 8 KiB blocks, or zero after a mount/query failure. */
u16 FreeBlocks(s32 chn);
/** Refresh the save directory cache and return cached entry count. */
int CardGetDirectory(int slot);
/** Return nonzero only when id identifies a currently cached save. */
int MCardIsValidSaveIndex(int id);
/** Read save count and available blocks for a card. */
int MCardGetUsage(int slot, int *save_count, u16 *free_blocks);
/** Print cached save names to the debug console. */
void CardListFiles(void);
/** Load selected save header, comments, banner, and icon data. */
int CardReadFileHeader(int slot, int id);
/** Copy selected metadata and a NUL-terminated 64-byte comment preview. */
int MCardGetSaveDetails(int slot, int id, card_direntry *entry, char comments[65]);
/** Load only metadata, banner, and comments for a list preview. */
int MCardLoadSavePreview(int slot, int id, card_direntry *entry, char comments[65]);
/** Shared decoded-preview source buffers owned by the card module. */
extern u16 bannerdata[CARD_BANNER_W * CARD_BANNER_H] ATTRIBUTE_ALIGN(32);
extern u8 bannerdataCI[CARD_BANNER_W * CARD_BANNER_H] ATTRIBUTE_ALIGN(32);
extern u16 tlutbanner[256] ATTRIBUTE_ALIGN(32);
extern u8 CommentBuffer[64] ATTRIBUTE_ALIGN(32);
/** Load selected GCI header and complete payload into FileBuffer. */
int CardReadFile(int slot, int id);

#define MCARD_WRITE_FAILED 0
#define MCARD_WRITE_OK     1
#define MCARD_WRITE_EXISTS 2
/**
 * Write FileBuffer to slot. Existing saves are replaced only when allowed.
 * Returns one of MCARD_WRITE_FAILED, MCARD_WRITE_OK, or MCARD_WRITE_EXISTS.
 */
int CardWriteFile(int slot, int overwrite_allowed);
/** Compare destination payload against last write before source deletion. */
int MCardVerifyLastWrite(int slot);
/** Delete selected cached save after caller has confirmed destructive action. */
int MCardDeleteFile(int slot, int id);
/** Format entire card after caller has confirmed destructive action. */
int MCardFormat(int slot);
/** Convert a libogc error code into a visible user-facing error message. */
void WaitCardError(char *src, int error);

#endif /* GCMM_MCARD_H */
