# GCMM-EX Task-Oriented Redesign Plan

This checklist translates `gc-memory-manager-redesign-usabilidade.md` into implementation work for GCMM-EX. It also records two explicit scope decisions:

- Ship a dark theme only.
- Do not reuse legacy BMP artwork. All required visual assets must be original.

The redesign must preserve real GameCube/Wii hardware compatibility, save-format compatibility, data-safety checks, low memory use, historical credits, and existing storage/filesystem support.

## Status legend

- `[x]` Prepared in the current worktree.
- `[ ]` Still required or not yet validated.
- `[~]` Started, but incomplete or not integrated.

## 1. Product and safety constraints

- [ ] Keep shared code in `source/` and GameCube-only code in `source/aram/`.
- [ ] Keep C code compatible with `gnu17`.
- [ ] Preserve GameCube and Wii support.
- [ ] Preserve GameCube controller and Wii Remote input support.
- [ ] Preserve SD Gecko A/B, SD2SP2, GC Loader, Wii SD, and Wii USB support.
- [ ] Preserve FAT12, FAT16, FAT32, and exFAT support through libdvm.
- [ ] Keep filesystem linkage as `-lfat` supplied by `libogc2-libdvm`.
- [ ] Preserve GCI, GCS, SAV, RAW, GCP, and MCI binary compatibility.
- [ ] Preserve the 2 MB aligned file buffer and existing DMA/CARD alignment rules.
- [ ] Preserve `FLASHIDCHECK` for RAW restore.
- [ ] Preserve protected-save serial and checksum patches, including F-Zero GX and Phantasy Star Online.
- [ ] Never reduce validation or confirmations around overwrite, delete, RAW restore, or formatting.
- [ ] Never instruct users to remove a memory card or storage device during reads or writes.
- [ ] Keep all new public UI text, documentation, and code comments in U.S. English.

## 2. Dark-only visual system

- [x] Create an original dark background asset for the redesigned UI.
- [x] Convert the background to a 640×450, 24-bit Windows BMP suitable for the existing renderer.
- [x] Store the shared asset at `data/ui_bg.bmp`.
- [x] Change `source/bitmap.c` to load `ui_bg_bmp.h` instead of legacy platform/theme backgrounds.
- [ ] Confirm the new background is readable in NTSC, PAL, progressive, and interlaced output modes.
- [ ] Confirm the central content area remains legible after common CRT overscan.
- [ ] Remove every unused legacy background from the build inputs.
- [ ] Audit and remove unused legacy source artwork under `background/` only after confirming no documentation or packaging path consumes it.
- [ ] Remove `LIGHT_MODE` branches from active UI code.
- [ ] Make the dark palette unconditional and remove unreachable light-theme constants.
- [ ] Define one consistent palette for:
  - Background.
  - Primary panel.
  - Secondary panel.
  - Borders.
  - Focus/selection.
  - Primary text.
  - Secondary text.
  - Success.
  - Warning.
  - Destructive action.
  - Disabled/unavailable state.
- [ ] Check contrast for every text and selection state on an SD television.
- [ ] Avoid alpha blending, large extra buffers, or GPU-heavy effects.

## 3. Build simplification for one theme

- [ ] Change `Makefile.gc` so `DARK_MODE` is always enabled.
- [ ] Change `Makefile.wii` so `DARK_MODE` is always enabled.
- [ ] Remove light/dark conditional output naming.
- [ ] Produce only these official outputs:
  - `make gc` → `releases/gcmm_GC.dol`.
  - `make wii` → `releases/gcmm_WII.dol`.
- [ ] Remove or convert `gc-dark` and `wii-dark` targets into documented compatibility aliases without creating duplicate artifacts.
- [ ] Remove unused `light` and `dark` aggregate targets.
- [ ] Simplify `make clean` so it removes all GC/Wii intermediates and the two official outputs.
- [ ] Confirm shared `data/ui_bg.bmp` is embedded once per platform build.
- [ ] Confirm old BMP assets are no longer embedded as unused binary objects.

