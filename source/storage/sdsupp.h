/**
 * @file sdsupp.h
 * @brief Mounted-storage file I/O for save backups and card images.
 */
#ifndef GCMM_STORAGE_H
#define GCMM_STORAGE_H

#include <stdbool.h>

#define MCSAVES "MCBACKUP"

/* Stable device indexes shared with main.c's mount state. */
#define DEV_NUM    0
#define DEV_GCSDA  1
#define DEV_GCSDB  2
#define DEV_GCSDC  3
#define DEV_GCODE  4
#define DEV_WIISD  5
#define DEV_WIIUSB 6
#define DEV_TOTAL  6

#define DEV_ND      0
#define DEV_AVAIL   1
#define DEV_MOUNTED 2

/** Write current FileBuffer GCI data to the active storage folder. */
int SDSaveMCImage(void);
/** Load and normalize a GCI, GCS, or SAV save file into FileBuffer. */
int SDLoadMCImage(char *sdfilename);
/** Load GCI/GCS/SAV metadata and preview assets without loading save payload. */
int SDLoadMCImageHeader(char *sdfilename);
/** Load RAW/GCP/MCI system metadata for a full-card restore. */
int SDLoadCardImageHeader(char *sdfilename);
/** Return nonzero when path resolves to a directory on mounted storage. */
int isdir_sd(char *path);
/** Return true when filename can be opened for reading. */
bool file_exists(const char *filename);
/** Populate filelist with folders and supported files for requested mode. */
int SDGetFileList(int mode);

#endif /* GCMM_STORAGE_H */
