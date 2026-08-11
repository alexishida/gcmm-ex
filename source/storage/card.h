/*-------------------------------------------------------------

card.h -- Memory card subsystem

Copyright (C) 2004
Michael Wiedenbauer (shagkur)
Dave Murphy (WinterMute)

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1.	The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.

2.	Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3.	This notice may not be removed or altered from any source
distribution.

-------------------------------------------------------------*/

/**
 * @file card.h
 * @brief Extra CARD declarations required by GCMM's local compatibility driver.
 *
 * The legacy include guard is retained for compatibility with libogc versions
 * that expect it. Only declarations absent from those versions live here.
 */
#ifndef __CARD_H__
#define __CARD_H__

#include <ogc/libversion.h>
#if (_V_MAJOR_ <= 2) && (_V_MINOR_ <= 2)

/*!
\file card.h
\brief Additions to libogc's memory card subsystem, implemented by card.c

This used to be a full copy of libogc's card.h, and it never took effect when
<ogc/card.h> had already been included: both used the same __CARD_H__ include
guard, so the copy silently stepped aside. libogc2 renamed its guard to
__OGC_CARD_H__ in 2023, from which point the copy was included for real and
collided with the very header it was meant to shadow. Only the declarations
libogc does not provide are kept here; card.c still replaces the implementation
of the rest.
*/

#include <ogc/card.h>

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

/* Defined in card.c. Forward declared so the tag has file scope: mentioning it
   for the first time inside a prototype would scope it to that prototype. */
struct card_direntry;

/*! \fn s32 CARD_GetFreeBlocks(s32 chn)
\brief Get the free blocks in memory card
\param[in] chn CARD slot.
\param[in] freeblocks pointer to receive freeblocks value.

\return \ref card_errors "card error codes" or free blocks
*/
s32 CARD_GetFreeBlocks(s32 chn, u16* freeblocks);

/*! \fn s32 __card_getstatusex(s32 chn,s32 fileno,struct card_direntry *entry)
\brief Get the directory entry (GCI header)
\param[in] chn CARD slot.
\param[in] fileno file index. returned by a previous call to CARD_Open().
\param[out] entry pointer to receive the directory entry.

\return \ref card_errors "card error codes"
*/
s32 __card_getstatusex(s32 chn,s32 fileno,struct card_direntry *entry);


/*! \fn s32 __card_setstatusex(s32 chn,s32 fileno,struct card_direntry *entry)
\brief Set the directory entry (preferably from a GCI header), except block index and lenght
\param[in] chn CARD slot.
\param[in] fileno file index. returned by a previous call to CARD_Open().
\param[out] entry pointer to a directory entry structure (or GCI header).

\return \ref card_errors "card error codes"
*/
s32 __card_setstatusex(s32 chn,s32 fileno,struct card_direntry *entry);

/* CARD_GetSerialNo                                        */
/*                                                         */
/* serial1 & serial2: Encrypted memory card serial numbers */
/* chn: Memory card port                                   */
/* ret: Error code                                         */
s32 CARD_GetSerialNo(s32 chn,u32 *serial1,u32 *serial2);

// Raw read and write functions
s32 __card_read(s32 chn,u32 address,u32 block_len,void *buffer,cardcallback callback);
s32 __card_write(s32 chn,u32 address,u32 block_len,void *buffer,cardcallback callback);
s32 __card_sync(s32 chn);
s32 __card_sectorerase(s32 chn,u32 sector,cardcallback callback);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif

#endif
