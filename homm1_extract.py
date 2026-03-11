#!/usr/bin/env python3
"""
Heroes of Might and Magic I - AGG file extractor.

Extracts all files from HEROES.AGG and decodes:
  - ICN  -> PNG sprites (RGBA, with transparency)
  - TIL  -> PNG tiles
  - BMP  -> PNG images (HoMM palette-indexed format, NOT standard BMP)
  - PAL  -> raw palette saved + palette_swatch.png for inspection
  - 82M  -> raw sound data saved as-is
  - everything else -> saved verbatim

Usage:
    pip install Pillow
    python homm1_extract.py heroes.agg

Output goes into a directory named after the .agg file (e.g. 'heroes/').


## Credits

Written by Andrew G. Stevens with assistance from Claude Sonnet 4.6.

Attribution also goes to some Java code from James Koppel about the 
HOMM2 AGG format and some notes derived from it here:
https://thaddeus002.github.io/fheroes2-WoT/infos/informations.html - 
there were some minor differences and errors that were debugged and 
have been documented here and in the code.

## License

This code is released under the Apache License, Version 2.0. 
"""

import struct
import os
import sys
import argparse
from pathlib import Path

try:
    from PIL import Image
    PIL_AVAILABLE = True
except ImportError:
    PIL_AVAILABLE = False
    print("WARNING: Pillow not found. Images will not be decoded.")
    print("         Install with: pip install Pillow\n")


# ===========================================================================
# Binary read helpers (all little-endian)
# ===========================================================================

def u8(d, p):  return d[p], p + 1
def u16(d, p): return struct.unpack_from('<H', d, p)[0], p + 2
def u32(d, p): return struct.unpack_from('<I', d, p)[0], p + 4
def s16(d, p): return struct.unpack_from('<h', d, p)[0], p + 2


# ===========================================================================
# AGG parsing - Heroes I format
#
# Header: u16 n_files
# FileInfo table: n_files * 14 bytes each
#   u32  hash
#   u16  unknown
#   u32  size
#   u32  size (duplicate)
# File data: concatenated, sequential (no explicit offsets stored)
# Filename table: n_files * 15 bytes at end of file
#   13-char null-terminated DOS filename + 2 padding bytes
# ===========================================================================

def parse_agg(data):
    pos = 0
    n_files, pos = u16(data, pos)
    print(f"AGG contains {n_files} file(s).")

    # Read FileInfo records (14 bytes each)
    records = []
    for _ in range(n_files):
        fhash = struct.unpack_from('<I', data, pos)[0]; pos += 4
        _unk  = struct.unpack_from('<H', data, pos)[0]; pos += 2
        size  = struct.unpack_from('<I', data, pos)[0]; pos += 4
        _dup  = struct.unpack_from('<I', data, pos)[0]; pos += 4
        records.append({'hash': fhash, 'size': size})

    # File data starts immediately after the header table and is sequential
    offset = pos
    for r in records:
        r['offset'] = offset
        offset += r['size']

    # Filename table: last 15*n bytes of the file
    name_table_start = len(data) - 15 * n_files
    for i, r in enumerate(records):
        raw = data[name_table_start + i*15 : name_table_start + i*15 + 13]
        null = raw.find(b'\x00')
        raw = raw[:null] if null != -1 else raw
        r['name'] = raw.decode('ascii', errors='replace').strip()

    # Attach file contents
    for r in records:
        r['data'] = data[r['offset'] : r['offset'] + r['size']]

    return records


# ===========================================================================
# Palette (PAL)
#
# 768 bytes = 256 * 3 (RGB).
# Each channel is 0-63; multiply by 4 to get 0-255.
# ===========================================================================

def load_palette(pal_data):
    if len(pal_data) < 768:
        raise ValueError(f"PAL too small: {len(pal_data)} bytes (need 768)")
    return [
        (min(pal_data[i*3]   * 4, 255),
         min(pal_data[i*3+1] * 4, 255),
         min(pal_data[i*3+2] * 4, 255))
        for i in range(256)
    ]

def save_palette_swatch(palette, path):
    """Save a 256x32 colour swatch so you can visually inspect the palette."""
    if not PIL_AVAILABLE:
        return
    img = Image.new('RGB', (256, 32))
    px = img.load()
    for i, (r, g, b) in enumerate(palette):
        for y in range(32):
            px[i, y] = (r, g, b)
    img.save(path)


# ===========================================================================
# ICN sprite decoder
#
# File layout:
#   u16  n_sprites
#   u32  total_data_size  (= file_size - 6)
#   n_sprites * 12-byte sprite headers
#   sprite pixel data (contiguous)
#
# Sprite header (12 bytes):
#   s16  offsetX    -- hotspot offset for game engine (not canvas shift)
#   s16  offsetY
#   u16  width
#   u16  height
#   u32  packed     -- high byte = type (0=normal, 32=monochrome)
#                      low 24 bits = data_off (relative to file byte 6)
#
# Pixel encoding (normal sprites):
#   0x00        end of line; advance to start of next row
#   0x01-0x7F   literal run: next N bytes are palette colour indices
#   0x80        end of sprite data
#   0x81-0xBF   skip (N-0x80) transparent pixels
#   0xC0        shadow pixels: count in next byte (mod 4 if nonzero, else byte after)
#   0xC1        RLE: next byte = count, byte after = colour index
#   0xC2-0xFF   RLE: (N-0xC0) pixels of colour in next byte
#
# Pixel encoding (monochrome, type=32):
#   0x00        end of line
#   0x01-0x7F   N black pixels
#   0x80        end of sprite data
#   0x81-0xFF   skip (N-0x80) transparent pixels
# ===========================================================================

