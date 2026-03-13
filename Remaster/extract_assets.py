#!/usr/bin/env python3
"""
LEGO Racers (1999) Asset Extractor
Extracts all textures from LEGO.JAM into PNG files for remastering.

Usage:
    python extract_assets.py --jam <path/to/LEGO.JAM> --out <output_folder>

Output structure mirrors JAM directory layout.
"""

import argparse
import os
import struct
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("ERROR: Pillow not installed. Run: pip install Pillow")
    sys.exit(1)


# ---------------------------------------------------------------------------
# JAM Extractor (embedded from JrMasterModelBuilder/JAM-Extractor)
# ---------------------------------------------------------------------------

def jam_extract_to_dict(jam_path: str) -> dict[str, bytes]:
    """Parse a LEGO.JAM file and return {relative_path: bytes} dict."""
    with open(jam_path, "rb") as f:
        data = f.read()

    if data[:4] != b"LJAM":
        raise ValueError(f"Not a LEGO JAM file: {jam_path}")

    def uint32(offset):
        return struct.unpack_from("<I", data, offset)[0]

    def read_name(offset):
        name = b""
        for b in data[offset:offset + 12]:
            if b == 0:
                break
            name += bytes([b])
        return name.decode("ascii", errors="replace")

    files = {}

    def recurse(folders):
        for folder_path, folder_offset in folders:
            total_files = uint32(folder_offset)
            if total_files == 0:
                sub_count = uint32(folder_offset + 4)
                subs = []
                for i in range(sub_count):
                    off = folder_offset + 8 + i * 16
                    name = read_name(off)
                    ptr = uint32(off + 12)
                    subs.append((folder_path + "/" + name if folder_path else name, ptr))
                recurse(subs)
            else:
                for i in range(total_files):
                    off = folder_offset + 4 + i * 20
                    name = read_name(off)
                    file_offset = uint32(off + 12)
                    file_size = uint32(off + 16)
                    rel_path = (folder_path + "/" + name if folder_path else name)
                    files[rel_path] = data[file_offset:file_offset + file_size]

                folder_count_pos = total_files * 20 + folder_offset + 4
                folder_count = uint32(folder_count_pos)
                if folder_count > 0:
                    subs = []
                    for i in range(folder_count):
                        off = folder_count_pos + 4 + i * 16
                        name = read_name(off)
                        ptr = uint32(off + 12)
                        subs.append((folder_path + "/" + name if folder_path else name, ptr))
                    recurse(subs)

    recurse([("", 4)])
    return files


# ---------------------------------------------------------------------------
# BMP Decoder  (custom LEGO Racers palette-indexed RLE format)
# ---------------------------------------------------------------------------

def decode_lego_bmp(data: bytes) -> Image.Image | None:
    """
    Decode a LEGO Racers custom BMP file.

    Header (6 bytes):
        byte[0]  : type (always 0x04)
        byte[1]  : palette entry count (0 = solid color fill)
        u16 LE   : width
        u16 LE   : height

    If num_colors == 0:
        bytes[6:9] = single RGB fill color

    If num_colors > 0:
        bytes[6 .. 6+num_colors*3] = RGB palette
        Remaining bytes = RLE-compressed pixel indices:
            (count_byte, index_byte) pairs; count 0 = 256 repetitions
    """
    if len(data) < 6:
        return None

    type_byte  = data[0]
    num_colors = data[1]
    width      = struct.unpack_from("<H", data, 2)[0]
    height     = struct.unpack_from("<H", data, 4)[0]

    if width == 0 or height == 0:
        return None

    # --- Solid color fill ---
    if num_colors == 0:
        if len(data) >= 9:
            r, g, b = data[6], data[7], data[8]
            return Image.new("RGB", (width, height), (r, g, b))
        return None

    # --- Palette ---
    palette = []
    p_start = 6
    for i in range(num_colors):
        off = p_start + i * 3
        if off + 2 >= len(data):
            break
        palette.append((data[off], data[off + 1], data[off + 2]))

    pixel_start   = p_start + num_colors * 3
    pixel_data    = data[pixel_start:]
    pixels_needed = width * height

    # --- Direct indexed (no compression) ---
    if len(pixel_data) >= pixels_needed:
        pixels = []
        for i in range(pixels_needed):
            idx = pixel_data[i]
            pixels.append(palette[idx] if idx < len(palette) else (0, 0, 0))
        img = Image.new("RGB", (width, height))
        img.putdata(pixels)
        return img

    # --- RLE compressed ---
    pixels = []
    i = 0
    while i + 1 < len(pixel_data) and len(pixels) < pixels_needed:
        count = pixel_data[i] if pixel_data[i] != 0 else 256
        idx   = pixel_data[i + 1]
        color = palette[idx] if idx < len(palette) else (0, 0, 0)
        pixels.extend([color] * count)
        i += 2

    if len(pixels) >= pixels_needed:
        img = Image.new("RGB", (width, height))
        img.putdata(pixels[:pixels_needed])
        return img

    return None


