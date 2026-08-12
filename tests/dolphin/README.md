# Dolphin smoke-test environment

`scripts/run-dolphin.sh` launches GCMM-EX with a test-only Dolphin user directory at `tests/dolphin/user/`. That directory is ignored by Git, so emulator settings, screenshots, save states, and virtual memory cards cannot modify a developer's normal Dolphin profile or enter the repository.

On first use, the launcher copies
`memorycards/backup.USA.raw` to
`tests/dolphin/user/GC/GCMM-EX/Test Card B.USA.raw`. It configures Dolphin
Slot B as this raw Memory Card and disables Slot A, so GCMM-EX reads the
faithful card dump rather than individually seeded GCI files. This explicit
USA-region name matches the file Dolphin opens, so it is not replaced by an
empty regional card. Source files remain unchanged; delete the isolated copy
only when a fresh copy is wanted.

Build first, then launch either platform:

```sh
retro-gcwii make
scripts/run-dolphin.sh --gc
scripts/run-dolphin.sh --wii
```

Use `scripts/run-dolphin.sh --setup` to create or update this configuration
without starting Dolphin.

The launcher accepts `DOLPHIN=/path/to/dolphin-emu` for a native Linux build. It also loads `DOLPHIN` from the local `.env` file. Under WSL, it converts both the DOL and isolated user-directory paths before launching `Dolphin.exe`.

Before the first GameCube run, open Dolphin's Controller Settings and configure Port 1 as a Standard Controller. The launcher enables the RAW Memory Card in Slot B in the isolated profile.

Before the first Wii run, configure a Wii Remote or emulated GameCube controller in Dolphin. Use a disposable virtual SD card or USB image if exercising storage flows. Never point the test profile at a real memory card, SD card, or USB device.

## Smoke-test checklist

Run these checks on both DOLs where the relevant device is emulated:

- Home screen appears and lists Memory Card B and storage state.
- D-pad/analog navigation, A, B, X, Y, L/R, and help map to expected actions.
- Missing cards and unavailable actions render as disabled and cannot be selected.
- Settings opens storage selection, help, information/credits, and Advanced options.
- Manage saves handles an empty card without a crash.
- Use only disposable virtual cards for copy, move, delete, restore, RAW restore, or format tests.
- Confirm cancellation leaves virtual card contents unchanged before testing a destructive confirmation.

Dolphin emulation is a UI and basic I/O regression check. It does not replace hardware validation for EXI storage adapters, Flash ID behavior, physical card removal, CRT overscan, or controller timing.