## 4. Reusable UI foundation

- [x] Add `source/ui.h` with reusable menu, list, confirmation, details, progress, message, and help APIs.
- [x] Add the initial `source/ui.c` implementation.
- [~] Implement a single input abstraction for GameCube controller, Wii Remote, and Classic Controller.
- [ ] Verify every Wii/Classic input constant against the installed libogc2 headers.
- [ ] Add repeat timing that works for D-pad and analog navigation without skipping entries.
- [ ] Ensure input debounce never blocks power-button handling.
- [ ] Use one consistent control model:
  - D-pad/analog: navigate.
  - A: select or confirm.
  - B: back or cancel.
  - X: contextual actions.
  - Y: multi-selection.
  - L/R: previous or next device/tab.
  - Start: help/quick menu.
- [ ] Provide equivalent Wii Remote mappings and display them contextually.
- [ ] Keep focus visible at all times.
- [ ] Keep the safe/cancel option selected by default in destructive dialogs.
- [ ] Add disabled-item rendering and prevent selection of unavailable actions.
- [ ] Add page indicators for long menus and save lists.
- [ ] Truncate long display names safely without modifying source filenames.
- [ ] Replace fixed-size `sprintf` calls in new UI paths with bounded formatting.
- [ ] Review all UI coordinates against 640×450 and the actual framebuffer height.
- [ ] Avoid adding large global UI buffers.

## 5. Startup and home screen

- [ ] Stop opening the storage selector before the home screen unless a command-line option explicitly requests it.
- [ ] Detect Memory Card A, Memory Card B, and supported storage before drawing the home screen.
- [ ] Show Memory Card A state:
  - Detected/not detected.
  - Save count when safely available.
  - Used/free blocks when safely available.
  - Unavailable when its slot is occupied by SD Gecko.
- [ ] Show Memory Card B with the same state details.
- [ ] Show active storage device and mounted/unmounted state.
- [ ] Distinguish “not detected,” “detected,” “mounted,” “unsupported filesystem,” and “mount failed.”
- [ ] Add primary home tasks:
  - Manage saves.
  - Back up memory card.
  - Restore backup.
  - Settings.
- [ ] Make B request exit from the home screen, with a clear confirmation.
- [ ] Make Start open the controls/help screen.
- [ ] Refresh device state after returning from operations.
- [ ] Preserve command-line device selection for Wii and GameCube loaders.
- [ ] Do not auto-mount a different storage device when a command-line device was selected successfully.

## 6. Storage-device selection and state

- [x] Replace the fixed four-entry device-selector mapping with a dynamic list supporting all configured devices.
- [x] Add readable names for every supported storage device.
- [~] Route device selection through the reusable menu component.
- [ ] Allow B to cancel storage selection without losing the currently mounted device.
- [ ] Preserve the previous mount if switching is canceled or the new mount fails.
- [ ] Unmount every volume mounted by libdvm before changing physical storage.
- [ ] Clear stale `fatpath` and `fatbase` state on every unmount path.
- [ ] Re-detect devices only from an explicit safe screen.
- [ ] Show source and destination as persistent state, not hidden actions.
- [ ] Support L/R device switching where it is safe and meaningful.
- [ ] Prevent choosing the same memory card as both source and destination.
- [ ] Prevent choosing a memory-card slot occupied by the active SD Gecko.

## 7. Memory-card selection

- [ ] Add one reusable Memory Card A/B selector.
- [ ] Filter or disable slots that cannot be accessed because storage occupies the same EXI slot.
- [ ] Show detected/not detected state before selection.
- [ ] Probe twice where required by current libogc2 debounce behavior.
- [ ] Re-probe immediately before sensitive operations.
- [ ] Keep `MEM_CARD` synchronized with the selected source/destination.
- [ ] Remove the legacy global prompt that asks for a card slot after every home-screen shortcut.

