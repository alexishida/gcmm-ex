/****************************************************************************
* ds bomb gamecube memory card manager
* based on libOGC Memory Card Backup by askot
* delete support + saves information made by dsbomb
* Gui original design by dsbomb
* Gui adds and user interaction by justb
* Banner/Icon display updates by Dronesplitter
* Uses freetype.
* libFreeType is available from the downloads sections.
*****************************************************************************/
/**
 * @file main.c
 * @brief Application startup, device lifecycle, and user workflow orchestration.
 *
 * Coordinates UI, mounted storage, and memory-card subsystems. File-format
 * parsing and card-driver details remain in storage/. Every workflow must
 * preserve source data after a failed copy, move, backup, or restore.
 */
#include <gccore.h>
#include <ogcsys.h>
#include <network.h>
#include <smb.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <sys/dir.h>
#include <sys/time.h>
#include <fat.h>
#include <dvm.h>
#include <sdcard/gcsd.h>

#ifdef HW_RVL
#include <wiiuse/wpad.h>
#include <sdcard/wiisd_io.h>
#endif


#include "mcard.h"
#include "gci.h"
#include "raw.h"
#include "sdsupp.h"
#include "freetype.h"
#include "bitmap.h"
#include "ui.h"

#ifndef HW_RVL
#include "aram/sidestep.h"
#endif

#define PSOSDLOADID 0x7c6000a6
//Comment FLASHIDCHECK to allow writing any image to any mc. This will corrupt official cards.
#define FLASHIDCHECK

#define SDGECKOA_PATH "sda"
#define SDGECKOB_PATH "sdb"
#define SD2SP2_PATH "sdc"
#define GCLOADER_PATH "gcl"
#define WIISD_PATH "wsd"
#define WIIUSB_PATH "usb"
/*** Room for a 3 char basename plus a partition index appended by libdvm
     (e.g. "sda2" for the second partition of the SD Gecko in slot A) ***/
char fatpath[8];
/*** Basename handed to the mount probe, kept so every volume it brought up
     can be unmounted again ***/
static char fatbase[8];

const char appversion[] = "v1.0.1";

/* Legacy I/O workflows retained while their task screens are migrated. */
void SD_RawBackupMode(void);
void SD_RawRestoreMode(void);

typedef struct {
	const char *title;
	const char *item;
} raw_progress_context;

static void show_raw_progress(void *context, u32 completed, u32 total)
{
	raw_progress_context *progress = context;

	if (progress)
		UI_Progress(progress->title, progress->item, completed, total);
}

/*** 2D Video Globals ***/
GXRModeObj *vmode;		/*** Graphics Mode Object ***/
u32 *xfb[2] = { NULL, NULL };	/*** Framebuffers ***/
int whichfb = 0;		/*** Frame buffer toggle ***/
int screenheight;
int vmode_60hz = 0;
u32 retraceCount;

extern u8 filelist[1024][1024];
extern u8 currFolder[260];
u8 selector_flag;

s32 MEM_CARD = CARD_SLOTB;
extern syssramex *sramex;
extern u8 imageserial[12];
extern syssramex *__SYS_LockSramEx();
extern u32 __SYS_UnlockSramEx(u32 write);

#define NOFAT_MSG "No FAT device detected. You may run device selector again."

static void updatePAD(u32 retrace)
{
    retraceCount = retrace;
	PAD_ScanPads();
#ifdef HW_RVL
	WPAD_ScanPads();
#endif
}

/* DEfinitions in sdsupp.h
#define DEV_NUM 	0
#define DEV_GCSDA 	1
#define DEV_GCSDB 	2
#define DEV_GCSDC 	3
#define DEV_GCODE 	4
#define DEV_WIISD 	5
#define DEV_WIIUSB	6

#define DEV_TOTAL 6

#define DEV_ND		0
#define DEV_AVAIL	1
#define DEV_MOUNTED 2
*/
typedef enum {
	STORAGE_NOT_DETECTED = 0,
	STORAGE_DETECTED,
	STORAGE_MOUNTED,
	STORAGE_UNSUPPORTED_FILESYSTEM,
	STORAGE_MOUNT_FAILED
} storage_state;

typedef enum {
	MOUNT_OK = 0,
	MOUNT_FAILED,
	MOUNT_UNSUPPORTED_FILESYSTEM
} mount_result;

u8 DEVICES [DEV_TOTAL+1];
bool have_sd;
u8 CUR_DEVICE; //Current device index
static storage_state storage_mount_state = STORAGE_NOT_DETECTED;

static void detect_devices(){

	int i = 0;
	DEVICES[DEV_NUM]=0;
	for (i=0;i<=DEV_TOTAL;i++) DEVICES[i]=0;

		if (__io_gcsda.startup(&__io_gcsda)&&__io_gcsda.shutdown(&__io_gcsda)){
			DEVICES[DEV_GCSDA] = DEV_AVAIL;
			DEVICES[DEV_NUM] +=1;
		}
		if (__io_gcsdb.startup(&__io_gcsdb)&&__io_gcsdb.shutdown(&__io_gcsdb)){
			DEVICES[DEV_GCSDB] = DEV_AVAIL;
			DEVICES[DEV_NUM] +=1;
		}
#ifdef HW_DOL
		if (__io_gcsd2.startup(&__io_gcsd2)&&__io_gcsd2.shutdown(&__io_gcsd2)){
			DEVICES[DEV_GCSDC] = DEV_AVAIL;
			DEVICES[DEV_NUM] +=1;
		}
		if (__io_gcode.startup(&__io_gcode)&&__io_gcode.shutdown(&__io_gcode)){
			DEVICES[DEV_GCODE] = DEV_AVAIL;
			DEVICES[DEV_NUM] +=1;
		}
#endif
#ifdef HW_RVL
		if (__io_wiisd.startup(&__io_wiisd)&&__io_wiisd.shutdown(&__io_wiisd)){
			DEVICES[DEV_WIISD] = DEV_AVAIL;
			DEVICES[DEV_NUM] +=1;
		}
		if (__io_usbstorage.startup(&__io_usbstorage))
		{
			if (__io_usbstorage.isInserted(&__io_usbstorage))
			{
				DEVICES[DEV_WIIUSB] = DEV_AVAIL;
				DEVICES[DEV_NUM] +=1;
			}
			__io_usbstorage.shutdown(&__io_usbstorage);
		}
#endif
	if (!have_sd)
		storage_mount_state = DEVICES[DEV_NUM] ? STORAGE_DETECTED : STORAGE_NOT_DETECTED;
}

/****************************************************************************
* mountDevice
*
* Mounts a block device, autodetecting the filesystem it holds.
*
* libfat's fatMountSimple() only ever handles FAT12/16/32. libdvm's version
* of it is hardcoded to the "vfat" driver too, so it is bypassed here in
* favour of dvmProbeMountDiscIface(): that one reads the MBR/VBR, identifies
* the filesystem of each partition and mounts it with the matching driver,
* which gives us exFAT on top of plain FAT.
*
* The first (or only) partition is mounted under the basename as-is, so
* fatpath keeps holding "sda", "wsd" and friends. Further partitions get the
* partition index appended ("sda2", "sda3"...), hence the wider fatpath.
*
* Cache is kept deliberately small (8 pages of 8 sectors, 32KB): GCMM already
* holds a 2MB file buffer and the GameCube only has 24MB of MEM1.
****************************************************************************/
#define DVM_CACHE_PAGES 8
#define DVM_SECTORS_PER_PAGE 8
#define DVM_MAX_PARTITIONS 4

/*** Name libdvm gives to partition n (0 based) of a device: the first one
     keeps the basename, the rest get the partition number appended ***/
static void volumeName(char *out, const char *base, int part)
{
	size_t length = strnlen(base, 7);

	if (length == 7 || (part > 0 && length > 5)) {
		out[0] = '\0';
		return;
	}
	memcpy(out, base, length);
	if (part > 0)
		out[length++] = (char)('1' + part);
	out[length] = '\0';
}

static mount_result mountDevice(const char *name, DISC_INTERFACE *iface)
{
	char volname[8];
	char devname[10];
	int i;
	int length;

	/*** Registering a driver twice is a no-op, so this is safe to repeat ***/
	dvmRegisterFsDriver(&g_vfatFsDriver);
	dvmRegisterFsDriver(&g_exfatFsDriver);

	if (!dvmProbeMountDiscIface(name, iface, DVM_CACHE_PAGES, DVM_SECTORS_PER_PAGE))
		return MOUNT_FAILED;

	length = snprintf(fatbase, sizeof(fatbase), "%s", name);
	if (length < 0 || length >= (int)sizeof(fatbase))
		return MOUNT_FAILED;

	/*** Point fatpath at the first volume that actually came up. The probe
	     skips partitions holding a filesystem we have no driver for, so the
	     usable one is not necessarily the first ***/
	for (i = 0; i < DVM_MAX_PARTITIONS; i++)
	{
		volumeName(volname, name, i);
		length = snprintf(devname, sizeof(devname), "%s:", volname);
		if (length < 0 || length >= (int)sizeof(devname))
			continue;

		if (GetDeviceOpTab(devname) != NULL)
		{
			length = snprintf(fatpath, sizeof(fatpath), "%s", volname);
			if (length < 0 || length >= (int)sizeof(fatpath))
				continue;
			return MOUNT_OK;
		}
	}

	/* A probe can succeed even when no supported filesystem was mounted. */
	for (i = 0; i < DVM_MAX_PARTITIONS; i++) {
		volumeName(volname, name, i);
		fatUnmount(volname);
	}
	fatpath[0] = '\0';
	fatbase[0] = '\0';
	return MOUNT_UNSUPPORTED_FILESYSTEM;
}

static bool mount_storage_device(const char *name, DISC_INTERFACE *iface,
	const char *mount_error)
{
	mount_result result = mountDevice(name, iface);

	if (result == MOUNT_OK) {
		storage_mount_state = STORAGE_MOUNTED;
		return true;
	}
	storage_mount_state = result == MOUNT_UNSUPPORTED_FILESYSTEM ?
		STORAGE_UNSUPPORTED_FILESYSTEM : STORAGE_MOUNT_FAILED;
	if (result == MOUNT_UNSUPPORTED_FILESYSTEM)
		WaitPrompt("Unsupported filesystem. Use FAT12, FAT16, FAT32, or exFAT.");
	else
		WaitPrompt((char *)mount_error);
	return false;
}

