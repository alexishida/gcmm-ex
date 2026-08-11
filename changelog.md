# Changelog

This file documents GCMM-EX changes and the historical GCMM change history.

GCMM-EX is based on [suloku's GCMM](https://github.com/suloku/gcmm), with a redesigned codebase and support for current toolchains, devices, filesystems, and future features. Historical entries below were rewritten in U.S. English from [`readme-original.txt`](readme-original.txt) while preserving their original meaning, dates, and contributor credits.

## [Unreleased]

### Added

- Added exFAT support for every storage device: Wii SD, Wii USB, SD Gecko, SD2SP2, and GC Loader. Cards larger than 32 GB no longer need to be reformatted as FAT32.
- Added task-oriented home, settings, memory-card selection, save details, and
  contextual save actions.
- Added guided full-card and single-save GCI backup, card-to-card copy, move,
  batch deletion, and GCI/GCS/SAV restore flows with progress and results.

### Changed

- Set the GCMM-EX application version to 1.0.
- Clarified that GCMM-EX is a new project with a fully redesigned interface
  and workflow, while retaining technical GCMM code and historical credits.
- Renamed the application to GCMM-EX and credited Alex Ishida as author.
- Replaced libfat with Extrems' libdvm, which bundles FatFs and supports FAT12, FAT16, FAT32, and exFAT. libdvm is now a build requirement.
- Changed device mounting to probe partition tables and select the matching filesystem driver instead of assuming FAT. Devices whose first partition is unsupported or uses another filesystem can now be handled correctly.
- Changed final build output paths. DOL and ELF files are now written to the root-level `releases/` directory.
- Simplified builds to one visual theme and one output per platform; removed legacy themed build variants and background inputs.
- Rewrote the main README in U.S. English and documented the current build output layout.
- Organized shared code into dedicated `source/ui/` and `source/storage/`
  modules, and moved Dolphin smoke-test files under `tests/dolphin/`.
- Documented source-module boundaries, shared buffer ownership, binary-format
  constraints, and public function contracts in U.S. English.
- Replaced normal-operation shortcut dispatch with consistent navigation:
  D-pad/analog navigation, A confirmation, B cancellation, X actions, Y
  marking, L/R device changes, and Start/HOME help.

### Fixed

- Cleared the device path during unmounting, preventing stale drive names after using the device selector.
