#!/usr/bin/env python3
"""
LEGO Racers (1999) Texture Upscaler
Runs Real-ESRGAN on extracted PNG textures.

Requirements:
  - Download realesrgan-ncnn-vulkan from:
    https://github.com/xinntao/Real-ESRGAN/releases
    (Windows binary: realesrgan-ncnn-vulkan-*-windows.zip)
  - Extract to a folder, set REALESRGAN_PATH below or pass --esrgan

Usage:
    python upscale_textures.py
    python upscale_textures.py --input original_textures --output upscaled_textures --esrgan C:/tools/realesrgan/realesrgan-ncnn-vulkan.exe

Models (best for pixel/game art):
    realesrgan-x4plus        General photos (good for backgrounds)
    realesrgan-x4plus-anime  Anime/cartoon style (best for LEGO textures)
    realesr-animevideov3     Anime video — fast, great for game textures
"""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path


# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

# Set this to your realesrgan-ncnn-vulkan.exe path, or pass --esrgan
DEFAULT_ESRGAN_PATHS = [
    r"C:\tools\realesrgan\realesrgan-ncnn-vulkan.exe",
    r"C:\Users\clayt\Downloads\realesrgan-ncnn-vulkan\realesrgan-ncnn-vulkan.exe",
    r"C:\Users\clayt\AppData\Local\realesrgan\realesrgan-ncnn-vulkan.exe",
]


def find_esrgan() -> str | None:
    for p in DEFAULT_ESRGAN_PATHS:
        if os.path.exists(p):
            return p
    # Search PATH
    found = shutil.which("realesrgan-ncnn-vulkan")
    if found:
        return found
    return None


# ---------------------------------------------------------------------------
# Upscaling
# ---------------------------------------------------------------------------

def upscale_folder(input_dir: str, output_dir: str, esrgan_exe: str,
                   model: str = "realesr-animevideov3", scale: int = 4,
                   verbose: bool = False):
    """
    Run Real-ESRGAN on every PNG in input_dir, saving results to output_dir.
    Mirrors the folder structure.
    """
    input_base  = Path(input_dir)
    output_base = Path(output_dir)
    output_base.mkdir(parents=True, exist_ok=True)

    # Find all PNG files
    all_pngs = list(input_base.rglob("*.png"))
    print(f"Found {len(all_pngs)} PNG textures to upscale")
    print(f"Using model: {model}  scale: {scale}x")
    print(f"Real-ESRGAN: {esrgan_exe}")
    print()

    # Real-ESRGAN can process entire folders at once — much faster than file-by-file
    # But we need to mirror the subfolder structure ourselves since ESRGAN
    # flattens output. We'll process per-subdirectory.

    subdirs = set(p.parent for p in all_pngs)
    total_processed = 0

    for subdir in sorted(subdirs):
        rel_subdir = subdir.relative_to(input_base)
        out_subdir = output_base / rel_subdir
        out_subdir.mkdir(parents=True, exist_ok=True)

        png_files = list(subdir.glob("*.png"))
        if not png_files:
            continue

        if verbose:
            print(f"Processing {rel_subdir} ({len(png_files)} files)...")

        cmd = [
            esrgan_exe,
            "-i", str(subdir),
            "-o", str(out_subdir),
            "-n", model,
            "-s", str(scale),
            "-f", "png",
        ]

        try:
            result = subprocess.run(
                cmd,
                capture_output=not verbose,
                text=True,
                timeout=300  # 5 min per folder
            )
            if result.returncode != 0:
                print(f"  WARNING: Real-ESRGAN returned {result.returncode} for {rel_subdir}")
                if result.stderr:
                    print(f"  STDERR: {result.stderr[:200]}")
            else:
                total_processed += len(png_files)
                if not verbose:
                    print(f"  [{total_processed}/{len(all_pngs)}] {rel_subdir} ✓")
        except subprocess.TimeoutExpired:
            print(f"  TIMEOUT: {rel_subdir}")
        except Exception as e:
            print(f"  ERROR: {rel_subdir}: {e}")

    print(f"\nUpscaling complete: {total_processed}/{len(all_pngs)} files processed")
    print(f"Output: {output_dir}")


def main():
    parser = argparse.ArgumentParser(
        description="Upscale extracted LEGO Racers textures using Real-ESRGAN"
    )
    parser.add_argument(
        "--input", "-i", default=None,
        help="Folder of extracted PNG textures (from extract_assets.py)"
    )
    parser.add_argument(
        "--output", "-o", default=None,
        help="Output folder for upscaled textures"
    )
    parser.add_argument(
        "--esrgan", default=None,
        help="Path to realesrgan-ncnn-vulkan.exe"
    )
    parser.add_argument(
        "--model", default="realesr-animevideov3",
        choices=["realesrgan-x4plus", "realesrgan-x4plus-anime", "realesr-animevideov3"],
        help="Real-ESRGAN model to use (default: realesr-animevideov3)"
    )
    parser.add_argument(
        "--scale", type=int, default=4,
        choices=[2, 3, 4],
        help="Upscale factor (default: 4)"
    )
    parser.add_argument("--verbose", "-v", action="store_true")
    args = parser.parse_args()

    # Find Real-ESRGAN
    esrgan = args.esrgan or find_esrgan()
    if not esrgan or not os.path.exists(esrgan):
        print("ERROR: realesrgan-ncnn-vulkan.exe not found!")
        print()
        print("Download it from:")
        print("  https://github.com/xinntao/Real-ESRGAN/releases")
        print("  (Look for: realesrgan-ncnn-vulkan-*-windows.zip)")
        print()
        print("Then either:")
        print("  1. Extract to one of these paths:")
        for p in DEFAULT_ESRGAN_PATHS:
            print(f"       {p}")
        print("  2. Or pass --esrgan <path/to/realesrgan-ncnn-vulkan.exe>")
        sys.exit(1)

    base_dir = os.path.join(
        os.path.expanduser("~"),
        "OneDrive", "Documents", "dc-lego-racers", "LegoRacersPortable",
        "remaster_workspace"
    )

    input_dir  = args.input  or os.path.join(base_dir, "original_textures")
    output_dir = args.output or os.path.join(base_dir, "upscaled_textures")

    if not os.path.exists(input_dir):
        print(f"ERROR: Input folder not found: {input_dir}")
        print("Run extract_assets.py first.")
        sys.exit(1)

    upscale_folder(input_dir, output_dir, esrgan,
                   model=args.model, scale=args.scale, verbose=args.verbose)


if __name__ == "__main__":
    main()
