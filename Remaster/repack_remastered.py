#!/usr/bin/env python3
"""
LEGO Racers (1999) Remaster Repacker
Combines upscaled PNG textures + original non-texture assets into LEGO_REMASTERED.JAM.

Workflow:
  1. Run extract_assets.py  -> remaster_workspace/original_textures/
  2. Run Real-ESRGAN on the PNGs -> remaster_workspace/upscaled_textures/
     OR copy original_textures/ to upscaled_textures/ for a basic pass
  3. Run this script -> LEGO_REMASTERED.JAM (drop-in replacement for LEGO.JAM)

The game texture size limit is the same as the original — upscaled images are
downsampled back to their original dimensions and re-quantized to the original
palette. The visual difference comes from the AI upscale → downsample pipeline,
which produces much sharper, cleaner results than the original bilinear filtering.

Usage:
    python repack_remastered.py
    python repack_remastered.py --jam <LEGO.JAM> --upscaled <folder> --out <LEGO_REMASTERED.JAM>
"""

import argparse
import io
import os
import struct
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("ERROR: Pillow not installed. Run: pip install Pillow")
    sys.exit(1)

# Re-use decoders/encoders from extract_assets
sys.path.insert(0, str(Path(__file__).parent))
from extract_assets import (
    jam_extract_to_dict,
    decode_lego_bmp,
    encode_lego_bmp,
    decode_tga,
    encode_tga,
    SUPPORTED_TEXTURE_EXTS,
)


# ---------------------------------------------------------------------------
# JAM Builder (compatible with JrMasterModelBuilder/JAM-Extractor format)
# ---------------------------------------------------------------------------

def build_jam(files: dict[str, bytes]) -> bytes:
    """Build a LEGO.JAM archive from a {relative_path: bytes} dict."""

    # Normalize separators
    normalized = {}
    for k, v in files.items():
        normalized[k.replace("\\", "/")] = v

    # Organise into a tree
    tree: dict = {}  # tree[folder] = {files: [...], subdirs: [tree_node]}

    for rel_path in normalized:
        parts = rel_path.split("/")
        node = tree
        for part in parts[:-1]:
            node = node.setdefault(part, {})
        node.setdefault("__files__", []).append(parts[-1])

    out = bytearray(b"LJAM")

    # We'll do a BFS/DFS and fix up pointers in two passes
    # Using a simple approach: write directory index first, fix offsets after

    # Flatten the tree depth-first into a list of (path, [files], [subdirs]) entries
    entries = []  # (folder_prefix, files_list)

    def flatten(node, prefix):
        files_here = node.get("__files__", [])
        subdirs = [k for k in node if k != "__files__"]
        entries.append((prefix, files_here, subdirs))
        for sd in subdirs:
            flatten(node[sd], (prefix + "/" + sd) if prefix else sd)

    flatten(tree, "")

    # Pass 1: write structure, leaving file/folder offsets as 0 (to be patched)
    folder_offsets = {}   # folder_prefix -> offset in `out` where its block starts
    file_offset_slots = []  # (out_offset, rel_path) for patching

    for folder_prefix, files_here, subdirs in entries:
        folder_offsets[folder_prefix] = len(out)
        struct.pack_into("<I", out, len(out), len(files_here)) if False else None

        # Write file count
        out.extend(struct.pack("<I", len(files_here)))

        # Write file entries: 12-char name + offset(u32) + size(u32) = 20 bytes each
        for fname in files_here:
            rel = (folder_prefix + "/" + fname) if folder_prefix else fname
            # Write name (12 bytes, null-padded)
            name_bytes = fname.encode("ascii", errors="replace")[:12]
            out.extend(name_bytes.ljust(12, b"\x00"))
            # Placeholder offset
            file_offset_slots.append((len(out), rel))
            out.extend(struct.pack("<I", 0))  # offset placeholder
            size = len(normalized[rel])
            out.extend(struct.pack("<I", size))

        # Write subdir count
        out.extend(struct.pack("<I", len(subdirs)))

        # Write subdir entries: 12-char name + offset(u32) = 16 bytes each
        for sd in subdirs:
            sd_full = (folder_prefix + "/" + sd) if folder_prefix else sd
            name_bytes = sd.encode("ascii", errors="replace")[:12]
            out.extend(name_bytes.ljust(12, b"\x00"))
            # Placeholder for subdir block offset (patched in pass 2)
            # We need to record where this pointer lives so we can patch it
            file_offset_slots.append((len(out), "__dir__:" + sd_full))
            out.extend(struct.pack("<I", 0))

    # Patch directory block offsets
    for (slot_off, key) in file_offset_slots:
        if key.startswith("__dir__:"):
            dir_key = key[len("__dir__:"):]
            if dir_key in folder_offsets:
                struct.pack_into("<I", out, slot_off, folder_offsets[dir_key])

    # Pass 2: append file data and patch file offsets
    for (slot_off, key) in file_offset_slots:
        if not key.startswith("__dir__:"):
            data = normalized[key]
            file_start = len(out)
            struct.pack_into("<I", out, slot_off, file_start)
            out.extend(data)

    return bytes(out)


