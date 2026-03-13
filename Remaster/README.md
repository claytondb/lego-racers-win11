# LEGO Racers Remaster Toolkit

Extracts, upscales, and repacks all game textures using AI (Real-ESRGAN), with a launcher toggle to switch between original and remastered assets at any time.

## What gets remastered

| Asset type | Count | Description |
|---|---|---|
| BMP textures | ~1,616 | All in-game surfaces, characters, cars, tracks, UI |
| TGA textures | 9 | Transparent particle effects (smoke, flame, lightning) |
| **Total** | **~1,625** | Covering all visual surfaces in the game |

Texture sizes: mostly 64×64 and 32×32 — small originals that benefit most from AI upscaling.

## How it works

```
LEGO.JAM
   │
   ▼ extract_assets.py
original_textures/  (PNG files, mirroring JAM folder structure)
   │
   ▼ upscale_textures.py  (Real-ESRGAN, GPU-accelerated)
upscaled_textures/  (4× larger PNGs, AI-sharpened)
   │
   ▼ repack_remastered.py
LEGO_REMASTERED.JAM  (drop-in replacement for LEGO.JAM)
```

The game engine can't load larger textures than the originals, so upscaled images are downsampled back to original size and re-quantized to the original palette. The improvement comes from the AI upscale → downsample pipeline producing much cleaner, sharper detail than the original bilinear filtering.

## Setup

### Requirements

- Python 3.10+ with Pillow (`pip install Pillow`)
- [Real-ESRGAN Windows binary](https://github.com/xinntao/Real-ESRGAN/releases) — download `realesrgan-ncnn-vulkan-*-windows.zip`
  - Extract to `C:\tools\realesrgan\` or `%USERPROFILE%\Downloads\realesrgan-ncnn-vulkan\`

## Running the pipeline

Double-click `remaster_workflow.bat` and follow the menu, or run steps individually:

```batch
# Step 1: Extract all textures to PNG
python extract_assets.py

# Step 2: AI upscale (requires Real-ESRGAN)
python upscale_textures.py

# Step 3: Rebuild the JAM
python repack_remastered.py
```

This produces `LEGO_REMASTERED.JAM` in your game folder.

## Using in-game

Launch the game via `LegoController.exe`. The launcher now shows a menu:

```
╔══════════════════════════════════════╗
║        LEGO Racers Launcher          ║
╠══════════════════════════════════════╣
║  Textures: Original                  ║
╠══════════════════════════════════════╣
║  [Enter]  Launch game                ║
║  [R]      Switch to Remastered       ║
║  [Esc]    Exit                       ║
╚══════════════════════════════════════╝
```

Press `R` before launching to switch to remastered textures, or `O` to switch back. The setting is remembered between sessions.

## Choosing a Real-ESRGAN model

| Model | Best for | Speed |
|---|---|---|
| `realesr-animevideov3` | **Game textures (recommended)** — clean, cartoon-like | Fast |
| `realesrgan-x4plus-anime` | Anime/illustration style | Medium |
| `realesrgan-x4plus` | Photos / realistic textures | Medium |

Override with: `python upscale_textures.py --model realesrgan-x4plus-anime`

## Manual texture editing

Want to hand-paint specific textures? The workflow supports it:

1. Run `extract_assets.py` to get PNGs
2. Edit any PNG in `original_textures/` using Photoshop, Aseprite, etc.
3. Copy the edited PNGs to `upscaled_textures/` (run upscale for the rest)
4. Run `repack_remastered.py` to rebuild the JAM

The repacker uses the upscaled folder as the source of truth. Any PNG present there replaces the original; files missing from upscaled fall back to the original game asset.

## File structure

```
Remaster/
├── extract_assets.py       # Step 1: JAM → PNG
├── upscale_textures.py     # Step 2: PNG → upscaled PNG (Real-ESRGAN)
├── repack_remastered.py    # Step 3: upscaled PNG → LEGO_REMASTERED.JAM
├── remaster_workflow.bat   # Menu-driven launcher for all steps
└── README.md               # This file
```

## Known limitations

- **Texture sizes are fixed**: The game hardcodes texture dimensions. We upscale for quality improvement during the upscale→downsample pipeline, but can't increase actual in-game resolution.
- **3D model geometry**: Polygon counts are unchanged. Only textures are remastered.
- **Palette constraint**: BMP textures use an indexed color palette (up to 255 colors). Re-quantization to the original palette is automatic but some color fidelity may be lost in complex gradients.
- **Audio (TUN files)**: Not touched. Audio remastering is a separate effort.