## 8. Manage saves flow

- [ ] Implement `Home → Manage saves → Choose device → Save list`.
- [ ] Support Memory Card A and Memory Card B as browse sources.
- [ ] Support mounted storage as a backup-file browse source.
- [ ] Show the active source device in the list header.
- [ ] Show save count and block usage for memory cards.
- [ ] Preserve save banner, icon animation, comments, game code, company code, date, block count, permissions, and copy count.
- [ ] Keep list rendering responsive without repeatedly reading full save contents on every frame.
- [ ] Use A to open save details/actions.
- [ ] Use X to open the contextual action menu.
- [ ] Use Y to toggle multi-selection.
- [ ] Use B to return without side effects.
- [ ] Use L/R to switch between accessible devices.
- [ ] Preserve selection when returning from details if the underlying list did not change.
- [ ] Refresh and clamp selection after delete or move.
- [ ] Handle an empty card with a useful empty state.
- [ ] Handle card removal while the list is open.

## 9. Save details and contextual actions

- [ ] Add a save-details screen containing:
  - Display name/comments.
  - Internal filename.
  - Game code and company code.
  - Size in blocks.
  - Last-modified date/time.
  - Source slot/device.
  - Permissions and copy count.
- [ ] Add contextual actions only after a save is selected:
  - Copy.
  - Move.
  - Back up.
  - Delete.
  - Details.
- [ ] Hide or disable actions that cannot be completed with current devices.
- [ ] Explain why an action is disabled.
- [ ] Preserve the original internal filename and metadata during copy/restore.

## 10. Copy and move flows

- [ ] Implement `Save → Copy → Choose destination → Review → Execute → Result`.
- [ ] Allow card-to-storage copy through verified GCI backup.
- [ ] Allow card-to-card copy when both slots are accessible.
- [ ] Read and validate the source completely before writing the destination.
- [ ] Reuse `CardWriteFile()` so protected-save patches and overwrite validation remain active.
- [ ] Verify storage output size exactly, not merely “greater than zero.”
- [ ] Show source, destination, filename, and block count on the review screen.
- [ ] Show progress during read and write phases.
- [ ] Implement move as verified copy followed by source deletion.
- [ ] Never delete the source when destination creation or verification fails.
- [ ] Require an extra confirmation for move because it ends with deletion.
- [ ] Report partial failure precisely when copy succeeds but source deletion fails.

## 11. Multi-selection

- [ ] Store selection state in a bounded array sized to `CARD_MAXFILES`.
- [ ] Show a visible mark next to each selected save.
- [ ] Show selected-save count in the footer/header.
- [ ] Add multi-save actions:
  - Back up selected.
  - Copy selected when a valid common destination exists.
  - Delete selected.
  - Clear selection.
- [ ] Review batch source, destination, item count, and estimated size before execution.
- [ ] Show per-item and overall progress.
- [ ] Continue/stop on per-file errors through an explicit prompt.
- [ ] Summarize succeeded, failed, and skipped counts.
- [ ] Require a strong confirmation before batch deletion.
- [ ] Delete by stable identifiers or safe ordering so indices do not drift.

## 12. Full memory-card backup

- [ ] Implement `Home → Back up memory card`.
- [ ] Choose Memory Card A or B as the source.
- [ ] Choose or confirm the mounted storage destination.
- [ ] Show a generated backup label/path and save count.
- [ ] Show estimated data size when it can be calculated safely.
- [ ] Show a review screen before starting.
- [ ] Back up every save as GCI while preserving current naming compatibility.
- [ ] Verify each written file.
- [ ] Show `current/total` progress and current filename.
- [ ] Allow cancel only before writing begins or between files; never interrupt a file write unsafely.
- [ ] Report complete, partial, canceled, and failed outcomes distinctly.
- [ ] Keep full-card RAW backup separate in Advanced options.

## 13. Restore backup flow

