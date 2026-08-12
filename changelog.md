# Changelog

This file documents GCMM-EX changes and the historical GCMM change history.

GCMM-EX manages GameCube memory-card saves and complete card images on
Nintendo GameCube and Wii. It provides a new interface and workflow while
retaining selected technical code from [suloku's GCMM](https://github.com/suloku/gcmm)
for save formats, memory-card compatibility, and hardware safety. Historical
entries below were rewritten in U.S. English from
[`readme-original.txt`](readme-original.txt) while preserving their original
meaning, dates, and contributor credits.

## [Unreleased]

### Added

- Configured the isolated Dolphin profile to seed Slot B from the faithful
  local RAW test image without modifying source files.

- Added exFAT support for every storage device: Wii SD, Wii USB, SD Gecko, SD2SP2, and GC Loader. Cards larger than 32 GB no longer need to be reformatted as FAT32.
- Added task-oriented home, settings, memory-card selection, save details, and
  contextual save actions.
- Added guided full-card and single-save GCI backup, card-to-card copy, move,
  batch deletion, and GCI/GCS/SAV restore flows with progress and results.

### Changed

- Updated release version to v1.0.0 (alpha).
- Updated the save manager with selected-save banner previews and a compact,
  single-row controller footer.
- Refined the in-app information and credits layout for TV readability.
- Improved FreeType glyph blending so small UI text renders with smoother,
  fuller edges.

### Fixed

- Fixed full-card RAW transfer progress so it advances with completed card
  blocks instead of remaining at its initial state until the operation ends.
- Opening an operation with no mounted storage now shows the storage-device
  chooser directly, including a retry path when mounting fails.
- Fixed restore-list banner previews so they follow the highlighted backup
  before it is selected.
- Consolidated action confirmations into one review screen while retaining
  operation details and destructive-action warnings.
- Reduced footer height, moved Exit to the home menu, and removed it from
  Advanced options.
- Updated the in-app credits for Alex Ishida and GCMM by suloku.
- Updated Homebrew Channel metadata to match the README's application
  description.
- Renamed generated GameCube and Wii DOL outputs to `gcmm_ex_GC.dol` and
  `gcmm_ex_WII.dol`.

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

- Fixed Dolphin smoke-test card paths to use the USA-region names Dolphin
  resolves at runtime, ensuring isolated cards are seeded from `memorycards/`
  rather than replaced with empty cards.

- Fixed the Dolphin test-profile writer on Windows-style INI files so repeated
  launches no longer create duplicate Core sections.
- Aligned generated DOL load sections to 32 bytes for strict loaders, including
  current Dolphin releases.
- Cleared the device path during unmounting, preventing stale drive names after using the device selector.
