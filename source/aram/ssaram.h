/****************************************************************************
* SSARAM
***************************************************************************/
/**
 * @file ssaram.h
 * @brief GameCube-only DMA transfer helpers for auxiliary RAM.
 */
#ifndef HW_RVL
#ifndef __SSARAM__
#define __SSARAM__

/** Write len bytes to ARAM, preserving bytes around an unaligned destination. */
void ARAMPut(unsigned char *src, char *dst, int len);
/** Read len bytes from ARAM into main memory. */
void ARAMFetch(unsigned char *dst, char *src, int len);

#endif
#endif