- [ ] Implement `Home → Restore backup`.
- [ ] Choose or confirm the storage source.
- [ ] Browse `MCBACKUP` and supported subfolders.
- [ ] Show only supported GCI/GCS/SAV files in the normal restore flow.
- [ ] Validate file extension, header, block count, total size, and readable content before destination selection.
- [ ] Display backup metadata before restore.
- [ ] Choose Memory Card A or B as destination.
- [ ] Show backup, destination, and overwrite warning on the review screen.
- [ ] Preserve the current overwrite confirmation and second confirmation.
- [ ] Preserve space checks and destination validation.
- [ ] Preserve F-Zero GX and PSO patches.
- [ ] Show read/write progress.
- [ ] Show specific errors for invalid file, insufficient space, existing save, read failure, write failure, and card removal.
- [ ] Keep RAW/GCP/MCI restore separate in Advanced options.

## 14. Delete flow

- [x] Add a bounded low-level `MCardDeleteFile(slot, id)` operation for task workflows.
- [ ] Integrate deletion only through contextual single-save or multi-save actions.
- [ ] Show selected save name, source slot, and size before deletion.
- [ ] State clearly that deletion cannot be undone.
- [ ] Default focus to Cancel.
- [ ] Re-probe and remount immediately before deletion.
- [ ] Preserve company/game-code scoping for `CARD_Delete()`.
- [ ] Refresh the directory only after the delete result is known.
- [ ] Never expose delete as a home-screen physical-button shortcut.

## 15. Advanced operations and settings

- [ ] Implement Settings with:
  - Storage devices.
  - Controls/help.
  - Information/credits.
  - Advanced options.
- [ ] Implement Advanced options with:
  - Full-card RAW backup.
  - Full-card RAW/GCP/MCI restore.
  - Format Memory Card.
  - Boot loader/exit behavior.
- [ ] Remove RAW backup, RAW restore, format, and device switching from hidden button combinations.
- [ ] Keep advanced operations visually separated from normal save management.
- [ ] Show high-risk styling only for destructive operations.

## 16. Format Memory Card

- [x] Add a low-level `MCardFormat(slot)` operation for the task workflow.
- [ ] Place formatting at `Settings → Advanced options → Format Memory Card`.
- [ ] Choose Memory Card A or B explicitly.
- [ ] Probe and validate the selected card immediately before confirmation.
- [ ] First confirmation: identify the selected card and state that all data will be erased.
- [ ] Final confirmation: require selecting an explicit `FORMAT MEMORY CARD` action.
- [ ] Default both confirmation screens to Cancel.
- [ ] Show non-interruptible formatting progress.
- [ ] Check and report `CARD_Format()` return status.
- [ ] Remount and verify the card after formatting.
- [ ] Never expose format through a direct home-screen shortcut.

## 17. RAW restore safety

- [ ] Keep RAW restore only in Advanced options.
- [ ] Validate RAW/GCP/MCI type and minimum header size.
- [ ] Validate image capacity against the destination card.
- [ ] Keep Flash ID comparison enabled.
- [ ] Show source image, destination card, capacity, and overwrite warning.
- [ ] Require two explicit confirmations before any write.
- [ ] Show block-level progress when supported by the existing raw routines.
- [ ] Report Flash ID mismatch without offering an unsafe bypass.
- [ ] Preserve all existing read/write error handling.

## 18. Progress, results, and errors

- [~] Add reusable progress, message, confirmation, and details surfaces.
- [ ] Integrate progress with every copy, backup, restore, move, RAW, and format operation.
- [ ] Keep current operation, source, destination, and item visible.
- [ ] Show determinate percentage when total work is known.
- [ ] Show an indeterminate state when total work is not available.
- [ ] Keep “Do not remove…” warnings visible during I/O.
- [ ] Standardize success, warning, cancellation, partial success, and failure messages.
- [ ] Replace generic errors with actionable messages where error codes are known.
- [ ] Ensure every opened file is closed on all error paths.
- [ ] Ensure every mounted card/device is unmounted on all relevant exit paths.