def decode_icn(icn_data, palette, out_dir):
    if not PIL_AVAILABLE:
        return
    if len(icn_data) < 6:
        raise ValueError("ICN file too small")

    pos = 0
    n_sprites, pos = u16(icn_data, pos)
    _total_size, pos = u32(icn_data, pos)

    if n_sprites == 0:
        return

    # Read 12-byte sprite headers
    headers = []
    for _ in range(n_sprites):
        ox,   pos = s16(icn_data, pos)
        oy,   pos = s16(icn_data, pos)
        w,    pos = u16(icn_data, pos)
        h,    pos = u16(icn_data, pos)
        pack, pos = u32(icn_data, pos)
        t   = (pack >> 24) & 0xFF
        dof =  pack & 0x00FFFFFF
        headers.append({'ox': ox, 'oy': oy, 'w': w, 'h': h, 'type': t, 'dof': dof})

    # Each sprite's data ends at the start of the next sprite's data.
    # Sprite headers may not be in the same order as their pixel data on disk,
    # so we find each sprite's end by sorting all dof values and taking the
    # next larger one, rather than blindly using headers[i+1]['dof'].
    sorted_dofs = sorted(set(h['dof'] for h in headers))

    def data_end(dof):
        for d in sorted_dofs:
            if d > dof:
                return 6 + d
        return len(icn_data)

    os.makedirs(out_dir, exist_ok=True)

    for idx, hdr in enumerate(headers):
        sw, sh = hdr['w'], hdr['h']
        if sw == 0 or sh == 0:
            continue

        # Canvas = exactly width x height.
        # offsetX/Y are stored in spec.xml for the game engine to use;
        # they don't affect the PNG canvas size.
        buf = bytearray(sw * sh * 4)  # RGBA, all zeros = fully transparent

        def set_px(px, py, r, g, b, a, _sw=sw, _sh=sh, _buf=buf):
            if 0 <= px < _sw and 0 <= py < _sh:
                base = (py * _sw + px) * 4
                _buf[base]   = r
                _buf[base+1] = g
                _buf[base+2] = b
                _buf[base+3] = a

        p     = 6 + hdr['dof']
        p_end = data_end(hdr['dof'])
        mono  = (hdr['type'] == 32)
        x, y  = 0, 0

        while p < p_end:
            cmd = icn_data[p]; p += 1

            if mono:
                if   cmd == 0x00:
                    x = 0; y += 1
                elif cmd == 0x80:
                    break
                elif 0x01 <= cmd <= 0x7F:
                    for _ in range(cmd):
                        set_px(x, y, 0, 0, 0, 255); x += 1
                else:
                    x += cmd - 0x80
            else:
                if cmd == 0x00:
                    x = 0; y += 1
                elif cmd == 0x80:
                    break
                elif 0x01 <= cmd <= 0x7F:
                    for _ in range(cmd):
                        if p >= p_end: break
                        ci = icn_data[p]; p += 1
                        cr, cg, cb = palette[ci]
                        set_px(x, y, cr, cg, cb, 255); x += 1
                elif 0x81 <= cmd <= 0xBF:
                    x += cmd - 0x80
                elif cmd == 0xC0:
                    if p >= p_end: break
                    nxt = icn_data[p]; p += 1
                    rem = nxt % 4
                    if rem != 0:
                        n_sh = rem
                    else:
                        if p >= p_end: break
                        n_sh = icn_data[p]; p += 1
                    for _ in range(n_sh):
                        set_px(x, y, 0, 0, 0, 64); x += 1
                elif cmd == 0xC1:
                    if p + 1 >= p_end: break
                    count = icn_data[p]; p += 1
                    ci    = icn_data[p]; p += 1
                    cr, cg, cb = palette[ci]
                    for _ in range(count):
                        set_px(x, y, cr, cg, cb, 255); x += 1
                else:
                    if p >= p_end: break
                    count = cmd - 0xC0
                    ci    = icn_data[p]; p += 1
                    cr, cg, cb = palette[ci]
                    for _ in range(count):
                        set_px(x, y, cr, cg, cb, 255); x += 1

        Image.frombytes('RGBA', (sw, sh), bytes(buf)).save(
            os.path.join(out_dir, f'{idx:04d}.png'))

    # Write spec.xml with offsets for reference
    with open(os.path.join(out_dir, 'spec.xml'), 'w') as f:
        f.write(f'<icn count="{n_sprites}">\n')
        for idx, hdr in enumerate(headers):
            f.write(
                f'  <sprite id="{idx}" file="{idx:04d}.png"'
                f' offsetX="{hdr["ox"]}" offsetY="{hdr["oy"]}"'
                f' width="{hdr["w"]}" height="{hdr["h"]}"'
                f' type="{hdr["type"]}"/>\n'
            )
        f.write('</icn>\n')