# ---------------------------------------------------------------------------
# Main repacking logic
# ---------------------------------------------------------------------------

def repack(jam_path: str, upscaled_dir: str, output_jam: str, verbose: bool = False):
    print(f"Loading original JAM: {jam_path}")
    original_files = jam_extract_to_dict(jam_path)
    print(f"  {len(original_files)} files loaded")

    upscaled_base = Path(upscaled_dir)
    replaced = 0
    kept     = 0
    failed   = 0

    remastered_files = {}

    for rel_path, original_data in original_files.items():
        ext = Path(rel_path).suffix.upper()

        if ext not in SUPPORTED_TEXTURE_EXTS:
            remastered_files[rel_path] = original_data
            kept += 1
            continue

        # Look for upscaled PNG
        png_path = upscaled_base / (rel_path + ".png")
        if not png_path.exists():
            # Fall back to original
            remastered_files[rel_path] = original_data
            kept += 1
            if verbose:
                print(f"  KEEP (no upscaled version): {rel_path}")
            continue

        try:
            upscaled_img = Image.open(str(png_path)).convert("RGB")

            if ext in (".BMP", ".TGB"):
                new_data = encode_lego_bmp(upscaled_img, original_data)
            elif ext == ".TGA":
                new_data = encode_tga(upscaled_img, original_data)
            else:
                new_data = original_data

            remastered_files[rel_path] = new_data
            replaced += 1

            if verbose:
                print(f"  REPLACED: {rel_path}")

        except Exception as e:
            print(f"  ERROR encoding {rel_path}: {e}")
            remastered_files[rel_path] = original_data
            failed += 1

    print(f"\nBuild summary:")
    print(f"  Textures replaced  : {replaced}")
    print(f"  Files kept original: {kept}")
    print(f"  Encode errors      : {failed}")

    print(f"\nBuilding JAM archive...")
    jam_data = build_jam(remastered_files)
    print(f"  Archive size: {len(jam_data):,} bytes ({len(jam_data)/1024/1024:.1f} MB)")

    out_path = Path(output_jam)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with open(str(out_path), "wb") as f:
        f.write(jam_data)

    print(f"  Written to: {output_jam}")
    print("\nDone! Copy LEGO_REMASTERED.JAM to your game folder and rename it to LEGO.JAM to use.")


def main():
    parser = argparse.ArgumentParser(
        description="LEGO Racers Remaster Repacker — builds LEGO_REMASTERED.JAM from upscaled textures"
    )
    parser.add_argument("--jam",      default=None)
    parser.add_argument("--upscaled", default=None, help="Folder containing upscaled PNGs")
    parser.add_argument("--out",      default=None, help="Output JAM path")
    parser.add_argument("--verbose", "-v", action="store_true")
    args = parser.parse_args()

    base_dir = os.path.join(
        os.path.expanduser("~"),
        "OneDrive", "Documents", "dc-lego-racers", "LegoRacersPortable"
    )
    default_jam      = os.path.join(base_dir, "LEGO.JAM")
    default_upscaled = os.path.join(base_dir, "remaster_workspace", "upscaled_textures")
    default_out      = os.path.join(base_dir, "LEGO_REMASTERED.JAM")

    jam_path    = args.jam      or default_jam
    upscaled    = args.upscaled or default_upscaled
    output_jam  = args.out      or default_out

    if not os.path.exists(jam_path):
        print(f"ERROR: LEGO.JAM not found: {jam_path}")
        sys.exit(1)
    if not os.path.exists(upscaled):
        print(f"ERROR: Upscaled textures folder not found: {upscaled}")
        print("Run extract_assets.py first, then upscale the PNG files, then run this script.")
        sys.exit(1)

    repack(jam_path, upscaled, output_jam, verbose=args.verbose)


if __name__ == "__main__":
    main()
