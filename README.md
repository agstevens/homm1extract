# Heroes of Might and Magic I — AGG Extractor

A Python tool for extracting and decoding all asset files from the `HEROES.AGG` archive used by Heroes of Might and Magic I.

Update: C++ 17 version for Mac/Unix also added

## Requirements

```bash
pip install Pillow
```

## Usage

```bash
python homm1_extract.py heroes.agg
```

```bash
python homm1_extract.py editor.agg
```

All files are extracted into a subdirectory named after the archive (e.g. `heroes/`). Image files are decoded to PNG. Sound and raw data files are saved verbatim.

Tested on the HEROES.AGG file from the retail version from good old games (GOG), the windows 95 editor (editor.agg), and the HOMM 1 Demo version HEROES.AGG files. The demo is included in the repsitory. The AGG files seem to be identical for the Demo and the full game. I believe the demo version would only let you play 1 month in game time, so 28 days.

---

## C++ 17 Version Build Instructions
The C++ version is found in the cpp/ folder of this project.

On MacOS:
```bash
brew install cmake
brew install sdl2 sdl2_mixer sdl2_image

mkdir build

cd build

cmake .. && make -j$(sysctl -n hw.ncpu)
```

Unix version is untested right now, but theoretically, to use it, rename CMakeLists_unix.txt to CMakeLists.txt and make sure you have Cmake and zlib installed.

## Usage

Assuming Heroes.agg is copied into the program directory (one level below build/):
```bash
./homm1 ../Heroes.agg 
```

---

### AGG — Aggregate Archive

The `.agg` file is a flat binary archive containing all game assets concatenated together with a small metadata header.

**Overall structure:**

| Section | Description |
|---|---|
| `u16` | Number of files `n` |
| `n × 14 bytes` | FileInfo table (see below) |
| File data | All file contents concatenated sequentially |
| `n × 15 bytes` | Filename table at the very end of the file |

**FileInfo record (14 bytes each):**

| Field | Type | Notes |
|---|---|---|
| Hash | `u32` | CRC hash of the filename (not used for extraction) |
| Unknown | `u16` | Purpose unknown; ignored |
| Size | `u32` | Size of the file in bytes |
| Size (duplicate) | `u32` | Identical to the size field above |

File offsets are **not stored** in the FileInfo records. Instead, file data is laid out sequentially in the same order as the FileInfo table, starting immediately after the table ends. Offsets are computed by accumulating sizes as you walk the table.

**Filename record (15 bytes each):**

13-character null-terminated DOS-compatible filename, followed by 2 bytes of padding. The filename table sits at the very end of the `.agg` file: `name_table_start = len(file) - 15 * n`.

> **Note:** This differs from Heroes of Might and Magic II, which uses 12-byte FileInfo records that include an explicit offset field.

---

### PAL — Palette

There is one file type: `kb.pal` (Heroes I ships two identical copies).

- 768 bytes total: 256 × 3 bytes of RGB color data
- Each channel is stored in the range 0–63; **multiply by 4** to get the actual 0–255 display value
- This palette must be loaded before any ICN, TIL, or BMP files can be decoded

**Color cycling ranges** (used for animation in-game, informational only):

| Range | Effect |
|---|---|
| 214–217 | Red / fire cycling |
| 218–221 | Yellow cycling |
| 231–237 | Ocean / river / lake |
| 238–241 | Blue cycling |

The first and last 10 palette entries and index 36 are black. Nineteen entries are reserved for cycling, leaving 217 colors available for artwork.

**Editor.agg**
The editor was built with a richer palette — 256 colors with extra bands for purples, light pinks, bright greens, and vivid blues. The portraits in editor.agg were re-painted against this richer palette. My theory is that the original game was created for DOS, and the editor was created for Windows 95 and got a palette upgrade.

---

### ICN — Sprites

ICN files contain one or more sprites (animation frames). Each ICN file decodes to a directory of numbered PNG files plus a `spec.xml` metadata file.

**ICN file layout:**

| Field | Type | Description |
|---|---|---|
| `n_sprites` | `u16` | Number of sprites in this file |
| `total_size` | `u32` | Total data size excluding these 6 header bytes |
| Sprite headers | `n × 12 bytes` | One per sprite (see below) |
| Pixel data | variable | All sprites' pixel data concatenated |

**Sprite header (12 bytes):**

| Field | Type | Description |
|---|---|---|
| `offsetX` | `s16` | Hotspot X offset for game engine positioning |
| `offsetY` | `s16` | Hotspot Y offset for game engine positioning |
| `width` | `u16` | Sprite width in pixels |
| `height` | `u16` | Sprite height in pixels |
| `packed` | `u32` | High byte = sprite type; low 24 bits = `data_off` |