# ===========================================================================
# TIL tile decoder
#
# u16 n_tiles, u16 width, u16 height
# then n_tiles * width*height bytes of palette indices
# ===========================================================================

def decode_til(til_data, palette, out_dir):
    if not PIL_AVAILABLE:
        return
    if len(til_data) < 6:
        return
    pos = 0
    n_tiles, pos = u16(til_data, pos)
    width,   pos = u16(til_data, pos)
    height,  pos = u16(til_data, pos)
    if width == 0 or height == 0:
        return
    os.makedirs(out_dir, exist_ok=True)
    tile_sz = width * height
    for i in range(n_tiles):
        chunk = til_data[pos : pos + tile_sz]; pos += tile_sz
        img = Image.new('RGB', (width, height))
        img.putdata([palette[b] for b in chunk])
        img.save(os.path.join(out_dir, f'{i:04d}.png'))


# ===========================================================================
# BMP decoder (HoMM palette-indexed, NOT standard Windows BMP)
#
# Header: 0x21 0x00, u16 width, u16 height
# Data:   width*height palette indices (values 0, 1, or 2)
# ===========================================================================

def decode_homm_bmp(bmp_data, palette, out_path):
    if not PIL_AVAILABLE:
        return
    if len(bmp_data) < 6:
        return
    pos = 0
    magic_hi = bmp_data[pos]; magic_lo = bmp_data[pos+1]; pos += 2
    if magic_hi != 0x21 or magic_lo != 0x00:
        print(f"  WARNING: unexpected BMP magic 0x{magic_hi:02X} 0x{magic_lo:02X}")
    width,  pos = u16(bmp_data, pos)
    height, pos = u16(bmp_data, pos)
    if width == 0 or height == 0:
        return
    raw = bmp_data[pos : pos + width * height]
    img = Image.new('RGB', (width, height))
    img.putdata([palette[b] for b in raw])
    img.save(out_path)


# ===========================================================================
# Main extraction
# ===========================================================================

def extract(agg_path):
    agg_path = Path(agg_path)
    if not agg_path.exists():
        print(f"ERROR: file not found: {agg_path}")
        sys.exit(1)

    data = agg_path.read_bytes()
    out_root = Path(agg_path.stem)
    out_root.mkdir(exist_ok=True)

    print(f"Reading {agg_path} ...")
    records = parse_agg(data)

    # --- Load palette first (needed for all image decoding) ---
    palette = None
    for r in records:
        if r['name'].upper().endswith('.PAL'):
            print(f"Loading palette from '{r['name']}' ...")
            palette = load_palette(r['data'])
            swatch = out_root / 'palette_swatch.png'
            save_palette_swatch(palette, swatch)
            print(f"  palette swatch -> {swatch}")
            break  # use first PAL found (both are identical in HoMM1)

    if palette is None:
        print("WARNING: No .PAL file found - using greyscale fallback.")
        palette = [(i, i, i) for i in range(256)]

    # --- Extract all files ---
    for r in records:
        name  = r['name']
        fdata = r['data']

        if not name:
            continue

        ext      = Path(name).suffix.upper() if '.' in name else ''
        out_path = out_root / name

        if ext == '.ICN':
            sprite_dir = out_root / Path(name).stem
            print(f"  ICN  {name}  ->  {sprite_dir}/")
            try:
                decode_icn(fdata, palette, str(sprite_dir))
            except Exception as e:
                import traceback
                print(f"    ERROR: {e}")
                traceback.print_exc()

        elif ext == '.TIL':
            tile_dir = out_root / Path(name).stem
            print(f"  TIL  {name}  ->  {tile_dir}/")
            try:
                decode_til(fdata, palette, str(tile_dir))
            except Exception as e:
                print(f"    ERROR: {e}")

        elif ext == '.BMP':
            png_path = out_root / (Path(name).stem + '.png')
            print(f"  BMP  {name}  ->  {png_path}")
            try:
                decode_homm_bmp(fdata, palette, str(png_path))
            except Exception as e:
                print(f"    ERROR: {e}")

        elif ext == '.82M':
            print(f"  SND  {name}  ->  {out_path}")
            out_path.write_bytes(fdata)

        elif ext == '.PAL':
            print(f"  PAL  {name}  ->  {out_path}")
            out_path.write_bytes(fdata)

        else:
            print(f"  RAW  {name}  ->  {out_path}")
            out_path.write_bytes(fdata)

    print(f"\nDone. All files extracted to '{out_root}/'")


# ===========================================================================
# Entry point
# ===========================================================================

if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description='Extract and decode Heroes of Might and Magic I AGG archives.')
    parser.add_argument('agg_file', help='Path to heroes.agg')
    args = parser.parse_args()
    extract(args.agg_file)