## 19. Legacy UI removal and cleanup

- [ ] Stop calling legacy `SelectMode()` from `main()`.
- [ ] Remove direct physical-button action dispatch from `SelectMode()`.
- [ ] Replace legacy `WaitPromptChoiceAZ()` destructive semantics with normal A/B navigation.
- [ ] Remove mode-specific status text from `FreeBlocks()`.
- [ ] Remove obsolete spaced-letter headings such as `B a c k u p   M o d e`.
- [ ] Retire duplicated backup/delete/restore list setup after task flows are stable.
- [ ] Remove unused mode values and globals only after all callers are migrated.
- [ ] Keep low-level save/card/storage functions separate from screen navigation.
- [ ] Preserve historical license and author headers in every retained source file.
- [ ] Run `rg` for stale references to deleted background headers and old shortcuts.

## 20. Main-loop integration

- [ ] Replace the shortcut-oriented main loop with task dispatch.
- [ ] Make home selection drive Manage, Backup, Restore, and Settings workflows.
- [ ] Centralize exit/loader behavior in one function.
- [ ] Keep Wii return-to-menu behavior unchanged when no loader stub exists.
- [ ] Keep GameCube PSO/SD loader and `autoexec.dol` fallback behavior unchanged.
- [ ] Free card buffers between completed workflows without freeing active operation data.
- [ ] Reset cancel, multi-select, and progress state when returning home.
- [ ] Refresh mounted-device state after settings/device changes.
- [ ] Preserve power-button callbacks during every screen and operation.

## 21. Documentation

- [ ] Update `README.md` screenshots/usage narrative for task-oriented navigation.
- [ ] Remove the legacy shortcut table.
- [ ] Document the new standard control map for GameCube controller and Wii Remote.
- [ ] Document that GCMM-EX now ships a dark theme only.
- [ ] Document the two official build outputs and revised make targets.
- [ ] Keep storage-device, filesystem, and safety documentation intact.
- [ ] Update installation examples to use the new output names.
- [ ] Add the redesign to `changelog.md` under `Unreleased`.
- [ ] Record removal of light-theme variants and legacy artwork under `Changed`.
- [ ] Record task-oriented navigation, multi-selection, progress, and guided flows under `Added`.
- [ ] Do not change `appversion` or `hbc/meta.xml` version/date without an explicit release request.

## 22. Static checks and build validation

- [ ] Inspect all new bounded strings for truncation and null termination.
- [ ] Check all indices before reading `filelist`, `CardList`, marked-state arrays, and folder paths.
- [ ] Check file lengths before reading into the 2 MB buffer.
- [ ] Check every `fread`, `fwrite`, `fseek`, `ftell`, `CARD_Read`, and `CARD_Write` result touched by the redesign.
- [ ] Confirm no new large global/static buffers were introduced.
- [ ] Confirm all CARD/DMA buffers retain 32-byte alignment.
- [ ] Run a clean GameCube build: `make gc-clean && make gc`.
- [ ] Run a clean Wii build: `make wii-clean && make wii`.
- [ ] Resolve all new compiler warnings.
- [ ] Confirm only the two intended DOL/ELF pairs are generated.
- [ ] Confirm no generated `build_*` or `releases/` artifacts are committed.

## 23. Functional test matrix