/*
1 GC SD Gecko slot A
2 GC SD Gecko slot B
3 GC SD2SP2
4 GC Loader
5 Wii SD
6 Wii uSB
*/
static bool initFAT(int device)
{
	ShowAction("Mounting device...");
	char msg[128];
	if (device < 1 || device > DEV_TOTAL) {
		storage_mount_state = STORAGE_NOT_DETECTED;
		snprintf(msg, sizeof(msg), "Failed to mount invalid device %d.", device);
		WaitPrompt(msg);
		return false;
	}
	if (DEVICES[device] != DEV_AVAIL){
		storage_mount_state = STORAGE_NOT_DETECTED;
		snprintf(msg, sizeof(msg), "Failed to mount unavailable device %d.", device);
		WaitPrompt(msg);
		return false;
	}
	storage_mount_state = STORAGE_DETECTED;
	//sprintf (msg, "Mounting device... %d", device);
	//WaitPrompt(msg);
	switch (device)
	{
		case DEV_GCSDA:
			__io_gcsda.startup(&__io_gcsda);
			if (!__io_gcsda.isInserted(&__io_gcsda))
			{
				WaitPrompt("No SD Gecko inserted in SLOT A!");
				__io_gcsda.shutdown(&__io_gcsda);
				return false;
			}
			if (!mount_storage_device(SDGECKOA_PATH, &__io_gcsda,
				"Error mounting SD Gecko in Slot A.")) {
				__io_gcsda.shutdown(&__io_gcsda);
				return false;
			}
			DEVICES[DEV_GCSDA] = DEV_MOUNTED;
			break;
		
		case DEV_GCSDB:
			__io_gcsdb.startup(&__io_gcsdb);
			if (!__io_gcsdb.isInserted(&__io_gcsdb))
			{
				WaitPrompt("No SD card inserted in SLOT B!");
				__io_gcsdb.shutdown(&__io_gcsdb);
				return false;
			}
			if (!mount_storage_device(SDGECKOB_PATH, &__io_gcsdb,
				"Error mounting SD Gecko in Slot B.")) {
				__io_gcsdb.shutdown(&__io_gcsdb);
				return false;
			}
			DEVICES[DEV_GCSDB] = DEV_MOUNTED;
			break;
#ifdef HW_DOL
		case DEV_GCSDC:
			__io_gcsd2.startup(&__io_gcsd2);
			if (!__io_gcsd2.isInserted(&__io_gcsd2))
			{
				WaitPrompt("No SD card inserted in SD2SP2!");
				__io_gcsd2.shutdown(&__io_gcsd2);
				return false;
			}
			if (!mount_storage_device(SD2SP2_PATH, &__io_gcsd2,
				"Error mounting SD2SP2.")) {
				__io_gcsd2.shutdown(&__io_gcsd2);
				return false;
			}
			DEVICES[DEV_GCSDC] = DEV_MOUNTED;
			break;

		case DEV_GCODE:
			__io_gcode.startup(&__io_gcode);
			if (!__io_gcode.isInserted(&__io_gcode))
			{
				WaitPrompt("No SD card inserted in GCLoader!");
				__io_gcode.shutdown(&__io_gcode);
				return false;
			}
			if (!mount_storage_device(GCLOADER_PATH, &__io_gcode,
				"Error mounting GC Loader.")) {
				__io_gcode.shutdown(&__io_gcode);
				return false;
			}
			DEVICES[DEV_GCODE] = DEV_MOUNTED;
			break;
#elif HW_RVL
		case DEV_WIISD:
			__io_wiisd.startup(&__io_wiisd);
			if (!__io_wiisd.isInserted(&__io_wiisd))
			{
				WaitPrompt("No SD card inserted in front SD slot!");
				__io_wiisd.shutdown(&__io_wiisd);
				return false;
			}
			if (!mount_storage_device(WIISD_PATH, &__io_wiisd,
				"Error mounting Wii Front SD.")) {
				__io_wiisd.shutdown(&__io_wiisd);
				return false;
			}
			DEVICES[DEV_WIISD] = DEV_MOUNTED;
			break;
		
		case DEV_WIIUSB:
			__io_usbstorage.startup(&__io_usbstorage);
			if (!__io_usbstorage.isInserted(&__io_usbstorage))
			{
				WaitPrompt("No USB device inserted!");
				__io_usbstorage.shutdown(&__io_usbstorage);
				return false;
			}
			if (!mount_storage_device(WIIUSB_PATH, &__io_usbstorage,
				"Error mounting USB storage.")) {
				__io_usbstorage.shutdown(&__io_usbstorage);
				return false;
			}
			DEVICES[DEV_WIIUSB] = DEV_MOUNTED;
			break;
#endif

		default:
			WaitPrompt("Unwknown error mounting device");
			return false;
			break;
	}
	
	return true;
}

void deinitFAT()
{
	char volname[8];
	int i;

	//First unmount all the devs...
	//The probe may have mounted more than one partition of the device,
	//so walk them all instead of just the one fatpath points at
	for (i = 0; fatbase[0] && i < DVM_MAX_PARTITIONS; i++)
	{
		volumeName(volname, fatbase, i);
		fatUnmount (volname);
	}
	fatpath[0]='\0';
	fatbase[0]='\0';
	have_sd = 0;
	CUR_DEVICE = 0;
	//...and then shutdown em!
	if (DEVICES[DEV_GCSDA] == DEV_MOUNTED){
		__io_gcsda.shutdown(&__io_gcsda);
		DEVICES[DEV_GCSDA]=DEV_AVAIL;
	}
	if (DEVICES[DEV_GCSDB] == DEV_MOUNTED){
		__io_gcsdb.shutdown(&__io_gcsdb);
		DEVICES[DEV_GCSDB]=DEV_AVAIL;
	}
#ifdef HW_DOL
	if (DEVICES[DEV_GCSDC] == DEV_MOUNTED){
		__io_gcsd2.shutdown(&__io_gcsd2);
		DEVICES[DEV_GCSDC]=DEV_AVAIL;
	}
	if (DEVICES[DEV_GCODE] == DEV_MOUNTED){
		__io_gcode.shutdown(&__io_gcode);
		DEVICES[DEV_GCODE]=DEV_AVAIL;
	}
#endif
#ifdef	HW_RVL
	if (DEVICES[DEV_WIISD] == DEV_MOUNTED){
		__io_wiisd.shutdown(&__io_wiisd);
		DEVICES[DEV_WIISD]=DEV_AVAIL;
	}
	if (DEVICES[DEV_WIIUSB] == DEV_MOUNTED){
		__io_usbstorage.shutdown(&__io_usbstorage);
		DEVICES[DEV_WIIUSB]=DEV_AVAIL;
	}
#endif
}

u8 skip_selector = 1;
static const char *device_name(int device)
{
	switch (device) {
	case DEV_GCSDA: return "SD Gecko - Slot A";
	case DEV_GCSDB: return "SD Gecko - Slot B";
	case DEV_GCSDC: return "SD2SP2";
	case DEV_GCODE: return "GC Loader";
	case DEV_WIISD: return "Wii Front SD";
	case DEV_WIIUSB: return "USB storage";
	default: return "No storage device";
	}
}

static const char *workflow_source = "Not selected";
static const char *workflow_destination = "Not selected";

static const char *memory_card_name(int slot)
{
	return slot == CARD_SLOTA ? "Memory Card A" : "Memory Card B";
}

static void set_workflow_state(const char *source, const char *destination)
{
	workflow_source = source ? source : "Not selected";
	workflow_destination = destination ? destination : "Not selected";
	UI_SetTransferState(workflow_source, workflow_destination);
}

static void workflow_status(char *status, size_t status_size)
{
	snprintf(status, status_size, "Source: %s   Destination: %s", workflow_source,
		workflow_destination);
}

int device_select()
{
	const char *items[DEV_TOTAL];
	u8 item_devices[DEV_TOTAL];
	int item_count = 0;
	int selected;
	int i;

	if (!DEVICES[DEV_NUM]) {
		UI_MessageError("Storage device", "No supported storage detected.",
			"Connect SD or USB storage and try again.");
		return 0;
	}
	if (DEVICES[DEV_NUM] == 1 && skip_selector) {
		skip_selector = 0;
		for (i = 1; i <= DEV_TOTAL; i++)
			if (DEVICES[i])
				return i;
	}

	for (i = 1; i <= DEV_TOTAL; i++) {
		if (DEVICES[i]) {
			items[item_count] = device_name(i);
			item_devices[item_count++] = i;
		}
	}
	skip_selector = 0;
	selected = UI_Menu("Storage device", "Choose the storage GCMM-EX should use",
		items, item_count, 0, NULL, true);
	return selected >= 0 && selected < item_count ? item_devices[selected] : 0;
}

static bool card_slot_is_reserved(int slot)
{
	return (slot == CARD_SLOTA && DEVICES[DEV_GCSDA] != DEV_ND) ||
		(slot == CARD_SLOTB && DEVICES[DEV_GCSDB] != DEV_ND);
}

static bool probe_memory_card(int slot)
{
	s32 result;
	s32 card_size = 0;
	s32 sector_size = 0;
	int tries = 0;

	if (card_slot_is_reserved(slot))
		return false;

	/* CARD_Probe only reports EXI presence, so an unselected SD Gecko would
	 * otherwise be mistaken for a memory card and fail later with
	 * CARD_ERROR_WRONGDEVICE. CARD_ProbeEx validates the device type and may
	 * remain busy briefly while a newly inserted device is debounced. */
	do {
		result = CARD_ProbeEx(slot, &card_size, &sector_size);
		if (result == CARD_ERROR_READY)
			return card_size > 0 && sector_size > 0;
		if (result != CARD_ERROR_BUSY)
			return false;
		VIDEO_WaitVSync();
		tries++;
	} while (tries < 30);

	return false;
}

