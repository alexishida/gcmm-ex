# Dolphin smoke-test environment

`scripts/run-dolphin.sh` launches GCMM-EX with a test-only Dolphin user directory at `test/dolphin/user/`. That directory is ignored by Git, so emulator settings, screenshots, save states, and virtual memory cards cannot modify a developer's normal Dolphin profile or enter the repository.

Build first, then launch either platform:

```sh
retro-gcwii make
scripts/run-dolphin.sh --gc
scripts/run-dolphin.sh --wii
```

The launcher accepts `DOLPHIN=/path/to/dolphin-emu` for a native Linux build. It also loads `DOLPHIN` from the local `.env` file. Under WSL, it converts both the DOL and isolated user-directory paths before launching `Dolphin.exe`.

Before the first GameCube run, open Dolphin's Controller Settings and configure Port 1 as a Standard Controller. In GameCube settings, enable Memory Cards for slots A and B. Dolphin creates the virtual cards under the isolated user directory after first use.

Before the first Wii run, configure a Wii Remote or emulated GameCube controller in Dolphin. Use a disposable virtual SD card or USB image if exercising storage flows. Never point the test profile at a real memory card, SD card, or USB device.

## Smoke-test checklist

Run these checks on both DOLs where the relevant device is emulated:

- Home screen appears and lists Memory Card A, Memory Card B, and storage state.
- D-pad/analog navigation, A, B, X, Y, L/R, and help map to expected actions.
- Missing cards and unavailable actions render as disabled and cannot be selected.
- Settings opens storage selection, help, information/credits, and Advanced options.
- Manage saves handles an empty card without a crash.
- Use only disposable virtual cards for copy, move, delete, restore, RAW restore, or format tests.
- Confirm cancellation leaves virtual card contents unchanged before testing a destructive confirmation.

Dolphin emulation is a UI and basic I/O regression check. It does not replace hardware validation for EXI storage adapters, Flash ID behavior, physical card removal, CRT overscan, or controller timing.
