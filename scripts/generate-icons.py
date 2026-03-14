#!/usr/bin/env python3

from __future__ import annotations

import shutil
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
OUTPUT_DIR = ROOT / "assets" / "icons"
SOURCE_SVG = OUTPUT_DIR / "sure-smartie-gui.svg"
SIZES = (16, 24, 32, 48, 64, 128, 256, 512)


def find_renderer() -> list[str]:
    for command in ("inkscape", "rsvg-convert"):
        executable = shutil.which(command)
        if executable is not None:
            return [executable]
    raise SystemExit(
        "No SVG renderer found. Install inkscape or rsvg-convert to regenerate icons."
    )


def render_with_inkscape(executable: str, size: int, output_path: Path):
    subprocess.run(
        [
            executable,
            str(SOURCE_SVG),
            "--export-type=png",
            f"--export-filename={output_path}",
            f"--export-width={size}",
            f"--export-height={size}",
        ],
        check=True,
    )


def render_with_rsvg_convert(executable: str, size: int, output_path: Path):
    subprocess.run(
        [
            executable,
            "--keep-aspect-ratio",
            "--width",
            str(size),
            "--height",
            str(size),
            "--output",
            str(output_path),
            str(SOURCE_SVG),
        ],
        check=True,
    )


def main():
    if not SOURCE_SVG.exists():
        raise SystemExit(f"Source SVG not found: {SOURCE_SVG}")

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    renderer = find_renderer()

    for size in SIZES:
        output_path = OUTPUT_DIR / f"sure-smartie-gui-{size}.png"
        if Path(renderer[0]).name == "inkscape":
            render_with_inkscape(renderer[0], size, output_path)
        else:
            render_with_rsvg_convert(renderer[0], size, output_path)


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as error:
        print(f"Icon generation failed: {error}", file=sys.stderr)
        raise SystemExit(error.returncode) from error