static void card_status(char *status, size_t status_size, int slot)
{
	int save_count;
	u16 free_blocks;

	if (card_slot_is_reserved(slot)) {
		snprintf(status, status_size, "Card %c: unavailable (SD Gecko)",
			slot == CARD_SLOTA ? 'A' : 'B');
		return;
	}

	if (!probe_memory_card(slot)) {
		snprintf(status, status_size, "Card %c: not detected",
			slot == CARD_SLOTA ? 'A' : 'B');
		return;
	}
	if (MCardGetUsage(slot, &save_count, &free_blocks))
		snprintf(status, status_size, "Card %c: %d saves, %u free",
			slot == CARD_SLOTA ? 'A' : 'B', save_count, free_blocks);
	else
		snprintf(status, status_size, "Card %c: detected, unavailable",
			slot == CARD_SLOTA ? 'A' : 'B');
}

static void storage_status(char *status, size_t status_size)
{
	if (have_sd && CUR_DEVICE)
		snprintf(status, status_size, "Storage: %s (mounted)", device_name(CUR_DEVICE));
	else if (storage_mount_state == STORAGE_UNSUPPORTED_FILESYSTEM && CUR_DEVICE)
		snprintf(status, status_size, "Storage: %s (unsupported filesystem)",
			device_name(CUR_DEVICE));
	else if (storage_mount_state == STORAGE_MOUNT_FAILED && CUR_DEVICE)
		snprintf(status, status_size, "Storage: %s (mount failed)", device_name(CUR_DEVICE));
	else if (storage_mount_state == STORAGE_DETECTED)
		snprintf(status, status_size, "Storage: detected (not mounted)");
	else
		snprintf(status, status_size, "Storage: not detected");
}

static int select_memory_card(void)
{
	char labels[2][48];
	const char *items[2];
	int slots[2];
	bool enabled[2];
	bool detected;
	int count = 0;
	int detected_count = 0;
	int choice;
	int i;

	if (!card_slot_is_reserved(CARD_SLOTA)) {
		detected = probe_memory_card(CARD_SLOTA);
		snprintf(labels[count], sizeof(labels[count]), "Memory Card A (%s)",
			detected ? "detected" : "not detected");
		items[count] = labels[count];
		enabled[count] = detected;
		slots[count++] = CARD_SLOTA;
	}
	if (!card_slot_is_reserved(CARD_SLOTB)) {
		detected = probe_memory_card(CARD_SLOTB);
		snprintf(labels[count], sizeof(labels[count]), "Memory Card B (%s)",
			detected ? "detected" : "not detected");
		items[count] = labels[count];
		enabled[count] = detected;
		slots[count++] = CARD_SLOTB;
	}
	if (!count) {
		UI_MessageError("Memory card", "No accessible memory-card slots.",
			"Active SD Gecko occupies both available card slots.");
		return -1;
	}

	/* Report the empty case before opening the menu. Afterwards -1 cannot be
	 * told apart from the user simply pressing B, and going back is not an
	 * error. */
	for (i = 0; i < count; i++)
		if (enabled[i])
			detected_count++;
	if (!detected_count) {
		UI_MessageError("Memory card", "No detected memory cards are available.",
			"Insert a card or choose a different storage device.");
		return -1;
	}

	choice = UI_MenuDisabled("Memory card", "Choose source or destination card", items,
		enabled, count, 0, "Unavailable cards cannot be selected.", true);
	return choice >= 0 && choice < count ? slots[choice] : -1;
}

static int select_other_memory_card(int source_slot)
{
	const char *items[1];
	char label[48];
	int destination_slot;

	if (source_slot != CARD_SLOTA && source_slot != CARD_SLOTB)
		return -1;
	destination_slot = source_slot == CARD_SLOTA ? CARD_SLOTB : CARD_SLOTA;
	if (card_slot_is_reserved(destination_slot) || !probe_memory_card(destination_slot)) {
		UI_MessageError("Copy save", "No accessible destination memory card is detected.",
			"Insert the other card or choose a different storage device.");
		return -1;
	}
	snprintf(label, sizeof(label), "Memory Card %c (detected)",
		destination_slot == CARD_SLOTA ? 'A' : 'B');
	items[0] = label;
	return UI_Menu("Copy destination", "Choose destination memory card", items, 1,
		0, NULL, true) == 0 ? destination_slot : -1;
}

static bool require_storage(void)
{
	int device;

	if (have_sd)
		return true;

	detect_devices();
	for (;;) {
		/* An operation must always let the user confirm the storage target. */
		skip_selector = 0;
		device = device_select();
		if (!device)
			return false;

		CUR_DEVICE = device;
		have_sd = initFAT(CUR_DEVICE);
		if (have_sd)
			return true;

		UI_MessageError("Storage device", "Could not mount selected device.",
			"Check the device and choose another one, or press B to cancel.");
		detect_devices();
	}
}

static bool storage_entry_name_is_valid(const char *name)
{
	if (!name || !name[0] || strnlen(name, 1024) >= 1024)
		return false;
	return strcmp(name, ".") != 0 && strcmp(name, "..") != 0 &&
		strchr(name, '/') == NULL && strchr(name, '\\') == NULL;
}

static bool filelist_entry_is_valid(int index, int count)
{
	return count > 0 && count <= 1024 && index >= 0 && index < count &&
		storage_entry_name_is_valid((char *)filelist[index]);
}

static bool card_save_entry_is_valid(int index)
{
	return MCardIsValidSaveIndex(index) &&
		strnlen((char *)filelist[index], sizeof(filelist[index])) <
			sizeof(filelist[index]);
}

static void run_full_backup(void)
{
	int slot;
	int count;
	int i;
	int completed = 0;
	int failed = 0;
	int bytes;
	bool canceled = false;
	char review[96];
	char detail[96];
	char item[64];
	char result[80];
	u32 total_blocks = 0;
	bool size_known = true;
	card_direntry entry;
	char comments[65];

	if (!require_storage())
		return;
	slot = select_memory_card();
	if (slot < 0)
		return;
	MEM_CARD = slot;
	set_workflow_state(memory_card_name(slot), device_name(CUR_DEVICE));
	if (!probe_memory_card(slot)) {
		UI_MessageError("Back up memory card", "Selected memory card is not detected.",
			"Insert card and try again.");
		return;
	}
	count = CardGetDirectory(slot);
	if (count <= 0 || count > CARD_MAXFILES) {
		UI_Message("Back up memory card", "No saves found on this memory card.",
			"Card may be empty or unavailable.");
		return;
	}
	for (i = 0; i < count; i++) {
		if (!MCardGetSaveDetails(slot, i, &entry, comments) ||
			total_blocks > 0xffffffffU - entry.length) {
			size_known = false;
			break;
		}
		total_blocks += entry.length;
	}
	snprintf(review, sizeof(review), "Memory Card %c: %d saves to %s:/%s.",
		slot == CARD_SLOTA ? 'A' : 'B', count, fatpath, MCSAVES);
	if (size_known)
		snprintf(detail, sizeof(detail), "%u blocks, about %llu KiB. Each GCI is size-verified.",
			total_blocks, ((u64)total_blocks * 8192 + (u64)count * MCDATAOFFSET) / 1024);
	else
		snprintf(detail, sizeof(detail), "Size estimate unavailable. Each GCI is size-verified.");
	if (!UI_Confirm("Review backup", review, detail, "Start backup"))
		return;

	for (i = 0; i < count; i++) {
		if (!card_save_entry_is_valid(i)) {
			failed += count - i;
			break;
		}
		snprintf(item, sizeof(item), "Saving %d of %d: %.32s", i + 1, count,
			(char *)filelist[i]);
		UI_Progress("Creating backup", item, i, count);
		/* Cancellation is handled only between complete save writes. */
		if (UI_CancelRequested() && UI_Confirm("Cancel backup",
			"Stop after the saves already written?", "The current file has not started.",
			"Stop backup")) {
			canceled = true;
			break;
		}
		if (!probe_memory_card(slot)) {
			failed += count - i;
			break;
		}
		bytes = CardReadFile(slot, i);
		if (bytes <= 0 || !SDSaveMCImage())
			failed++;
		else
			completed++;
	}
	UI_Progress("Creating backup", "Finalizing backup", count, count);
	snprintf(result, sizeof(result), "%d saved, %d failed.", completed, failed);
	if (canceled)
		UI_Message("Backup canceled", "No file write was interrupted.", result);
	else if (failed && completed)
		UI_MessageError("Backup partial", "Some saves could not be backed up.", result);
	else if (failed)
		UI_MessageError("Backup failed", "No saves were backed up.", result);
	else
		UI_MessageSuccess("Backup complete", "Backup completed successfully.", result);
}

static bool storage_entry_is_folder(const char *name)
{
	char path[1024];
	int length;

	if (!storage_entry_name_is_valid(name) ||
		strnlen((char *)currFolder, sizeof(currFolder)) >= sizeof(currFolder))
		return false;
	length = snprintf(path, sizeof(path), "%s:/%s/%s", fatpath, currFolder, name);
	return length > 0 && (size_t)length < sizeof(path) && isdir_sd(path) == 1;
}

static bool enter_backup_folder(const char *name)
{
	char next_folder[sizeof(currFolder)];
	size_t used;
	size_t name_length;
	int length;

	if (!storage_entry_name_is_valid(name))
		return false;
	used = strnlen((char *)currFolder, sizeof(currFolder));
	name_length = strnlen(name, 1024);
	if (used >= sizeof(currFolder))
		return false;
	if (!name_length || name_length >= 1024)
		return false;
	length = snprintf(next_folder, sizeof(next_folder), "%s/%s", currFolder,
		name);
	if (length < 0 || (size_t)length >= sizeof(next_folder))
		return false;
	memcpy(currFolder, next_folder, (size_t)length + 1);
	return true;
}

static bool leave_backup_folder(void)
{
	char *separator;

	if (strnlen((char *)currFolder, sizeof(currFolder)) >= sizeof(currFolder))
		return false;
	if (!strcmp((char *)currFolder, MCSAVES))
		return false;
	separator = strrchr((char *)currFolder, '/');
	if (!separator)
		return false;
	*separator = '\0';
	return true;
}

static bool write_save_file(int slot);
static void clean_detail_text(char *out, size_t out_size, const u8 *in, size_t in_size);

static void update_restore_preview(int selected, int count, char title[65])
{
	if (!filelist_entry_is_valid(selected, count) ||
		storage_entry_is_folder((char *)filelist[selected]) ||
		!SDLoadMCImageHeader((char *)filelist[selected])) {
		UI_ClearSavePreview();
		return;
	}
	clean_detail_text(title, 65, (const u8 *)CommentBuffer, sizeof(CommentBuffer));
	UI_SetSavePreview(gci.banner_fmt, bannerdata, bannerdataCI, tlutbanner, title,
		"Banner from backup file");
}