def encode_lego_bmp(img: Image.Image, original_data: bytes) -> bytes:
    """
    Re-encode an image back to LEGO Racers BMP format.
    Preserves the original palette structure (re-quantizes to original palette).

    For upscaled images: we quantize to the original palette and re-RLE encode.
    For solid-color textures: just update the RGB bytes.
    """
    if len(original_data) < 6:
        return original_data

    num_colors = original_data[1]
    orig_width  = struct.unpack_from("<H", original_data, 2)[0]
    orig_height = struct.unpack_from("<H", original_data, 4)[0]

    # Get target dimensions (original, since game format is fixed-size)
    # NOTE: Upscaled images should be downsampled back to original size here
    #       The game engine will not accept larger textures.
    img = img.resize((orig_width, orig_height), Image.LANCZOS)

    if num_colors == 0:
        # Solid color: sample dominant color from center of upscaled image
        center_pixel = img.getpixel((orig_width // 2, orig_height // 2))
        r, g, b = center_pixel[:3]
        result = bytearray(original_data)
        result[6], result[7], result[8] = r, g, b
        return bytes(result)

    # Extract original palette
    palette = []
    p_start = 6
    for i in range(num_colors):
        off = p_start + i * 3
        palette.append((original_data[off], original_data[off + 1], original_data[off + 2]))

    # Build PIL palette image from our palette
    # Quantize the upscaled (then downscaled back) image to the original palette
    palette_img = Image.new("P", (1, 1))
    flat_palette = []
    for r, g, b in palette:
        flat_palette.extend([r, g, b])
    # PIL needs 256 palette entries for quantize
    while len(flat_palette) < 768:
        flat_palette.extend([0, 0, 0])
    palette_img.putpalette(flat_palette)

    img_rgb = img.convert("RGB")
    quantized = img_rgb.quantize(colors=num_colors, palette=palette_img, dither=0)

    pixels = list(quantized.getdata())
    pixels_needed = orig_width * orig_height

    # RLE encode
    pixel_data_orig = original_data[p_start + num_colors * 3:]
    was_rle = len(pixel_data_orig) < pixels_needed

    header = bytearray(original_data[:p_start + num_colors * 3])

    if not was_rle:
        # Direct encoding
        header.extend(pixels[:pixels_needed])
        return bytes(header)
    else:
        # RLE encoding
        rle = bytearray()
        i = 0
        while i < pixels_needed:
            current = pixels[i]
            count = 1
            while i + count < pixels_needed and pixels[i + count] == current and count < 255:
                count += 1
            rle.append(count)
            rle.append(current)
            i += count
        header.extend(rle)
        return bytes(header)


# ---------------------------------------------------------------------------
# TGA: standard format, PIL handles it natively
# ---------------------------------------------------------------------------

def decode_tga(data: bytes) -> Image.Image | None:
    import io
    try:
        return Image.open(io.BytesIO(data))
    except Exception:
        return None


def encode_tga(img: Image.Image, original_data: bytes) -> bytes:
    """Re-encode image as TGA, preserving original dimensions."""
    import io
    with open("/dev/null", "rb") as _:
        pass
    orig = decode_tga(original_data)
    if orig:
        img = img.resize(orig.size, Image.LANCZOS)
        if orig.mode == "RGBA":
            img = img.convert("RGBA")
        elif orig.mode == "RGB":
            img = img.convert("RGB")
    buf = io.BytesIO()
    img.save(buf, format="TGA")
    return buf.getvalue()


# ---------------------------------------------------------------------------
# Main extraction logic
# ---------------------------------------------------------------------------

SUPPORTED_TEXTURE_EXTS = {".BMP", ".TGA", ".TGB"}


def extract_textures(jam_path: str, output_dir: str, verbose: bool = False):
    """Extract all textures from LEGO.JAM to PNG files."""
    print(f"Loading {jam_path}...")
    all_files = jam_extract_to_dict(jam_path)
    print(f"  Found {len(all_files)} files in archive")

    out_base = Path(output_dir)
    out_base.mkdir(parents=True, exist_ok=True)

    extracted    = 0
    skipped      = 0
    failed       = 0
    total_pixels = 0

    for rel_path, file_data in all_files.items():
        ext = Path(rel_path).suffix.upper()

        if ext not in SUPPORTED_TEXTURE_EXTS:
            skipped += 1
            continue

        # Build output path, changing extension to .png
        png_rel  = rel_path + ".png"
        out_path = out_base / png_rel
        out_path.parent.mkdir(parents=True, exist_ok=True)

        # Decode
        img = None
        if ext in (".BMP", ".TGB"):
            img = decode_lego_bmp(file_data)
        elif ext == ".TGA":
            img = decode_tga(file_data)

        if img is None:
            if verbose:
                print(f"  SKIP (decode failed): {rel_path}")
            failed += 1
            continue

        img.save(str(out_path))
        total_pixels += img.width * img.height
        extracted += 1

        if verbose:
            print(f"  {rel_path} -> {img.size[0]}x{img.size[1]}")

    print(f"\nExtraction complete:")
    print(f"  Textures extracted : {extracted}")
    print(f"  Non-texture files  : {skipped}")
    print(f"  Failed             : {failed}")
    print(f"  Total pixels       : {total_pixels:,}")
    print(f"  Output folder      : {output_dir}")
    return extracted


def main():
    parser = argparse.ArgumentParser(
        description="LEGO Racers (1999) Asset Extractor — exports textures to PNG for remastering"
    )
    parser.add_argument("--jam",  default=None, help="Path to LEGO.JAM")
    parser.add_argument("--out",  default=None, help="Output folder for PNG files")
    parser.add_argument("--verbose", "-v", action="store_true")
    args = parser.parse_args()

    # Auto-detect game path
    default_jam = os.path.join(
        os.path.expanduser("~"),
        "OneDrive", "Documents", "dc-lego-racers", "LegoRacersPortable", "LEGO.JAM"
    )
    jam_path = args.jam or default_jam

    if not os.path.exists(jam_path):
        print(f"ERROR: LEGO.JAM not found at: {jam_path}")
        print("Use --jam to specify its location.")
        sys.exit(1)

    default_out = str(Path(jam_path).parent / "remaster_workspace" / "original_textures")
    out_dir = args.out or default_out

    extract_textures(jam_path, out_dir, verbose=args.verbose)


if __name__ == "__main__":
    main()
