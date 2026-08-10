# Changelog

All notable changes inherited from the original GCMM project are documented in this file.

GCMM-EX is a community fork of [suloku's GCMM](https://github.com/suloku/gcmm), focused on updating the codebase and improving compatibility with current toolchains, devices, filesystems, and future features. Historical entries below were rewritten in U.S. English from [`readme-original.txt`](readme-original.txt) while preserving their original meaning, dates, and contributor credits.

## [Unreleased]

### Added

- Added exFAT support for every storage device: Wii SD, Wii USB, SD Gecko, SD2SP2, and GC Loader. Cards larger than 32 GB no longer need to be reformatted as FAT32.

### Changed

- Replaced libfat with Extrems' libdvm fork, which bundles FatFs and supports FAT12, FAT16, FAT32, and exFAT. libdvm is now a build requirement.
- Changed device mounting to probe partition tables and select the matching filesystem driver instead of assuming FAT. Devices whose first partition is unsupported or uses another filesystem can now be handled correctly.
- Changed final build output paths. DOL and ELF files are now written to the root-level `releases/` directory.
- Split light and dark builds into separate intermediate directories. Dark theme outputs now use the `_dark` filename suffix to prevent collisions and stale object reuse.
- Rewrote the main README in U.S. English and documented the current build output layout.

### Fixed

- Cleared the device path during unmounting, preventing stale drive names after using the device selector.

## [1.5.2] - 2021-11-19

_By suloku._

### Added

- Added a device selector, accessible with `1` on the Wii Remote or `R` on the GameCube controller.
- Added hot-swapping for storage devices without restarting GCMM. The device selector provides instructions for safe insertion and removal.
- Added SD Gecko support in Wii mode.
- Added GC Loader support in GameCube mode.
- Added the current device to the main screen: Wii SD, Wii USB, SD Gecko, SD2SP2, GC Loader, or no device.
- Added command-line parameter support in Wii and GameCube modes. GameCube supports Swiss CLI files.
- Added dark theme builds for Wii and GameCube as separate DOL files.
- Added device-specific backgrounds in GameCube mode.

### Changed

- Switched to libogc2 to fix SD2SP2 remounting after it had already been mounted by GCMM or another DOL without a system restart. The project could still be compiled with libogc at that time.
- Restored custom `card.c` and `card.h` files because libogc2 lacked required functions.

### Fixed

- Fixed the exit and reboot sequence.
- Applied several memory card fixes with help from Extrems and libogc2.
- Fixed SD2SP2 mounting behavior.

## [1.5.1] - 2021-10-10

_By suloku._

### Changed

- Merged DacoTaco's changes for libogc 2.3.1 compatibility, removing the need for local `card.c` and `card.h` copies at that time.
- Reduced memory use in RAW read and restore operations, which had caused problems with 2,048-block memory cards in recent builds.
- Reworked RAW read and restore functions for broader memory card compatibility.

### Fixed

- Fixed a libogc `card.c` bug that prevented correct writes from block 1,024 onward on 2,048-block cards. This required a modified libogc build because GCMM was not using local `card.c` and `card.h` files in this release.

## [1.5] - 2021-04-21

_By suloku._

### Added

- Added an SD2SP2 and SD Gecko selection prompt when holding `A` or `Z` during startup. When SD Gecko is selected, its slot can also be chosen.

### Changed

- Merged carstene1ns' changes for compatibility with newer libraries.
- Adopted the latest libogc `card.c` changes available at the time, removing the `system.h` dependency. Thanks to tueidj for years of patches.
- Changed the default GameCube storage startup flow. GCMM first attempts to use SD2SP2; if unavailable, it asks for the SD Gecko slot.
- Changed the GameCube exit sequence:
  1. Return to a reboot stub such as PSOLOAD or SDLOADER when present.
  2. Otherwise, try to load `autoexec.dol` from the storage device used by GCMM.
  3. Restart the console if neither option is available.

### Fixed

- Fixed a `card.c` bug that prevented some saves with shared filename characters, including Paper Mario saves, from being restored.

> **SDLOADER note:** Running Swiss and keeping a Swiss `autoexec.dol` available is recommended. Returning directly to SDLOADER may leave its selection bar red and prevent further DOL loading. This behavior was observed in Wii GameCube mode; behavior on original GameCube hardware was not confirmed.

## [1.4g] - 2020-01-03

_By suloku. Beta build; no official release was published._

### Added

- Added SD2SP2 support.

## [1.4f] - 2017-04-05

_By suloku._

### Added

- Merged dragonbane0's GCMM 1.4c modification with folder selection and alphabetical sorting, plus additional minor adjustments. Thanks to Zephiles for bringing the modification to the project's attention.

## [1.4e] - 2016-02-27

_By suloku._

### Fixed

- Fixed a `card.c` bug that prevented correct backup and writing of saves whose filenames differed only by letter case. The bug affected TimeSplitters 2 and likely TimeSplitters: Future Perfect. Thanks to DakuTree for reporting the issue and Antidote for fixing it.

## [1.4d] - 2015-08-08

_By suloku._

### Changed

- Changed the button combination for deleting a single save to reduce accidental deletion.

### Fixed

- Fixed a `card.c` bug that prevented writing to the final block of a memory card and therefore prevented restoring a save that would completely fill the card. Thanks to undergroundmonorail.
- Fixed a `card.c` bug that did not release blocks correctly after deleting a file. Previously, the issue was corrected only after using the card with official memory card management software or a game.
- Added a libogc fix for a `card.c` bug that did not affect GCMM because its behavior had already been addressed in version 1.4b.

## [1.4c] - 2014-01-05

_By suloku._

### Changed

- Disabled the `__sector_erase()` check during RAW restoration because some unofficial memory cards had problems with it.

## [1.4b] - 2013-09-03

_By suloku._

### Added

- Added version information to the interface.

### Fixed

- Fixed memory card initialization. Incorrect initialization caused saves from different regions of the same game, or games with similar filenames such as The Legend of Zelda: Twilight Princess and The Legend of Zelda: The Wind Waker, to work incorrectly. Thanks to antidote.crk for identifying the problem.

## [1.4a] - 2012-10-18

_By suloku._

### Changed

- Revised potentially confusing interface text.
- Made font sizes more consistent.

### Fixed

- Fixed SD Gecko detection when a card was inserted or swapped on the slot selection screen in GameCube mode.
- Fixed a missing byte in the Flash ID display.

## [1.4] - 2012-10-08

_By suloku._

### Added

- Added animated save icons and other minor graphical improvements.
- Added SD Gecko slot selection in GameCube mode, similar to the Wii SD/USB prompt.
- Added **Restore All** to Restore Mode, including overwrite support.
- Added filenames to overwrite prompts, including **Restore All** prompts.
- Added a more explicit, user-friendly display for save permissions.
- Added memory card free-block information.
- Added page numbers to the file selector.
- Added security checks to RAW Restore Mode.
- Added font characters required by additional save comments.

### Changed

- Moved **Backup All** to Backup Mode. Press `R` on a GameCube controller or `1` on a Wii Remote to activate it.
- Changed left and right navigation to scroll five file entries at a time.
- Added continuous scrolling while holding up, down, left, or right.
- Made minor code improvements.

Thanks to bm123456 and themanuel for beta testing and support.

## [1.3] - 2012-09-14

_By suloku._

### Added

- Added card and image serial number display to RAW Restore Mode.
- Added restore-time patches by Ralf for F-Zero GX, Phantasy Star Online Episode I & II, and Phantasy Star Online Episode III saves, allowing them to work on the target memory card.

## [1.2d] - 2012-09-08

_By suloku._

### Added

- Added a double overwrite confirmation when restoring a save file to a memory card, based on an idea by Nano/Excelsiior.

### Changed

- Updated RAW mode graphics to make commands easier to understand. The Wii mode design was based on JoostinOnline's work for GCMM+.
- Adopted the DejaVu Sans font from GCMM+ for improved readability, based on work by Nano/Excelsiior.

### Fixed

- Fixed RAW backups failing when the backup directory did not already exist on the SD or USB device.

## [1.2c] - 2012-09-06

_By suloku._

### Changed

- Changed RAW backup filenames to include the card's block count. For example, `Backup_<timestamp>.raw` became `0059b_<timestamp>.raw` or `2043b_<timestamp>.raw`.
- Made minor defensive code changes.

## [1.2b] - 2012-09-06

_By suloku._

### Fixed

- Fixed a potential bug that did not appear to affect versions 1.2 or 1.2a.

## [1.2a] - 2012-09-06

_By suloku._

### Fixed

- Fixed a memory leak introduced in version 1.2 that eventually caused RAW backup and restore operations to hang. A 2,043-block card could trigger the hang on the second RAW backup attempt.

## [1.2] - 2012-09-06

_By suloku._

### Added

- Added RAW backup mode with `.raw` output compatible with Dolphin and Devolution.
- Added RAW, GCP, and MCI image support to RAW Restore Mode.
- Added memory card formatting.
- Added Flash ID display for the inserted card and selected SD image in RAW Restore Mode.
- Added Flash ID checks to protect against writing a RAW image to the wrong memory card.
- Added RAW mode support for official and unofficial memory cards, matching GCI mode compatibility. Thanks to tueidj for guidance.

## [1.1] - 2012-08-29

_By suloku, incorporating changes by PabloACZ and Pikachu025._

### Added

- Added icon and banner support with artwork by dronesplitter.
- Added SD/USB selection during Wii mode startup.
- Added memory card slot selection in Wii mode.
- Added accurate GCI backup and restore through `__card_getstatusex` and `__card_setstatuex`, providing closer one-to-one preservation.

### Changed

- Corrected save date display and reorganized save information.
- Improved the storage mounting function.
- Changed duplicate backup naming. Files are no longer prefixed with a per-session number; duplicate names receive incrementing suffixes such as `savegame_00.gci`, `savegame_01.gci`, and `savegame_02.gci`.

### Fixed

- Prevented the infinite retry loop that could occur during backup in r11 MOD 2.

## [r11 MOD 2] - 2011-09-11

_By Pikachu025._

### Added

- Added **Backup All**, started with `R` on a GameCube controller or `1` on a Wii Remote. All saves are copied to SD without prompts between files.
- Added an `illegal_name` fallback for saves whose filenames cannot be written to SD.
- Added sequential numeric prefixes to files written during the current session, preventing collisions when several saves produce the same filename.
- Added a write verification check that retries failed writes. This version could enter an infinite retry loop when the SD card lacked free space.
- Updated the controls image to include the new option.

## [r11 MOD] - 2011-09-09

_By PabloACZ._

### Added

- Added preliminary support for official GameCube memory cards. Testing at the time confirmed a 256-block card.
- Added zlib to the Makefiles for compatibility with the FreeType PowerPC port available at the time.

### Changed

- Updated `SDGetFileList()` in `sdsupp.c` for then-current devkitPPC and libogc directory APIs, replacing `diropen`, `dirnext`, and `dirclose` with `opendir`, `readdir`, and `closedir`.
- Changed `MountCard()` in `mcard.c` to probe the GameCube memory card slot and confirm that it mounted correctly.
- Improved GCS and SAV compatibility using jcwitzel's December 2009 patch.
- Built with devkitPPC r24, libogc 1.8.8, libfat 1.0.10, and FreeType 2.4.2.

## [1.0] - 2008-12-31

### Added

- Added Wii support.
- Added a new background.
- Added interlaced and widescreen support for all regions.
- Added Delete Mode, based on work by dsbomb and justb.
- Added save information display, primarily based on work by dsbomb and justb.

### Changed

- Updated the project to use libfat.
- Added other user-facing fixes and improvements.

### Fixed

- Fixed save restoration.