static void restore_backup_file(const char *filename)
{
	card_direntry entry;
	int slot;
	char review[96];
	char detail[80];

	if (!storage_entry_name_is_valid(filename) || !SDLoadMCImageHeader((char *)filename)) {
		UI_MessageError("Restore backup", "Backup file is invalid or unreadable.",
			"Only complete GCI, GCS, and SAV files can be restored.");
		return;
	}
	set_workflow_state(device_name(CUR_DEVICE), NULL);
	memcpy(&entry, &gci, sizeof(entry));
	slot = select_memory_card();
	if (slot < 0)
		return;
	MEM_CARD = slot;
	set_workflow_state(device_name(CUR_DEVICE), memory_card_name(slot));
	if (!probe_memory_card(slot)) {
		UI_MessageError("Restore backup", "Selected memory card is not detected.",
			"Insert card and try again.");
		return;
	}
	snprintf(review, sizeof(review), "%.48s to Memory Card %c.", filename,
		slot == CARD_SLOTA ? 'A' : 'B');
	snprintf(detail, sizeof(detail),
		"Size: %u blocks. Existing matching save may be overwritten.", entry.length);
	if (!UI_Confirm("Review restore", review, detail, "Start restore"))
		return;
	UI_Progress("Restoring backup", filename, 0, 1);
	if (!probe_memory_card(slot)) {
		UI_MessageError("Restore backup", "Destination memory card was removed.",
			"Reinsert the card and start the restore again.");
		return;
	}
	if (!SDLoadMCImage((char *)filename)) {
		UI_MessageError("Restore backup", "Could not read the complete backup file.",
			"Check the storage device and backup file.");
		return;
	}
	if (!write_save_file(slot)) {
		UI_MessageError("Restore backup", "Could not write the destination memory card.",
			"Check available blocks, overwrite prompts, and card connection.");
		return;
	}
	UI_Progress("Restoring backup", filename, 1, 1);
	UI_MessageSuccess("Restore complete", "Backup restored successfully.",
		"Do not remove either device until this message is shown.");
}

static bool write_save_file(int slot)
{
	char message[96];
	int result;

	result = CardWriteFile(slot, 0);
	if (result == MCARD_WRITE_OK)
		return true;
	if (result != MCARD_WRITE_EXISTS)
		return false;
	snprintf(message, sizeof(message), "Save %.40s already exists on Memory Card %c.",
		(char *)gci.filename, slot == CARD_SLOTA ? 'A' : 'B');
	if (!UI_ConfirmDestructive("Overwrite existing save", message,
		"The existing save will be permanently replaced.", "Overwrite save"))
		return false;
	return CardWriteFile(slot, 1) == MCARD_WRITE_OK;
}

static void run_restore_backup(void)
{
	int count;
	int selected = 0;
	char subtitle[96];
	char preview_title[65];
	ui_list_action action;

	if (!require_storage())
		return;
	snprintf((char *)currFolder, sizeof(currFolder), "%s", MCSAVES);
	for (;;) {
		count = SDGetFileList(1);
		if (count <= 0) {
			UI_Message("Restore backup", "No supported backups in this folder.",
				"Use GCI, GCS, or SAV files in MCBACKUP.");
			if (!leave_backup_folder())
				return;
			continue;
		}
		if (count > 1024)
			count = 1024;
		snprintf(subtitle, sizeof(subtitle), "%s: %.62s", device_name(CUR_DEVICE),
			(char *)currFolder);
		update_restore_preview(selected, count, preview_title);
		action = UI_SaveList("Restore backup", subtitle, filelist, count,
			&selected, NULL, -1, true);
		if (action == UI_LIST_BACK) {
			if (!leave_backup_folder())
				return;
			selected = 0;
			continue;
		}
		if (action == UI_LIST_PREVIEW)
			continue;
		if (action != UI_LIST_OPEN)
			continue;
		if (!filelist_entry_is_valid(selected, count))
			continue;
		if (storage_entry_is_folder((char *)filelist[selected])) {
			if (!enter_backup_folder((char *)filelist[selected]))
				UI_MessageError("Restore backup", "Folder path is too long.",
					"Choose a folder with a shorter path.");
			selected = 0;
			continue;
		}
		restore_backup_file((char *)filelist[selected]);
	}
}

static void clean_detail_text(char *out, size_t out_size, const u8 *in, size_t in_size)
{
	size_t i;
	size_t used = 0;

	if (!out_size)
		return;
	for (i = 0; i < in_size && used + 1 < out_size; i++) {
		if (in[i] == '\0' || in[i] == '\n' || in[i] == '\r') {
			if (used && out[used - 1] != ' ')
				out[used++] = ' ';
		} else if (in[i] >= 32 && in[i] <= 126) {
			out[used++] = (char)in[i];
		}
	}
	while (used && out[used - 1] == ' ')
		used--;
	out[used] = '\0';
}

/* Card images run to megabytes, so a raw byte count reads as noise. */
static void format_transfer_size(s32 bytes, char *out, size_t out_size)
{
	if (bytes < 0)
		bytes = 0;
	if (bytes >= 1024 * 1024)
		snprintf(out, out_size, "%d.%d MB", bytes / (1024 * 1024),
			(bytes % (1024 * 1024)) * 10 / (1024 * 1024));
	else if (bytes >= 1024)
		snprintf(out, out_size, "%d KB", bytes / 1024);
	else
		snprintf(out, out_size, "%d bytes", bytes);
}

static void format_save_datetime(u32 timestamp, char *out, size_t out_size)
{
	static const char *const months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	u64 seconds = (u64)timestamp + 946684800ULL;
	u64 days = seconds / 86400;
	u64 l = days + 2509157;
	u64 n = 4 * l / 146097;
	u64 i;
	u64 j;
	u64 day;
	u64 month;
	u64 year;

	l -= (146097 * n + 3) / 4;
	i = 4000 * (l + 1) / 1461001;
	l = l - 1461 * i / 4 + 31;
	j = 80 * l / 2447;
	day = l - 2447 * j / 80;
	l = j / 11;
	month = j + 2 - 12 * l;
	year = 100 * (n - 49) + i + l;
	if (month < 1 || month > 12) {
		snprintf(out, out_size, "Unavailable");
		return;
	}
	snprintf(out, out_size, "%s %02llu, %04llu  %02llu:%02llu:%02llu",
		months[month - 1], day, year, (seconds / 3600) % 24,
		(seconds / 60) % 60, seconds % 60);
}

/* Loads the banner for one card save. title backs the stored preview text and
 * must outlive every screen drawn while the preview stays set. */
static void update_save_preview(int slot, int selected, char title[65])
{
	card_direntry preview_entry;
	char preview_comments[65];

	if (MCardLoadSavePreview(slot, selected, &preview_entry, preview_comments)) {
		clean_detail_text(title, 65, (const u8 *)preview_comments,
			sizeof(preview_comments) - 1);
		UI_SetSavePreview(preview_entry.banner_fmt, bannerdata, bannerdataCI,
			tlutbanner, title, "Banner from memory card");
	} else {
		title[0] = '\0';
		UI_ClearSavePreview();
	}
}

static void show_save_details(int slot, int selected)
{
	ui_field fields[6];
	card_direntry entry;
	char comments[65];
	char title[33];
	char description[33];
	char internal_name[CARD_FILENAMELEN + 1];
	char gamecode[5];
	char company[3];
	char blocks[16];
	char copies[16];
	char date[64];
	char permissions[48];
	char subtitle[32];
	char preview_title[65];

	if (!card_save_entry_is_valid(selected) ||
		!MCardGetSaveDetails(slot, selected, &entry, comments))
		return;
	/* The comment block holds two fixed 32-byte fields: title, then note. */
	clean_detail_text(title, sizeof(title), (const u8 *)comments, 32);
	clean_detail_text(description, sizeof(description), (const u8 *)comments + 32, 32);
	clean_detail_text(internal_name, sizeof(internal_name), entry.filename,
		sizeof(entry.filename));
	memcpy(gamecode, entry.gamecode, 4);
	gamecode[4] = '\0';
	memcpy(company, entry.company, 2);
	company[2] = '\0';
	format_save_datetime(entry.last_modified, date, sizeof(date));
	snprintf(blocks, sizeof(blocks), "%u", entry.length);
	snprintf(copies, sizeof(copies), "%u", entry.copy_times);
	snprintf(permissions, sizeof(permissions), "%s%s%s",
		(entry.permission & 16) ? "" : "No move / ",
		(entry.permission & 8) ? "" : "No copy / ",
		(entry.permission & 4) ? "Public" : "Private");
	snprintf(subtitle, sizeof(subtitle), "Memory Card %c",
		slot == CARD_SLOTA ? 'A' : 'B');

	fields[0].label = "GAME CODE";
	fields[0].value = gamecode;
	fields[1].label = "COMPANY";
	fields[1].value = company;
	fields[2].label = "BLOCKS";
	fields[2].value = blocks;
	fields[3].label = "COPIES";
	fields[3].value = copies;
	fields[4].label = "MODIFIED";
	fields[4].value = date;
	fields[5].label = "PERMISSIONS";
	fields[5].value = permissions;

	update_save_preview(slot, selected, preview_title);
	UI_SaveDetails(subtitle, title[0] ? title : (char *)filelist[selected],
		description, internal_name, fields, 6);
	/* preview_title dies with this frame, so drop the stored pointer too. */
	UI_ClearSavePreview();
}

static int marked_save_count(const bool *marked, int count)
{
	int i;
	int marked_count = 0;

	if (!marked || count < 0 || count > CARD_MAXFILES)
		return 0;
	for (i = 0; i < count; i++)
		if (marked[i])
			marked_count++;
	return marked_count;
}

static bool selected_save_blocks(int slot, const bool *marked, int count,
	u32 *blocks)
{
	card_direntry entry;
	char comments[65];
	u32 total = 0;
	int i;

	if (!marked || !blocks || count < 0 || count > CARD_MAXFILES)
		return false;
	for (i = 0; i < count; i++) {
		if (!marked[i])
			continue;
		if (!MCardGetSaveDetails(slot, i, &entry, comments) ||
			total > 0xffffffffU - entry.length)
			return false;
		total += entry.length;
	}
	*blocks = total;
	return true;
}

