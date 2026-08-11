/****************************************************************************
* mcard support prototypes
****************************************************************************/
#ifndef _MCARDSUP_
#define _MCARDSUP_

#include "gci.h"

extern card_direntry gci;

void GCIMakeHeader();
void ExtractGCIHeader();
int MountCard(int cslot);
u16 FreeBlocks(s32 chn);
int CardGetDirectory (int slot);
int MCardGetUsage(int slot, int *save_count, u16 *free_blocks);
int MCardIsValidSaveIndex(int id);
void CardListFiles ();
int CardReadFileHeader (int slot, int id);
int MCardGetSaveDetails(int slot, int id, card_direntry *entry, char comments[65]);
int CardReadFile (int slot, int id);
#define MCARD_WRITE_FAILED 0
#define MCARD_WRITE_OK 1
#define MCARD_WRITE_EXISTS 2
int CardWriteFile(int slot, int overwrite_allowed);
int MCardVerifyLastWrite(int slot);
int MCardDeleteFile(int slot, int id);
int MCardFormat(int slot);
void WaitCardError(char *src, int error);
extern int OFFSET;
#endif
