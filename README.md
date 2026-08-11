# GCMM-EX

A Nintendo GameCube memory card manager for GameCube and Wii.

Current application metadata: **GCMM-EX 2.0** by **Alex Ishida**.

> [!IMPORTANT]
> GCMM-EX is based on [GCMM, created and maintained by suloku](https://github.com/suloku/gcmm). It preserves that project's contributions while providing a redesigned, modernized application for current toolchains, storage devices, filesystems, and new features.

## About

GCMM-EX (*GameCube/Wii Memory Manager Extended*) is a homebrew application for backing up, restoring, deleting, and managing Nintendo GameCube save data. It runs on both Nintendo GameCube and Nintendo Wii hardware.

GCMM was started by dsbomb and justb, based on Askot's SD support modification for the libogc `mcbackup` example. Suloku later updated and expanded GCMM, ported it to Wii, and fixed critical save restoration behavior.

GCMM-EX builds on that foundation with the following goals:

- Keep GCMM-EX buildable with newer tools and libraries.
- Improve compatibility with current storage devices and filesystems.
- Fix remaining issues from GCMM.
- Modernize the code without dropping GameCube or Wii support.
- Make maintenance, testing, and community contributions easier.

## Features

- Back up and restore save data in GCI format.
- Restore save data in GCS and SAV formats.
- Delete save data from a memory card.
- Back up a complete memory card as a RAW image.
- Restore RAW, GCP, and MCI memory card images.
- Format memory cards.
- Display save details, banners, and animated icons.
- Support Wii Remotes and GameCube controllers.
- Support console power buttons.
- Provide one consistent visual theme.
- Select and swap storage devices without restarting GCMM-EX.
- Accept command-line parameters on Wii and GameCube.
- Support FAT16, FAT32, and exFAT storage.

### Supported storage devices

| Platform | Devices |
| --- | --- |
| Wii | Front SD, USB, and SD Gecko in slots A or B |
| GameCube | SD2SP2, SD Gecko in slots A or B, and GC Loader |

GCMM-EX shows its home screen before storage selection. Use **Settings → Storage devices** to choose or change storage. Avoid connecting more than one USB storage device in Wii mode.

## Installation and usage

### Wii — Homebrew Channel

1. Create an `apps/gcmm` directory on the SD card or USB device.
2. Copy `releases/gcmm_WII.dol` into that directory and rename it to `boot.dol`.
3. Copy `hbc/meta.xml` and `hbc/icon.png` into the same directory.
4. Start GCMM-EX from the Homebrew Channel.

### GameCube

Run `releases/gcmm_GC.dol` through a compatible homebrew loader such as Swiss or SDLoad.

## Navigation and controls

GCMM-EX opens on a task-oriented home screen showing detected memory cards and
mounted storage. Choose **Manage saves**, **Back up memory card**, **Restore
backup**, or **Settings** before choosing a specific operation.

| Purpose | GameCube controller | Wii Remote / Classic Controller |
| --- | --- | --- |
| Navigate | D-pad or analog stick | D-pad |
| Select / confirm | `A` | `A` |
| Back / cancel | `B` | `B` |
| Save actions | `X` | `+` / Classic `X` |
| Mark saves | `Y` | `-` / Classic `Y` |
| Change device | `L` / `R` | `1` / `2` |
| Help | `Start` | `HOME` |

RAW backup, RAW restore, and memory-card formatting are available only from
**Settings → Advanced options** and retain explicit destructive confirmations.

## Memory card and save data safety

> [!WARNING]
> RAW restoration and formatting overwrite memory card data. Keep a verified backup before using either operation. Never remove a memory card or storage device while GCMM-EX is reading or writing data.

- A RAW image is a complete copy of a memory card and should generally be restored only to its source card.
- Unofficial cards may share the same Flash ID. This can allow RAW restoration between same-sized unofficial cards, but it should be done with caution.
- Permission-protected saves can be backed up and restored by GCMM-EX.
- Since GCMM 1.3, serial-protected saves from games such as F-Zero and Phantasy Star Online are patched during restoration for use on the target card.
- Insert or remove memory cards only from the home screen or Settings storage screen.
- After changing SD or USB storage, use **Settings → Storage devices** to detect
  and mount it again. Canceling or a failed switch restores the previous mount
  when possible.

FAT32 remains the most broadly compatible option in the GameCube and Wii homebrew ecosystem. exFAT is mainly useful for large cards that are already formatted with that filesystem.

## Building from source

### Dependencies

- [devkitPPC/devkitPro](https://devkitpro.org/)
- [libogc2](https://github.com/extremscorner/libogc2)
- [libdvm](https://github.com/extremscorner/libdvm)
- FreeType for PowerPC (`ppc-freetype`)
- zlib

GCMM-EX uses libdvm instead of libfat for partition detection and FAT12, FAT16, FAT32, and exFAT support. The library is still linked as `-lfat`, but `libogc2-libdvm` is required; `libogc2-libfat` is not compatible with this configuration.

Set the devkitPro environment variables before building:

```sh
export DEVKITPRO=/opt/devkitpro
export DEVKITPPC="$DEVKITPRO/devkitPPC"
```

### Build commands

```sh
make       # Build GameCube and Wii versions
make gc    # Build GameCube version only
make wii   # Build Wii version only
make clean # Remove build artifacts
```

Final DOL and ELF files are written to the root-level `releases/` directory. Each platform has its own intermediate directory.

| Command | Main DOL output |
| --- | --- |
| `make gc` | `releases/gcmm_GC.dol` |
| `make wii` | `releases/gcmm_WII.dol` |

Current toolchains may provide FreeType only through `pkg-config`, while these Makefiles still call `freetype-config`. Updating that integration remains part of GCMM-EX modernization work.

### Dolphin smoke testing

The repository includes a launcher for testing built DOLs in Dolphin without
changing your normal emulator profile. It creates an isolated user directory at
`test/dolphin/user/`, which is ignored by Git.

```sh
scripts/run-dolphin.sh --gc    # Launch releases/gcmm_GC.dol
scripts/run-dolphin.sh --wii   # Launch releases/gcmm_WII.dol
```

Set `DOLPHIN=/path/to/dolphin-emu` when Dolphin is not on `PATH`, or set it in
the local `.env` file. The launcher supports Windows paths when running under
WSL. Configure virtual memory cards and controllers in Dolphin before
testing. Use only disposable virtual media for copy, move, delete, restore,
RAW restore, or format tests. See [`test/dolphin/README.md`](test/dolphin/README.md)
for the smoke-test checklist and emulator limitations.

## Repository layout

```text
source/          Shared source code
source/aram/     GameCube loader support
data/            Shared assets
data-gc/         GameCube visual assets
data-wii/        Wii visual assets
hbc/             Homebrew Channel metadata and icon
releases/        Final DOL and ELF build outputs
build_GC*/       GameCube intermediate build files
build_WII*/      Wii intermediate build files
Makefile.gc      GameCube build configuration
Makefile.wii     Wii build configuration
changelog.md     Project release history
```

## History and credits

See [`changelog.md`](changelog.md) for the normalized release history.

GCMM-EX is based on GCMM. Full credit for the GCMM foundation goes to suloku, dsbomb, justb, Askot, and every historical contributor. Special thanks to suloku, dsbomb, justb, Askot, SoftDev, Costis, Masken, CowTRobo, Samsom, Tantric, tueidj, dronesplitter, PabloACZ, Pikachu025, Nano/Excelsiior, bm123456, themanuel, DacoTaco, Extrems, fincs, ChaN, dragonbane0, Zephiles, Ralf, and f3bandit.

Visit the [GCMM repository](https://github.com/suloku/gcmm) for authorship, historical context, and earlier releases.

## License

Distributed under the [GNU General Public License v3.0](COPYING). GCMM-EX preserves the credits and rights of GCMM's authors and contributors.