static bool backup_one_save(int slot, int id)
{
	card_direntry entry;
	char comments[65];
	char review[96];
	char detail[80];

	if (!card_save_entry_is_valid(id) || !require_storage() || !probe_memory_card(slot))
		return false;
	set_workflow_state(memory_card_name(slot), device_name(CUR_DEVICE));
	if (!MCardGetSaveDetails(slot, id, &entry, comments)) {
		UI_MessageError("Back up save", "Could not read selected save details.",
			"No backup was created.");
		return false;
	}
	snprintf(review, sizeof(review), "%.48s from Memory Card %c to %s.",
		(char *)filelist[id], slot == CARD_SLOTA ? 'A' : 'B', device_name(CUR_DEVICE));
	snprintf(detail, sizeof(detail), "Size: %u blocks. Output will be size-verified.",
		entry.length);
	if (!UI_Confirm("Review save backup", review, detail, "Start backup"))
		return false;
	UI_Progress("Backing up save", (char *)filelist[id], 0, 1);
	if (!probe_memory_card(slot) || !CardReadFile(slot, id) || !SDSaveMCImage()) {
		UI_MessageError("Back up save", "Backup failed.",
			"Check memory card, storage space, and filesystem.");
		return false;
	}
	UI_Progress("Backing up save", (char *)filelist[id], 1, 1);
	UI_MessageSuccess("Back up save", "Backup completed successfully.",
		"GCI size verified on storage.");
	return true;
}

static bool copy_save_to_card(int source_slot, int id)
{
	int destination_slot;
	card_direntry entry;
	char comments[65];
	char review[96];
	char detail[80];

	if (!card_save_entry_is_valid(id) || !probe_memory_card(source_slot) ||
		!MCardGetSaveDetails(source_slot, id, &entry, comments))
		return false;
	destination_slot = select_other_memory_card(source_slot);
	if (destination_slot < 0)
		return false;
	if (!probe_memory_card(destination_slot)) {
		UI_MessageError("Copy save", "Destination memory card is not detected.",
			"Insert destination card and try again.");
		return false;
	}
	set_workflow_state(memory_card_name(source_slot), memory_card_name(destination_slot));
	snprintf(review, sizeof(review), "%.48s: Memory Card %c to Memory Card %c.",
		(char *)filelist[id], source_slot == CARD_SLOTA ? 'A' : 'B',
		destination_slot == CARD_SLOTA ? 'A' : 'B');
	snprintf(detail, sizeof(detail), "Size: %u blocks. Destination save may be overwritten.",
		entry.length);
	if (!UI_Confirm("Review copy", review, detail, "Start copy"))
		return false;
	UI_Progress("Copying save", (char *)filelist[id], 0, 3);
	if (!probe_memory_card(source_slot) || !CardReadFile(source_slot, id))
		return false;
	UI_Progress("Copying save", "Writing destination memory card", 1, 3);
	if (!probe_memory_card(destination_slot) || !write_save_file(destination_slot))
		return false;
	UI_Progress("Copying save", "Verifying destination memory card", 2, 3);
	if (!probe_memory_card(destination_slot) || !MCardVerifyLastWrite(destination_slot))
		return false;
	UI_Progress("Copying save", (char *)filelist[id], 3, 3);
	UI_MessageSuccess("Copy complete", "Save copied successfully.",
		"Source save was not modified.");
	return true;
}

static void copy_marked_saves(int source_slot, const bool *marked, int count)
{
	int i;
	int selected_count = marked_save_count(marked, count);
	int destination_slot;
	int completed = 0;
	int failed = 0;
	int skipped = 0;
	int current = 0;
	u32 total_blocks;
	char review[96];
	char detail[96];
	char item[64];
	char error[96];
	char result[80];

	if (!selected_count || !probe_memory_card(source_slot))
		return;
	if (!selected_save_blocks(source_slot, marked, count, &total_blocks)) {
		UI_MessageError("Copy selected saves", "Could not read selected save metadata.",
			"No saves were copied.");
		return;
	}
	destination_slot = select_other_memory_card(source_slot);
	if (destination_slot < 0)
		return;
	set_workflow_state(memory_card_name(source_slot), memory_card_name(destination_slot));
	snprintf(review, sizeof(review), "%d saves: Memory Card %c to Memory Card %c.",
		selected_count, source_slot == CARD_SLOTA ? 'A' : 'B',
		destination_slot == CARD_SLOTA ? 'A' : 'B');
	snprintf(detail, sizeof(detail), "%u blocks total. Destination saves may be overwritten.",
		total_blocks);
	if (!UI_Confirm("Review selected copy", review, detail, "Start copy"))
		return;

	for (i = 0; i < count; i++) {
		if (!marked[i])
			continue;
		current++;
		if (!card_save_entry_is_valid(i)) {
			failed++;
			continue;
		}
		snprintf(item, sizeof(item), "Copying %d of %d: %.24s", current, selected_count,
			(char *)filelist[i]);
		UI_Progress("Copying selected saves", item, current - 1, selected_count);
		if (!probe_memory_card(source_slot) || !CardReadFile(source_slot, i) ||
			!probe_memory_card(destination_slot) || !write_save_file(destination_slot)) {
			failed++;
			snprintf(error, sizeof(error), "Could not copy %.42s.", (char *)filelist[i]);
			if (!UI_Confirm("Copy error", error, NULL, "Continue")) {
				skipped = selected_count - current;
				break;
			}
		} else {
			completed++;
		}
	}
	UI_Progress("Copying selected saves", "Finalizing copy", selected_count, selected_count);
	snprintf(result, sizeof(result), "%d copied, %d failed, %d skipped.", completed,
		failed, skipped);
	if (skipped)
		UI_Message("Selected copy stopped", "Remaining selected saves were skipped.", result);
	else if (failed && completed)
		UI_MessageError("Selected copy partial", "Some selected saves could not be copied.", result);
	else if (failed)
		UI_MessageError("Selected copy failed", "No selected saves were copied.", result);
	else
		UI_MessageSuccess("Selected copy complete", "All selected saves were copied.", result);
}

static void backup_marked_saves(int slot, const bool *marked, int count)
{
	int i;
	int selected_count = marked_save_count(marked, count);
	int completed = 0;
	int failed = 0;
	int skipped = 0;
	int current = 0;
	u32 total_blocks;
	char review[96];
	char detail[96];
	char item[64];
	char result[80];
	char error[96];

	if (!selected_count || !require_storage() || !probe_memory_card(slot))
		return;
	set_workflow_state(memory_card_name(slot), device_name(CUR_DEVICE));
	if (!selected_save_blocks(slot, marked, count, &total_blocks)) {
		UI_MessageError("Selected backup", "Could not read selected save metadata.",
			"No backup was created.");
		return;
	}
	snprintf(review, sizeof(review), "%d saves from Memory Card %c to %s.",
		selected_count, slot == CARD_SLOTA ? 'A' : 'B', device_name(CUR_DEVICE));
	snprintf(detail, sizeof(detail), "%u blocks total. Each GCI output is size-verified.",
		total_blocks);
	if (!UI_Confirm("Review selected backup", review, detail, "Start backup"))
		return;
	for (i = 0; i < count; i++) {
		if (!marked[i])
			continue;
		current++;
		if (!card_save_entry_is_valid(i)) {
			failed++;
			continue;
		}
		snprintf(item, sizeof(item), "Saving %d of %d: %.24s", current, selected_count,
			(char *)filelist[i]);
		UI_Progress("Backing up selected saves", item, current - 1, selected_count);
		if (!probe_memory_card(slot) || !CardReadFile(slot, i) || !SDSaveMCImage()) {
			failed++;
			snprintf(error, sizeof(error), "Could not back up %.42s.",
				(char *)filelist[i]);
			if (!UI_Confirm("Backup error", error, NULL, "Continue")) {
				skipped = selected_count - current;
				break;
			}
		} else {
			completed++;
		}
	}
	UI_Progress("Backing up selected saves", "Finalizing backup", selected_count, selected_count);
	snprintf(result, sizeof(result), "%d saved, %d failed, %d skipped.", completed,
		failed, skipped);
	if (skipped)
		UI_Message("Selected backup stopped", "Remaining selected saves were skipped.", result);
	else if (failed && completed)
		UI_MessageError("Selected backup partial", "Some saves could not be backed up.", result);
	else if (failed)
		UI_MessageError("Selected backup failed", "No selected saves were backed up.", result);
	else
		UI_MessageSuccess("Selected backup complete", "All selected saves were backed up.", result);
}

static void move_save_to_card(int source_slot, int id)
{
	char message[96];

	if (!card_save_entry_is_valid(id))
		return;
	snprintf(message, sizeof(message), "%.42s will be copied, then deleted from Memory Card %c.",
		(char *)filelist[id], source_slot == CARD_SLOTA ? 'A' : 'B');
	if (!UI_ConfirmDestructive("Move save", message,
		"Source is deleted only after the destination copy succeeds.", "Start move"))
		return;
	if (!copy_save_to_card(source_slot, id)) {
		UI_MessageError("Move save", "Move stopped before source deletion.",
			"Source save remains unchanged.");
		return;
	}
	if (!probe_memory_card(source_slot) || !MCardDeleteFile(source_slot, id)) {
		UI_MessageError("Move partial", "Copy succeeded, but source deletion failed.",
			"Both copies may now exist. Verify cards before retrying.");
		return;
	}
	UI_MessageSuccess("Move complete", "Save copied and source deleted.", NULL);
}