- [ ] Test GameCube with SD2SP2.
- [ ] Test GameCube with SD Gecko A.
- [ ] Test GameCube with SD Gecko B.
- [ ] Test GameCube with GC Loader.
- [ ] Test Wii with front SD.
- [ ] Test Wii with USB storage.
- [ ] Test Wii with SD Gecko A/B.
- [ ] Test FAT12, FAT16, FAT32, and exFAT where practical.
- [ ] Test one storage device, multiple storage devices, and no storage device.
- [ ] Test no memory card, blank card, normal card, full card, and card removal.
- [ ] Test one save, many saves, long names, animated icons, missing banners, and protected saves.
- [ ] Test single backup, full backup, multi-backup, card-to-card copy, move, single delete, and batch delete.
- [ ] Test GCI, GCS, and SAV restore.
- [ ] Test RAW, GCP, and MCI restore from Advanced options.
- [ ] Test invalid header, wrong extension, truncated file, oversized file, insufficient space, read failure, write failure, overwrite cancel, and user cancel.
- [ ] Test format cancel at first confirmation and final confirmation.
- [ ] Test Flash ID mismatch.
- [ ] Test console power button and controller disconnect behavior.
- [ ] Test NTSC 480i, PAL 576i, and progressive mode where hardware/emulator access permits.
- [ ] Test both GameCube controller and Wii Remote control mappings.

## 24. Acceptance criteria

- [ ] A first-time user can identify connected devices and the four primary tasks from the home screen.
- [ ] No normal operation requires memorizing a hidden button combination.
- [ ] A/B behavior is consistent on every screen.
- [ ] Save actions appear only after a save is selected.
- [ ] Source and destination are visible before every copy, backup, restore, or move.
- [ ] Delete requires explicit confirmation.
- [ ] Move never deletes source data before destination verification.
- [ ] Format and RAW restore require two confirmations and live only in Advanced options.
- [ ] Progress and final results are visible for every I/O operation.
- [ ] The UI is readable with overscan on an SD display.
- [ ] The project uses only the new original dark artwork.
- [ ] GameCube and Wii builds complete from a clean tree.
- [ ] Existing save compatibility and hardware support remain intact.

## File-level change map

| File/path | Required change |
| --- | --- |
| `source/main.c` | Replace shortcut dispatch with task workflows; integrate home, device/card state, manage, backup, restore, settings, advanced, and exit. |
| `source/ui.c` | Finish reusable dark UI, input abstraction, menus, lists, confirmations, progress, help, and disabled states. |
| `source/ui.h` | Keep stable UI contracts and explicit result enums. |
| `source/freetype.c` | Remove legacy mode screen/shortcut UI after migration; preserve font, banner, icon, and low-level drawing support. |
| `source/freetype.h` | Export only retained drawing/text APIs and the new integration points. |
| `source/bitmap.c` | Use only `ui_bg_bmp`; remove platform/theme background selection. |
| `source/mcard.c` | Keep binary-safe card operations; expose bounded delete/format helpers; improve return checking. |
| `source/mcard.h` | Declare task-safe card operations. |
| `source/sdsupp.c` | Verify exact output size; strengthen file/path bounds and read/write checks. |
| `source/raw.c` | Connect progress and guided confirmations without weakening Flash ID/capacity checks. |
| `data/ui_bg.bmp` | Original shared dark background, 640×450, 24-bit BMP. |
| `data-gc/`, `data-wii/` | Remove obsolete theme/platform backgrounds if no longer consumed. |
| `background/` | Remove unused legacy source artwork only after dependency audit. |
| `Makefile` | Expose only dark GC/Wii default builds and clean targets. |
| `Makefile.gc` | Always compile dark GC build and embed shared new asset. |
| `Makefile.wii` | Always compile dark Wii build and embed shared new asset. |
| `README.md` | Document new navigation, controls, dark-only distribution, and build outputs. |
| `changelog.md` | Record redesign under `Unreleased`. |

## Current worktree notes

- The reusable UI layer exists but is not yet connected to the main application loop.
- The dynamic storage selector uses the new menu layer but still needs cancel/remount handling and build validation.
- The new background is wired into the bitmap renderer.
- Low-level delete and format helpers exist but must only be called from completed confirmation workflows.
- Theme/build makefiles and documentation have not yet been migrated to dark-only distribution.
- No devkitPPC/libogc2 build has been executed in this environment yet; toolchain availability must be confirmed before claiming validation.
