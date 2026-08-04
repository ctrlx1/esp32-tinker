#!/usr/bin/env python3
"""Split 320x320 pixel art sheets into 32x32 PNG frames."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError as exc:
    raise SystemExit(
        "Pillow is required to split pixel sheets. Install it with: python3 -m pip install Pillow"
    ) from exc


TILE_SIZE = 32
GRID_COLUMNS = 10
GRID_ROWS = 10
FRAMES_PER_SHEET = GRID_COLUMNS * GRID_ROWS
IMAGE_EXTENSIONS = {".bmp", ".gif", ".jpg", ".jpeg", ".png", ".webp"}

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SOURCE_DIR = ROOT / "pixel-sheets-320"
DEFAULT_OUTPUT_DIR = ROOT / "pixel-art"


def natural_key(path: Path) -> list[object]:
    return [
        int(part) if part.isdigit() else part.lower()
        for part in re.split(r"(\d+)", path.name)
    ]


def source_images(source_dir: Path) -> list[Path]:
    if not source_dir.exists():
        raise FileNotFoundError(f"Source directory does not exist: {source_dir}")

    return sorted(
        (
            path
            for path in source_dir.iterdir()
            if path.is_file() and path.suffix.lower() in IMAGE_EXTENSIONS
        ),
        key=natural_key,
    )


def split_sheet(sheet_path: Path, output_dir: Path) -> int:
    expected_width = TILE_SIZE * GRID_COLUMNS
    expected_height = TILE_SIZE * GRID_ROWS

    with Image.open(sheet_path) as image:
        if image.width != expected_width:
            raise ValueError(
                f"{sheet_path}: expected width {expected_width}, got {image.width}"
            )
        if image.height != expected_height:
            raise ValueError(
                f"{sheet_path}: expected height {expected_height}, got {image.height}"
            )

        rgba = image.convert("RGBA")
        for row in range(GRID_ROWS):
            for column in range(GRID_COLUMNS):
                frame_index = row * GRID_COLUMNS + column + 1
                x = column * TILE_SIZE
                y = row * TILE_SIZE
                tile = rgba.crop((x, y, x + TILE_SIZE, y + TILE_SIZE))
                output_path = output_dir / f"{sheet_path.stem}-{frame_index:02d}.png"
                tile.save(output_path)

    return FRAMES_PER_SHEET


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Split every image in pixel-sheets-320 into 100 32x32 frames, "
            "saved as <sheet-name>-01.png through <sheet-name>-100.png."
        )
    )
    parser.add_argument(
        "--source-dir",
        type=Path,
        default=DEFAULT_SOURCE_DIR,
        help="Directory containing 320x320 sheets (default: pixel-sheets-320)",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=DEFAULT_OUTPUT_DIR,
        help="Directory to write 32x32 PNG frames (default: pixel-art)",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    output_dir = args.output_dir
    output_dir.mkdir(parents=True, exist_ok=True)

    images = source_images(args.source_dir)
    written = 0
    for image_path in images:
        written += split_sheet(image_path, output_dir)

    print(f"Split {len(images)} sheet(s) into {written} image(s) in {output_dir.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