static int delete_marked_saves(int slot, bool *marked, int count)
{
	char message[64];
	char detail[96];
	char result[80];
	char item[64];
	char error[96];
	int i;
	int selected_count = marked_save_count(marked, count);
	int deleted = 0;
	int failed = 0;
	int skipped = 0;
	int current = 0;
	u32 total_blocks;

	if (!marked || count < 0 || count > CARD_MAXFILES || !selected_count)
		return 0;
	if (!probe_memory_card(slot)) {
		UI_MessageError("Delete selected saves", "Selected memory card is not detected.",
			"Insert card and try again.");
		return -1;
	}
	if (!selected_save_blocks(slot, marked, count, &total_blocks)) {
		UI_MessageError("Delete selected saves", "Could not read selected save metadata.",
			"No data was deleted.");
		return -1;
	}
	snprintf(message, sizeof(message), "%d selected saves will be permanently deleted.",
		selected_count);
	snprintf(detail, sizeof(detail), "%u blocks will be deleted. This action cannot be undone.",
		total_blocks);
	if (!UI_ConfirmDestructive("Delete selected saves", message, detail, "Delete selected saves"))
		return 0;

	/* Descending order avoids directory-index drift while entries are deleted. */
	for (i = count - 1; i >= 0; i--) {
		if (!marked[i])
			continue;
		current++;
		if (!card_save_entry_is_valid(i)) {
			failed++;
			continue;
		}
		snprintf(item, sizeof(item), "Deleting %d of %d: %.24s", current,
			selected_count, (char *)filelist[i]);
		UI_Progress("Deleting selected saves", item, current - 1, selected_count);
		if (!probe_memory_card(slot) || !MCardDeleteFile(slot, i)) {
			failed++;
			snprintf(error, sizeof(error), "Could not delete %.42s.",
				(char *)filelist[i]);
			if (!UI_Confirm("Delete error", error, NULL, "Continue")) {
				skipped = selected_count - current;
				break;
			}
		} else {
			deleted++;
		}
	}
	UI_Progress("Deleting selected saves", "Finalizing deletion", selected_count, selected_count);
	snprintf(result, sizeof(result), "%d deleted, %d failed, %d skipped.", deleted,
		failed, skipped);
	if (failed)
		UI_MessageError("Delete selected saves",
			"Some selected saves could not be deleted.", result);
	else if (skipped)
		UI_Message("Delete selected saves", "Batch delete stopped.", result);
	else
		UI_MessageSuccess("Delete selected saves",
			"All selected saves were deleted.", result);
	return 1;
}

static int add_action(ui_menu_item items[], int count, const char *label,
	bool enabled, bool destructive)
{
	items[count].label = label;
	items[count].enabled = enabled;
	items[count].destructive = destructive;
	items[count].heading = false;
	return count + 1;
}

static int add_action_heading(ui_menu_item items[], int count, const char *label)
{
	items[count].label = label;
	items[count].enabled = false;
	items[count].destructive = false;
	items[count].heading = true;
	return count + 1;
}

static void run_manage_saves(void)
{
	ui_menu_item actions[UI_ACTION_MAX_ITEMS];
	static const char *const sources[] = {
		"Memory card",
		"Mounted storage backups"
	};
	char marked_heading[32];
	char marked_copy[32];
	char marked_backup[32];
	char marked_delete[32];
	int at_details;
	int at_copy;
	int at_move;
	int at_backup;
	int at_delete;
	int at_marked_copy;
	int at_marked_backup;
	int at_marked_clear;
	int at_marked_delete;
	int action_count;
	bool other_card_ready;
	bool marked[CARD_MAXFILES] = { false };
	char subtitle[64];
	int slot;
	int count;
	int usage_count;
	u16 free_blocks;
	int selected = 0;
	int action;
	int selected_count;
	int batch_result;
	card_direntry selected_entry;
	char delete_message[96];
	char delete_detail[96];
	char preview_title[65] = "";
	ui_list_action result;

	if (have_sd) {
		int source = UI_Menu("Manage saves", "Choose a source", sources, 2, 0,
			NULL, true);

		if (source < 0)
			return;
		if (source == 1) {
			set_workflow_state(device_name(CUR_DEVICE), NULL);
			run_restore_backup();
			return;
		}
	}

	slot = select_memory_card();
	if (slot < 0)
		return;
	MEM_CARD = slot;
	set_workflow_state(memory_card_name(slot), NULL);
	if (!probe_memory_card(slot)) {
		UI_MessageError("Manage saves", "Selected memory card is not detected.",
			"Insert card and try again.");
		return;
	}

	count = CardGetDirectory(slot);
	if (count <= 0 || count > CARD_MAXFILES) {
		UI_Message("Manage saves", "No saves found on this memory card.",
			"Card may be empty or unavailable.");
		return;
	}
	if (MCardGetUsage(slot, &usage_count, &free_blocks))
		snprintf(subtitle, sizeof(subtitle), "Memory Card %c - %d saves, %u free blocks",
			slot == CARD_SLOTA ? 'A' : 'B', count, free_blocks);
	else
		snprintf(subtitle, sizeof(subtitle), "Memory Card %c - %d saves, usage unavailable",
			slot == CARD_SLOTA ? 'A' : 'B', count);

	for (;;) {
		update_save_preview(slot, selected, preview_title);
		result = UI_SaveList("Manage saves", subtitle, filelist, count, &selected,
			marked, slot, true);
		if (result == UI_LIST_DEVICE_REMOVED) {
			UI_MessageError("Manage saves", "Selected memory card was removed.",
				"No save operation was started.");
			return;
		}
		if (result == UI_LIST_BACK)
			return;
		if (result == UI_LIST_PREVIEW)
			continue;
		if (result == UI_LIST_PREVIOUS_DEVICE || result == UI_LIST_NEXT_DEVICE) {
			int next_slot = slot == CARD_SLOTA ? CARD_SLOTB : CARD_SLOTA;

			if (card_slot_is_reserved(next_slot) || !probe_memory_card(next_slot)) {
				UI_MessageError("Manage saves", "Other memory-card slot is unavailable.",
					"Insert an accessible card or return to choose a device.");
				continue;
			}
			slot = next_slot;
			MEM_CARD = slot;
			set_workflow_state(memory_card_name(slot), NULL);
			count = CardGetDirectory(slot);
			if (count <= 0 || count > CARD_MAXFILES) {
				UI_Message("Manage saves", "No saves found on this memory card.",
					"Card may be empty or unavailable.");
				return;
			}
			selected = 0;
			memset(marked, 0, sizeof(marked));
			if (MCardGetUsage(slot, &usage_count, &free_blocks))
				snprintf(subtitle, sizeof(subtitle), "Memory Card %c - %d saves, %u free blocks",
					slot == CARD_SLOTA ? 'A' : 'B', count, free_blocks);
			else
				snprintf(subtitle, sizeof(subtitle), "Memory Card %c - %d saves, usage unavailable",
					slot == CARD_SLOTA ? 'A' : 'B', count);
			continue;
		}
		if (result == UI_LIST_OPEN) {
			if (!card_save_entry_is_valid(selected) || selected >= count)
				continue;
			show_save_details(slot, selected);
			continue;
		}
		if (result != UI_LIST_CONTEXT) {
			continue;
		}
		if (!card_save_entry_is_valid(selected) || selected >= count)
			continue;

		selected_count = marked_save_count(marked, count);
		other_card_ready = !card_slot_is_reserved(slot == CARD_SLOTA ? CARD_SLOTB : CARD_SLOTA) &&
			probe_memory_card(slot == CARD_SLOTA ? CARD_SLOTB : CARD_SLOTA);

		action_count = add_action_heading(actions, 0, "THIS SAVE");
		at_details = action_count;
		action_count = add_action(actions, action_count, "Details", true, false);
		at_copy = action_count;
		action_count = add_action(actions, action_count, "Copy to memory card",
			other_card_ready, false);
		at_move = action_count;
		action_count = add_action(actions, action_count, "Move to memory card",
			other_card_ready, false);
		at_backup = action_count;
		action_count = add_action(actions, action_count, "Back up to storage",
			have_sd, false);
		at_delete = action_count;
		action_count = add_action(actions, action_count, "Delete save", true, true);
		at_marked_copy = -1;
		at_marked_backup = -1;
		at_marked_clear = -1;
		at_marked_delete = -1;
		if (selected_count) {
			/* The count belongs on screen at the moment the batch is chosen. */
			snprintf(marked_heading, sizeof(marked_heading), "%d MARKED SAVES",
				selected_count);
			snprintf(marked_copy, sizeof(marked_copy), "Copy %d marked saves",
				selected_count);
			snprintf(marked_backup, sizeof(marked_backup), "Back up %d marked saves",
				selected_count);
			snprintf(marked_delete, sizeof(marked_delete), "Delete %d marked saves",
				selected_count);
			action_count = add_action_heading(actions, action_count, marked_heading);
			at_marked_copy = action_count;
			action_count = add_action(actions, action_count, marked_copy,
				other_card_ready, false);
			at_marked_backup = action_count;
			action_count = add_action(actions, action_count, marked_backup,
				have_sd, false);
			at_marked_clear = action_count;
			action_count = add_action(actions, action_count, "Clear marks", true, false);
			/* Destructive last in its group, furthest from the benign rows. */
			at_marked_delete = action_count;
			action_count = add_action(actions, action_count, marked_delete, true, true);
		}
		action = UI_ActionMenu("Save actions", preview_title[0] ? preview_title :
			(char *)filelist[selected], actions, action_count, at_details,
			"Unavailable actions need storage or the other card.");
		if (action < 0)
			continue;
		if (action == at_details) {
			show_save_details(slot, selected);
			continue;
		}
		if (action == at_copy) {
			copy_save_to_card(slot, selected);
			continue;
		}
		if (action == at_move) {
			move_save_to_card(slot, selected);
			count = CardGetDirectory(slot);
			if (count <= 0 || count > CARD_MAXFILES)
				return;
			if (selected >= count)
				selected = count - 1;
			continue;
		}
		if (action == at_backup) {
			backup_one_save(slot, selected);
			continue;
		}
		if (action == at_marked_copy) {
			copy_marked_saves(slot, marked, count);
			continue;
		}
		if (action == at_marked_backup) {
			backup_marked_saves(slot, marked, count);
			continue;
		}
		if (action == at_marked_clear) {
			memset(marked, 0, sizeof(marked));
			continue;
		}
		if (action == at_marked_delete) {
			batch_result = delete_marked_saves(slot, marked, count);
			if (batch_result < 0)
				return;
			if (batch_result == 0)
				continue;
			count = CardGetDirectory(slot);
			if (count <= 0 || count > CARD_MAXFILES) {
				UI_Message("Manage saves", "No saves remain on this memory card.", NULL);
				return;
			}
			if (selected >= count)
				selected = count - 1;
			memset(marked, 0, sizeof(marked));
			if (MCardGetUsage(slot, &usage_count, &free_blocks))
				snprintf(subtitle, sizeof(subtitle), "Memory Card %c - %d saves, %u free blocks",
					slot == CARD_SLOTA ? 'A' : 'B', count, free_blocks);
			else
				snprintf(subtitle, sizeof(subtitle), "Memory Card %c - %d saves, usage unavailable",
					slot == CARD_SLOTA ? 'A' : 'B', count);
			continue;
		}
		/* Deleting is the only remaining action, and the only one that can
		 * destroy data. Match it explicitly rather than by falling through. */
		if (action != at_delete)
			continue;
		if (!probe_memory_card(slot)) {
			UI_MessageError("Delete save", "Selected memory card is not detected.",
				"Insert card and try again.");
			return;
		}
		if (!MCardGetSaveDetails(slot, selected, &selected_entry, delete_message)) {
			UI_MessageError("Delete save", "Could not read selected save details.",
			"No data was deleted.");
			return;
		}
		snprintf(delete_message, sizeof(delete_message),
			"Delete %.48s from Memory Card %c?", (char *)filelist[selected],
			slot == CARD_SLOTA ? 'A' : 'B');
		snprintf(delete_detail, sizeof(delete_detail),
			"Size: %u blocks. This action cannot be undone.", selected_entry.length);
		if (!UI_ConfirmDestructive("Delete save", delete_message, delete_detail, "Delete save"))
			continue;
		if (!MCardDeleteFile(slot, selected))
			return;
		UI_MessageSuccess("Delete save", "Save deleted.", "Directory will now refresh.");
		count = CardGetDirectory(slot);
		if (count <= 0 || count > CARD_MAXFILES) {
			UI_Message("Manage saves", "No saves remain on this memory card.", NULL);
			return;
		}
		if (selected >= count)
			selected = count - 1;
		memset(marked, 0, sizeof(marked));
		if (MCardGetUsage(slot, &usage_count, &free_blocks))
			snprintf(subtitle, sizeof(subtitle), "Memory Card %c - %d saves, %u free blocks",
				slot == CARD_SLOTA ? 'A' : 'B', count, free_blocks);
		else
			snprintf(subtitle, sizeof(subtitle), "Memory Card %c - %d saves, usage unavailable",
			slot == CARD_SLOTA ? 'A' : 'B', count);
	}
}

