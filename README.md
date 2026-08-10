# GCMM-EX

A Nintendo GameCube memory card manager for GameCube and Wii.

> [!IMPORTANT]
> This repository is a community fork of [the original GCMM project created and maintained by suloku](https://github.com/suloku/gcmm). GCMM-EX aims to preserve the project, modernize its codebase, and improve compatibility with current toolchains, storage devices, filesystems, and new features.

## About

GCMM (*GameCube/Wii Memory Manager*) is a homebrew application for backing up, restoring, deleting, and managing Nintendo GameCube save data. It runs on both Nintendo GameCube and Nintendo Wii hardware.

The original project was started by dsbomb and justb, based on Askot's SD support modification for the libogc `mcbackup` example. Suloku later updated and expanded the project, ported it to Wii, and fixed critical save restoration behavior.

This fork builds on that work with the following goals:

- Keep GCMM buildable with newer tools and libraries.
- Improve compatibility with current storage devices and filesystems.
- Fix remaining issues from the original project.
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
- Provide separate light and dark theme builds.
- Select and swap storage devices without restarting GCMM.
- Accept command-line parameters on Wii and GameCube.
- Support FAT16, FAT32, and exFAT storage.

### Supported storage devices

| Platform | Devices |
| --- | --- |
| Wii | Front SD, USB, and SD Gecko in slots A or B |
| GameCube | SD2SP2, SD Gecko in slots A or B, and GC Loader |

GCMM selects a device automatically when only one is available. If multiple devices are available, it opens the device selector. Avoid connecting more than one USB storage device in Wii mode.

## Installation and usage

### Wii — Homebrew Channel

1. Create an `apps/gcmm` directory on the SD card or USB device.
2. Copy `releases/gcmm_WII.dol` into that directory and rename it to `boot.dol`.
3. Copy `hbc/meta.xml` and `hbc/icon.png` into the same directory.
4. Start GCMM from the Homebrew Channel.

To use the dark theme, copy `releases/gcmm_WII_dark.dol` instead.

### GameCube

Run `releases/gcmm_GC.dol` through a compatible homebrew loader such as Swiss or SDLoad. Use `releases/gcmm_GC_dark.dol` for the dark theme.

## Special controls

Standard controls are displayed on the main screen. Full-card operations use these combinations:

| Operation | GameCube controller | Wii Remote |
| --- | --- | --- |
| RAW backup | `L + Y` | `B + -` |
| RAW restore | `L + X` | `B + +` |
| Format memory card | `L + Z` | `B + 2` |

Open the device selector with `R` on a GameCube controller or `1` on a Wii Remote.

## Memory card and save data safety

> [!WARNING]
> RAW restoration and formatting overwrite memory card data. Keep a verified backup before using either operation. Never remove a memory card or storage device while GCMM is reading or writing data.

- A RAW image is a complete copy of a memory card and should generally be restored only to its source card.
- Unofficial cards may share the same Flash ID. This can allow RAW restoration between same-sized unofficial cards, but it should be done with caution.
- Permission-protected saves can be backed up and restored by GCMM.
- Since GCMM 1.3, serial-protected saves from games such as F-Zero and Phantasy Star Online are patched during restoration for use on the target card.
- Insert or remove memory cards only from the main screen or when prompted by the device selector.
- After removing an SD or USB storage device, reopen the device selector to mount it again.

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
make          # Build light themes for GameCube and Wii
make dark     # Build dark themes for GameCube and Wii
make gc       # Build the light GameCube version only
make wii      # Build the light Wii version only
make gc-dark  # Build the dark GameCube version only
make wii-dark # Build the dark Wii version only
make clean    # Remove light and dark build artifacts
```

Final DOL and ELF files are written to the root-level `releases/` directory. Light and dark builds use separate intermediate directories, preventing one theme from reusing object files compiled for the other theme.

| Command | Main DOL output |
| --- | --- |
| `make gc` | `releases/gcmm_GC.dol` |
| `make wii` | `releases/gcmm_WII.dol` |
| `make gc-dark` | `releases/gcmm_GC_dark.dol` |
| `make wii-dark` | `releases/gcmm_WII_dark.dol` |

Current toolchains may provide FreeType only through `pkg-config`, while these Makefiles still call `freetype-config`. Updating that integration remains part of this fork's modernization work.

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

See [`changelog.md`](changelog.md) for the normalized release history. The original release notes and inherited technical documentation remain available in [`readme-original.txt`](readme-original.txt).

GCMM-EX exists because of the original project and its community. Special thanks to suloku, dsbomb, justb, Askot, SoftDev, Costis, Masken, CowTRobo, Samsom, Tantric, tueidj, dronesplitter, PabloACZ, Pikachu025, Nano/Excelsiior, bm123456, themanuel, DacoTaco, Extrems, fincs, ChaN, dragonbane0, Zephiles, Ralf, and f3bandit.

Visit the [original GCMM repository](https://github.com/suloku/gcmm) for its authorship, historical context, and earlier releases.

## License

Distributed under the [GNU General Public License v3.0](COPYING). This fork preserves the credits and rights of the original project's authors and contributors.