The `packed` field encodes two values:
- **type** = `packed >> 24` — `0` for normal color sprites, `32` for monochrome
- **data_off** = `packed & 0x00FFFFFF` — byte offset of this sprite's pixel data, **relative to byte 6 of the ICN file** (i.e. right after the 6-byte global header)

> **Important:** Sprite headers are not necessarily stored in the same order as their pixel data on disk. The `data_off` values may not be monotonically increasing. The end boundary for each sprite's data is found by sorting all `data_off` values and taking the next larger value, rather than assuming `headers[i+1]` is adjacent on disk.

**Pixel encoding — normal sprites:**

| Byte value | Meaning |
|---|---|
| `0x00` | End of line — advance to start of next row |
| `0x01`–`0x7F` | Literal run — next `N` bytes are palette color indices |
| `0x80` | End of sprite data |
| `0x81`–`0xBF` | Skip `N - 0x80` transparent pixels |
| `0xC0` | Shadow pixels — count in next byte (use `byte % 4` if nonzero, else read a second byte for count); drawn as black at alpha 64 |
| `0xC1` | RLE — next byte is count, byte after is color index |
| `0xC2`–`0xFF` | RLE — `N - 0xC0` pixels of the color in the next byte |

**Pixel encoding — monochrome sprites (type = 32):**

| Byte value | Meaning |
|---|---|
| `0x00` | End of line |
| `0x01`–`0x7F` | `N` black (fully opaque) pixels |
| `0x80` | End of sprite data |
| `0x81`–`0xFF` | Skip `N - 0x80` transparent pixels |

All unspecified pixels default to fully transparent. `offsetX`/`offsetY` are written to `spec.xml` for reference but do not affect the PNG canvas size — the PNG is always exactly `width × height` pixels.

---

### TIL — Tiles

TIL files contain rectangular tiles used for map terrain.

**Layout:**

| Field | Type | Description |
|---|---|---|
| `n_tiles` | `u16` | Number of tiles |
| `width` | `u16` | Tile width in pixels |
| `height` | `u16` | Tile height in pixels |
| Pixel data | `n × width × height bytes` | Palette index per pixel, row-major |

Each tile is decoded to a separate numbered PNG using the loaded palette.

The archive contains three TIL files:

| File | Contents |
|---|---|
| `ground32.til` | All terrain types for the main adventure map |
| `clof32.til` | Four dark tiles (night sky) |
| `ston.til` | Stone ground tiles |

---

### BMP — Background Images

HoMM uses a **custom BMP format** that is completely different from standard Windows BMP files. Do not attempt to open these with standard image software before decoding.

**Layout:**

| Field | Type | Description |
|---|---|---|
| Magic | `0x21 0x00` | Fixed identifier |
| `width` | `u16` | Image width in pixels |
| `height` | `u16` | Image height in pixels |
| Pixel data | `width × height bytes` | Palette index per pixel, row-major |

Each pixel value is an index into the 256-color palette loaded from `kb.pal`.

---

### 82M — Sound Effects

Raw audio data. Saved verbatim with the original filename. The exact encoding format (sample rate, bit depth, channels) varies by file and is not decoded by this tool.

---

### Other file types

All other file types (`BIN`, `MSE`, `STD`, `WLK`, `ATK`, `OBJ`, `BKG`, `TOD`, `WIP`, `MAP`, `BIN`, etc.) are saved verbatim. Their formats are not decoded by this tool.

---

## Output Structure

```
heroes/
├── palette_swatch.png      # Visual color swatch of the loaded palette
├── kb.pal                  # Raw palette file
├── overmain.png            # Decoded BMP backgrounds
├── ground32/               # Decoded TIL tiles (one PNG per tile)
│   ├── 0000.png
│   ├── 0001.png
│   └── ...
├── advmice/                # Decoded ICN sprites (one PNG per frame)
│   ├── 0000.png
│   ├── 0001.png
│   ├── spec.xml            # Sprite metadata (offsets, sizes, types)
│   └── ...
├── wsnd00.82m              # Raw sound files
└── ...                     # All other files saved verbatim
```

---

## Known Limitations

- Some sprites in the archive have truncated pixel data. These are decoded as far as possible and the remaining pixels are left transparent.
- Color cycling (palette animation) is not simulated — exported PNGs show the static palette colors.
- `82M` sound files are saved raw and require a separate tool to play back.
- `XMI` / MIDI music files are not present in HoMM I (they are a HoMM II feature).


## Credits

Written by Andrew G. Stevens with assistance from Claude Sonnet 4.6.

Attribution also goes to some Java code from James Koppel about the HOMM2 AGG format and some notes derived from it here:
https://thaddeus002.github.io/fheroes2-WoT/infos/informations.html - there were some minor differences and errors that were debugged and have been documented here and in the code.

## License

This code is released under the Apache License, Version 2.0. 