static void run_advanced_menu(void)
{
	static const char *const items[] = {
		"Full memory-card RAW backup",
		"Restore RAW/GCP/MCI image",
		"Format memory card"
	};
	int choice;
	int slot;
	char format_message[80];

	for (;;) {
		choice = UI_Menu("Advanced options", "High-risk operations", items, 3, 0,
			NULL, true);
		if (choice < 0)
			return;
		/* Formatting only touches the card, so it must not ask for storage.
		 * Test the choice first: require_storage prompts as a side effect. */
		if (choice != 2 && !require_storage())
			continue;

		slot = select_memory_card();
		if (slot < 0)
			continue;
		MEM_CARD = slot;
		if (choice == 0) {
			char message[96];

			set_workflow_state(memory_card_name(slot), device_name(CUR_DEVICE));

			snprintf(message, sizeof(message),
				"Create a complete RAW image of Memory Card %c on %s.",
				slot == CARD_SLOTA ? 'A' : 'B', device_name(CUR_DEVICE));
			if (UI_Confirm("Review RAW backup", message,
				"The card is read-only during this operation.", "Start RAW backup"))
				SD_RawBackupMode();
		}
		else if (choice == 1) {
			set_workflow_state(device_name(CUR_DEVICE), memory_card_name(slot));
			SD_RawRestoreMode();
		}
		else if (!probe_memory_card(slot)) {
			UI_MessageError("Format memory card", "Selected memory card is not detected.",
				"Insert card and try again.");
		} else {
			set_workflow_state(memory_card_name(slot), "Not applicable");
			snprintf(format_message, sizeof(format_message),
				"All saves on Memory Card %c will be erased.",
				slot == CARD_SLOTA ? 'A' : 'B');
			if (!UI_ConfirmDestructive("Format memory card", format_message,
				"Select FORMAT MEMORY CARD only if you intend to erase all data.",
				"FORMAT MEMORY CARD"))
				continue;
			if (!probe_memory_card(slot)) {
				UI_MessageError("Format memory card", "Selected memory card is no longer detected.",
					"Insert the card and start the operation again.");
				continue;
			}
			UI_Progress("Formatting memory card", "Formatting. Do not remove the card.", 0, 1);
			if (MCardFormat(slot))
				UI_MessageSuccess("Format complete", "Memory card formatted successfully.", "Card remounted and verified.");
			else
				UI_MessageError("Format failed", "Memory card could not be formatted.",
					"No further changes were made by GCMM-EX.");
		}
	}
}

static void show_information(void)
{
	UI_About("Alex Ishida", "GCMM by suloku");
}

static void run_others_menu(void)
{
	static const char *const items[] = {
		"Storage devices",
		"Advanced options",
		"Controls and help",
		"About"
	};
	int choice;
	int device;

	for (;;) {
		choice = UI_Menu("Others", "Additional GCMM-EX options", items, 4, 0,
			NULL, true);
		if (choice < 0)
			return;
		if (choice == 0) {
			int previous_device = CUR_DEVICE;
			bool previous_mounted = have_sd;

			if (previous_mounted)
				deinitFAT();
			detect_devices();
			device = device_select();
			if (device) {
				CUR_DEVICE = device;
				have_sd = initFAT(CUR_DEVICE);
				if (!have_sd)
					UI_MessageError("Storage device", "Could not mount selected device.",
						"Previous storage will be restored when available.");
			}
			if (!have_sd && previous_mounted && previous_device &&
				DEVICES[previous_device] == DEV_AVAIL) {
				CUR_DEVICE = previous_device;
				have_sd = initFAT(CUR_DEVICE);
			}
		} else if (choice == 1) {
			run_advanced_menu();
		} else if (choice == 2) {
			UI_Help();
		} else {
			show_information();
		}
	}
}

static void run_home_menu(void)
{
	char card_a[64];
	char card_b[64];
	char storage[96];
	char transfer[128];
	int choice;

	for (;;) {
		freecardbuf();
		card_status(card_a, sizeof(card_a), CARD_SLOTA);
		card_status(card_b, sizeof(card_b), CARD_SLOTB);
		storage_status(storage, sizeof(storage));
		workflow_status(transfer, sizeof(transfer));
		choice = UI_HomeMenu(card_a, card_b, storage, transfer, 0);
		if (choice < 0) {
			if (UI_Confirm("Exit GCMM-EX", "Return to loader or system menu?", NULL,
				"Exit"))
				return;
			continue;
		}
		if (choice == 0) {
			run_manage_saves();
			continue;
		}
		if (choice == 3) {
			run_others_menu();
			continue;
		}
		if (choice == 4) {
			if (UI_Confirm("Exit GCMM-EX", "Return to loader or system menu?", NULL,
				"Exit"))
				return;
			continue;
		}
		if (!require_storage())
			continue;
		if (choice == 1) {
			run_full_backup();
		} else {
			run_restore_backup();
		}
	}
}

/****************************************************************************
* Initialise Video
*
* Before doing anything in libogc, it's recommended to configure a video
* output.
****************************************************************************/
static void
Initialise (void)
{
	VIDEO_Init ();		/*** ALWAYS CALL FIRST IN ANY LIBOGC PROJECT!
				     Not only does it initialise the video
				     subsystem, but also sets up the ogc os
				***/

	PAD_Init ();			/*** Initialise pads for input ***/
#ifdef HW_RVL
	WPAD_Init ();
#endif

	// get default video mode
	vmode = VIDEO_GetPreferredMode(NULL);

	switch (vmode->viTVMode >> 2)
	{
	case VI_PAL:
		// 576 lines (PAL 50Hz)
		// display should be centered vertically (borders)
		//Make all video modes the same size so menus doesn't screw up
		vmode = &TVPal576IntDfScale;
		vmode->xfbHeight = 480;
		vmode->viYOrigin = (VI_MAX_HEIGHT_PAL - 480)/2;
		vmode->viHeight = 480;

		vmode_60hz = 0;
		break;

	case VI_NTSC:
		// 480 lines (NTSC 60hz)
		vmode_60hz = 1;
		break;

	default:
		// 480 lines (PAL 60Hz)
		vmode_60hz = 1;
		break;
	}

#ifdef HW_DOL
	/* we have component cables, but the preferred mode is interlaced
	 * why don't we switch into progressive?
	 * (user may not have progressive compatible display but component input)
	 * on the Wii, the user can do this themselves on their Wii Settings */
	if(VIDEO_HaveComponentCable())
		vmode = &TVNtsc480Prog;
#endif

	/*	// check for progressive scan // bool progressive = FALSE;
		if (vmode->viTVMode == VI_TVMODE_NTSC_PROG)
			progressive = true;
	*/

#ifdef HW_RVL
	// widescreen fix
	if(CONF_GetAspectRatio())
	{
		vmode->viWidth = 678;
		vmode->viXOrigin = (VI_MAX_WIDTH_PAL - 678) / 2;
	}
#endif

	// configure VI
	VIDEO_Configure (vmode);

	// always 480 lines /*** Update screen height for font engine ***/
	screenheight = vmode->xfbHeight;

	/*** Now configure the framebuffer.
	     Really a framebuffer is just a chunk of memory
	     to hold the display line by line.
	***/
	// Allocate the video buffers
	xfb[0] = (u32 *) MEM_K0_TO_K1 (SYS_AllocateFramebuffer (vmode));
	/*** I prefer also to have a second buffer for double-buffering.
	     This is not needed for the console demo.
	***/
	xfb[1] = (u32 *) MEM_K0_TO_K1 (SYS_AllocateFramebuffer (vmode));

	/*** Define a console ***/
	console_init (xfb[0], 20, 64, vmode->fbWidth, vmode->xfbHeight, vmode->fbWidth * 2);

	/*** Clear framebuffer to black ***/
	VIDEO_ClearFrameBuffer (vmode, xfb[0], COLOR_BLACK);
	VIDEO_ClearFrameBuffer (vmode, xfb[1], COLOR_BLACK);

	/*** Set the framebuffer to be displayed at next VBlank ***/
	VIDEO_SetNextFramebuffer (xfb[0]);

	/*** Get the PAD status updated by libogc ***/
	VIDEO_SetPostRetraceCallback (updatePAD);
	VIDEO_SetBlack (0);

	/*** Update the video for next vblank ***/
	VIDEO_Flush ();

	VIDEO_WaitVSync ();		/*** Wait for VBL ***/
	if (vmode->viTVMode & VI_NON_INTERLACE)
		VIDEO_WaitVSync ();

}



