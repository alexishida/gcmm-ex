# Changelog

This file documents user-visible changes included in GCMM-EX DOL releases.

GCMM-EX is a GameCube memory-card manager with complete memory-card
management and a modern workflow for GameCube and Wii.

## [v1.0.1]

### Fixed

- SD Gecko and GC2SD adapters are no longer probed as memory cards when the
  home screen opens, preventing a `CARD_ERROR_WRONGDEVICE` startup error.
- Error handling in `__card_txhandler`: `EXI_Deselect`/`EXI_Unlock` failures now
  set the error mask correctly, so `CARD_ERROR_NOCARD` is reported properly
  instead of being silently ignored.
- `SDSaveMCImage` could spin forever when the written GCI could not be reopened
  for verification; the retry limit now also applies on that path.
- `ARAMPut` (GameCube) read and transferred past the source buffer for small
  misaligned sections; the misaligned prefix is now capped to the transfer
  length.
- Storage folder names containing path separators or `..` could escape the
  backup directory during path construction.
- Storage drivers left started on some "no device" and mount-failure paths in
  `initFAT` and `detect_devices`; drivers are now shut down on those paths.
- `format_transfer_size` showed an incorrect decimal digit for the MB value;
  the fractional part is now computed correctly.

### Security

- Rewrote `storage_entry_name_is_safe`/`storage_folder_is_safe` to reject `.`,
  `..`, `/`, and `\` in file and folder names used to build storage paths.

## [v1.0.0 (alpha)]

### Added

- Task-oriented home, settings, memory-card selection, save details, and
  contextual save actions.
- Guided full-card and single-save GCI backup, card-to-card copy, move, batch
  deletion, and GCI/GCS/SAV restore flows with progress and results.
- exFAT support for Wii SD, Wii USB, SD Gecko, SD2SP2, and GC Loader through
  libdvm.

### Changed

- Renamed the application to GCMM-EX and credited Alex Ishida as author while
  preserving GCMM technical attribution and historical credits.
- Replaced the legacy interface with a new navigation model, visual design,
  and task-oriented workflow.
- Standardized controls across GameCube controllers, Wii Remotes, and Classic
  Controllers.
- Added selected-save banner previews and a compact single-row controller
  footer to the save manager.
- Consolidated action confirmations into one review screen while preserving
  operation details and destructive-action warnings.
- Moved Exit to the home menu and removed it from Advanced options.
- Refined information and credits screens for TV readability.
- Improved FreeType glyph blending for smoother small UI text.
- Replaced libfat with Extrems' libdvm and changed mounting to probe partition
  tables and select a supported filesystem.

### Fixed

- Full-card RAW transfer progress now advances with completed card blocks.
- Operations started without mounted storage now open the device chooser and
  provide a retry path after mount failures.
- Restore-list banner previews now follow the highlighted backup.
- Cleared the device path during unmounting, preventing stale drive names after
  using the device selector.
