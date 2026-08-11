# Source Architecture

GCMM-EX is written in GNU C17 for Nintendo GameCube and Wii hardware. Code is
split by responsibility so workflow changes do not need to alter card-driver,
file-format, or rendering code.

```text
main.c
  ├── ui/       Controller input, menus, text, BMP, banner, and icon previews
  ├── storage/  Card operations, save files, mounted storage, and RAW images
  └── aram/     GameCube-only DOL handoff through auxiliary RAM
```

## Module boundaries

| Area | Responsibility | Main public interface |
| --- | --- | --- |
| `main.c` | Application lifecycle, storage mounting, workflow sequencing | Private workflow functions |
| `ui/ui.c` | Controller input and task-oriented menus | `ui/ui.h` |
| `ui/freetype.c` | Embedded font, status prompts, framebuffer primitives | `ui/freetype.h` |
| `ui/bitmap.c` | Linear framebuffer, BMP, banner, and icon drawing | `ui/bitmap.h` |
| `ui/bannerload.c` | RGB5A3 and indexed save-art decoding | `ui/bannerload.h` |
| `storage/mcard.c` | Save listing, read/write, delete, format, verification | `storage/mcard.h` |
| `storage/sdsupp.c` | GCI/GCS/SAV and RAW/GCP/MCI file access | `storage/sdsupp.h` |
| `storage/raw.c` | Complete card-image backup and guarded restoration | `storage/raw.h` |
| `storage/card.c` | Local libogc-compatible CARD implementation | `storage/card.h` |
| `aram/` | GameCube-only auxiliary-RAM loader | `aram/sidestep.h`, `aram/ssaram.h` |

## Data ownership and lifetime

- `storage/mcard.c` owns the aligned 2 MiB `FileBuffer`. A loaded save contains
  a 64-byte GCI header at offset zero and payload at `MCDATAOFFSET`.
- `storage/gci.h` defines an on-disk 64-byte directory entry. Its layout is a
  compatibility boundary. Do not modify field order, packing, or widths.
- `main.c` owns mount state (`fatpath`, device state) and shared video state.
- UI functions draw and collect input. They do not mount storage or write cards.
- Storage operations are synchronous and may display their own error prompts.
  They do not leave a memory card mounted when they return.

## Safety rules for contributors

- Keep `ATTRIBUTE_ALIGN(32)` on buffers used by CARD, video, or ARAM DMA.
- Validate source path components, input sizes, offsets, headers, sector sizes,
  and card capacity before reading or writing data.
- Keep `FLASHIDCHECK` enabled. RAW restore must retain type, size, header, and
  target identity validation.
- Treat a move as copy, verify destination payload, then delete source only
  after verification succeeds.
- Preserve GameCube/Wii guards: `HW_DOL` for GameCube and `HW_RVL` for Wii.
- Keep platform-neutral code in `source/`; GameCube-only ARAM code belongs in
  `source/aram/`.

## Adding or changing code

1. Put the public contract in the module header, including ownership, return
   values, and destructive-operation requirements.
2. Keep implementation-local helpers `static` and place them near the public
   operation they support.
3. Use U.S. English for public text and new code comments.
4. Build both targets for shared changes: `make gc` and `make wii`.

When documentation changes are user-visible or affect project structure,
update the root README and the `Unreleased` changelog section as well.