/****************************************************************************
* RawBackupMode -SD Mode
*
* Perform backup of full memory card (in raw format) to a SD Card.
****************************************************************************/
void SD_RawBackupMode ()
{
	s32 writen = 0;
	raw_progress_context progress = { "Creating RAW backup", "Reading memory card" };
	int success;

	if (!probe_memory_card(MEM_CARD)) {
		UI_MessageError("RAW backup", "Selected memory card is no longer detected.",
			"Insert the card and start the operation again.");
		return;
	}
	RawSetProgressCallback(show_raw_progress, &progress);
	success = BackupRawImage(MEM_CARD, &writen) == 1;
	RawSetProgressCallback(NULL, NULL);
	if (success) {
		char result[80];
		char size[24];

		UI_Progress("Creating RAW backup", "Finalizing backup", 1, 1);
		format_transfer_size(writen, size, sizeof(size));
		snprintf(result, sizeof(result), "%s written to storage.", size);
		UI_MessageSuccess("RAW backup complete", "Memory card backup completed successfully.", result);
	} else {
		UI_MessageError("RAW backup failed", "Memory card backup could not be created.",
			"Check memory card, storage space, and filesystem.");
	}
}

/****************************************************************************
* RawRestoreMode
*
* Restore a full raw backup to Memory Card from SD Card
****************************************************************************/
void SD_RawRestoreMode ()
{
	int files;
	int selected = 0;
	char review[96];
	char detail[96];
	char result[80];
	char size[24];
	s32 writen = 0;
	u32 image_size;
	raw_progress_context progress;
	int i;
	ui_list_action action;

	UI_ClearSavePreview();
	snprintf((char *)currFolder, sizeof(currFolder), "%s", MCSAVES);
	for (;;) {
		files = SDGetFileList(0);
		if (files <= 0) {
			UI_Message("RAW restore", "No RAW backups in this folder.",
				"Use a complete RAW, GCP, or MCI image in MCBACKUP.");
			if (!leave_backup_folder())
				return;
			selected = 0;
			continue;
		}
		if (files > 1024)
			files = 1024;
		action = UI_SaveList("Restore RAW image", (char *)currFolder, filelist,
			files, &selected, NULL, -1, false);
		if (action == UI_LIST_BACK) {
			if (!leave_backup_folder())
				return;
			selected = 0;
			continue;
		}
		if (action == UI_LIST_PREVIEW)
			continue;
		if (action != UI_LIST_OPEN)
			continue;
		if (!filelist_entry_is_valid(selected, files))
			continue;
		if (storage_entry_is_folder((char *)filelist[selected])) {
			if (!enter_backup_folder((char *)filelist[selected]))
				UI_MessageError("RAW restore", "Folder path is too long.",
					"Choose a folder with a shorter path.");
			selected = 0;
			continue;
		}
		if (!ValidateRawImage(MEM_CARD, (char *)filelist[selected], &image_size)) {
			UI_MessageError("RAW restore", "Image is invalid for this memory card.",
				"Check image type, file size, and destination card capacity.");
			continue;
		}
		if (!SDLoadCardImageHeader((char *)filelist[selected])) {
			UI_MessageError("RAW restore", "Image header is invalid or unreadable.",
				"GCMM-EX cannot verify the image Flash ID.");
			continue;
		}
		getserial(imageserial);
		sramex = __SYS_LockSramEx();
		if (!sramex) {
			UI_MessageError("RAW restore blocked", "Could not read the console Flash ID.",
				"GCMM-EX will not offer an unsafe bypass.");
			continue;
		}
		__SYS_UnlockSramEx(0);
		snprintf(review, sizeof(review), "Restore %.42s to Memory Card %c.",
			(char *)filelist[selected], MEM_CARD == CARD_SLOTA ? 'A' : 'B');
		snprintf(detail, sizeof(detail),
			"%u bytes will overwrite every save on the destination card.", image_size);
		if (!UI_ConfirmDestructive("Review RAW restore", review, detail, "Restore RAW image"))
			continue;
		if (!probe_memory_card(MEM_CARD)) {
			UI_MessageError("RAW restore", "Selected memory card is no longer detected.",
				"Insert the card and start the operation again.");
			continue;
		}
#ifdef FLASHIDCHECK
		for (i = 0; i < 12; i++) {
			if (imageserial[i] != sramex->flash_id[MEM_CARD][i]) {
				UI_MessageError("RAW restore blocked", "Card and image Flash IDs do not match.",
					"GCMM-EX will not offer an unsafe bypass.");
				return;
			}
		}
#endif
		progress.title = "Restoring RAW image";
		progress.item = (char *)filelist[selected];
		RawSetProgressCallback(show_raw_progress, &progress);
		if (RestoreRawImage(MEM_CARD, (char *)filelist[selected], &writen) != 1) {
			RawSetProgressCallback(NULL, NULL);
			UI_MessageError("RAW restore failed", "Image could not be restored.",
				"No unsafe retry was attempted.");
			return;
		}
		RawSetProgressCallback(NULL, NULL);
		format_transfer_size(writen, size, sizeof(size));
		snprintf(result, sizeof(result), "%s written to Memory Card %c.", size,
			MEM_CARD == CARD_SLOTA ? 'A' : 'B');
		UI_MessageSuccess("RAW restore complete", "Image restored successfully.", result);
		return;
	}
}

/****************************************************************************
* Main
****************************************************************************/
int main (int argc, char *argv[])
{

	have_sd = false;
	CUR_DEVICE = 0;

	Initialise ();	/*** Start video ***/
	FT_Init ();		/*** Start FreeType ***/

#ifdef HW_RVL
	initialise_power();
#endif

	detect_devices();

	//Check for command line 
	if (argc > 1)
	{
		if (!strcasecmp(argv[1], "ask"))
		{
			//Run device selection screen
			skip_selector = 0;
			selector_flag = 1;
			ClearScreen();
			ShowScreen();
			
			CUR_DEVICE = device_select();
		}
		else if (!strcasecmp(argv[1], "sdgecko"))
		{
			if (DEVICES[DEV_GCSDA] == DEV_AVAIL && !(DEVICES[DEV_GCSDB] == DEV_AVAIL))
			{
				CUR_DEVICE = DEV_GCSDA;
			}
			else if (DEVICES[DEV_GCSDB] == DEV_AVAIL && !(DEVICES[DEV_GCSDA] == DEV_AVAIL))
			{
				CUR_DEVICE = DEV_GCSDB;
			}
			else //Run selector screen if there are both an SDGecko in slot A and Slot B...you won't be able to do anything with that setup though
			{
				//Run device selection screen
				selector_flag = 1;
				ClearScreen();
				ShowScreen();
				
				CUR_DEVICE = device_select();
			}
		}
		else if (!strcasecmp(argv[1], "sdgeckoA"))
		{
			if (DEVICES[DEV_GCSDA] == DEV_AVAIL)
			{
				CUR_DEVICE = DEV_GCSDA;
			}
		}
		else if (!strcasecmp(argv[1], "sdgeckoB"))
		{
			if (DEVICES[DEV_GCSDB] == DEV_AVAIL)
			{
				CUR_DEVICE = DEV_GCSDB;
			}
		}
#ifdef HW_DOL
		else if (!strcasecmp(argv[1], "gcloader"))
		{
			if (DEVICES[DEV_GCODE] == DEV_AVAIL)
			{
				CUR_DEVICE = DEV_GCODE;
			}
		}
		else if (!strcasecmp(argv[1], "sd2sp2"))
		{
			if (DEVICES[DEV_GCSDC] == DEV_AVAIL)
			{
				CUR_DEVICE = DEV_GCSDC;
			}
		}
#endif
#ifdef HW_RVL
		else if (!strcasecmp(argv[1], "wiisd"))
		{
			if (DEVICES[DEV_WIISD] == DEV_AVAIL)
			{
				CUR_DEVICE = DEV_WIISD;
			}
		}
		else if (!strcasecmp(argv[1], "wiiusb"))
		{
			if (DEVICES[DEV_WIIUSB] == DEV_AVAIL)
			{
				CUR_DEVICE = DEV_WIIUSB;
			}
		}
#endif
	}
	
	if (CUR_DEVICE) have_sd = initFAT(CUR_DEVICE);

	//Set correct memory card slot when SD gecko is selected device
	if (CUR_DEVICE == DEV_GCSDB) MEM_CARD = 0;
	else if (CUR_DEVICE == DEV_GCSDA) MEM_CARD = 1;

	selector_flag = 0;
	skip_selector = 0;
	run_home_menu();

	#ifdef HW_RVL
	deinitFAT();
	//if there's a loader stub load it, if not return to wii menu.
	if (!!*(u32*)0x80001800) exit(1);
	else SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
#else
	if (*(int *)0x80001800 == PSOSDLOADID) {
		void (*PSOReload)(void) = (void (*)(void))0x80001800;
		deinitFAT();
		PSOReload();
	}
	if (have_sd) {
		char exitdol[64];
		FILE *fp;
		long size;
		u8 *dol;
		int length;

		length = snprintf(exitdol, sizeof(exitdol), "%s:/autoexec.dol", fatpath);
		fp = length >= 0 && length < (int)sizeof(exitdol) ?
			fopen(exitdol, "rb") : NULL;
		if (fp) {
			if (fseek(fp, 0, SEEK_END) == 0 && (size = ftell(fp)) > 0 &&
				fseek(fp, 0, SEEK_SET) == 0 &&
				size < (long)(AR_GetSize() - (64 * 1024))) {
				dol = memalign(32, (size_t)size);
				if (dol) {
					if (fread(dol, 1, (size_t)size, fp) == (size_t)size) {
						fclose(fp);
						fp = NULL;
						deinitFAT();
						DOLtoARAM(dol, 0, NULL);
					}
					free(dol);
				}
			}
			if (fp)
				fclose(fp);
		}
	}
	deinitFAT();
	SYS_ResetSystem(SYS_HOTRESET, 0, 0);
#endif
	return 0;
}
