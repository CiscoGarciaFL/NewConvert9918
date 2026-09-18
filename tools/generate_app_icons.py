#!/usr/bin/env python3
"""Render the application SVG and package native Windows/macOS icons."""

from __future__ import annotations

import argparse
import shutil
import struct
import subprocess
from pathlib import Path


PNG_SIZES = (16, 24, 32, 48, 64, 128, 256, 512, 1024)
ICO_SIZES = (16, 24, 32, 48, 64, 128, 256)
ICNS_CHUNKS = (
    (b"icp4", 16),
    (b"ic11", 32),
    (b"icp5", 32),
    (b"ic12", 64),
    (b"icp6", 64),
    (b"ic07", 128),
    (b"ic13", 256),
    (b"ic08", 256),
    (b"ic14", 512),
    (b"ic09", 512),
    (b"ic10", 1024),
)


def render_pngs(inkscape: str, source: Path, output_dir: Path) -> dict[int, Path]:
    output_dir.mkdir(parents=True, exist_ok=True)
    rendered: dict[int, Path] = {}
    for size in PNG_SIZES:
        output = output_dir / f"NewConvert9918-{size}.png"
        subprocess.run(
            [
                inkscape,
                str(source),
                "--export-type=png",
                f"--export-filename={output}",
                f"--export-width={size}",
                f"--export-height={size}",
            ],
            check=True,
        )
        rendered[size] = output
    return rendered


def write_ico(rendered: dict[int, Path], output: Path) -> None:
    images = [(size, rendered[size].read_bytes()) for size in ICO_SIZES]
    header_size = 6 + 16 * len(images)
    offset = header_size
    entries = bytearray()
    payload = bytearray()

    for size, image in images:
        encoded_size = 0 if size == 256 else size
        entries.extend(
            struct.pack(
                "<BBBBHHII",
                encoded_size,
                encoded_size,
                0,
                0,
                1,
                32,
                len(image),
                offset,
            )
        )
        payload.extend(image)
        offset += len(image)

    output.write_bytes(struct.pack("<HHH", 0, 1, len(images)) + entries + payload)


def write_icns(rendered: dict[int, Path], output: Path) -> None:
    chunks = bytearray()
    for chunk_type, size in ICNS_CHUNKS:
        image = rendered[size].read_bytes()
        chunks.extend(chunk_type)
        chunks.extend(struct.pack(">I", len(image) + 8))
        chunks.extend(image)
    output.write_bytes(b"icns" + struct.pack(">I", len(chunks) + 8) + chunks)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--inkscape", help="Path to the Inkscape CLI executable")
    args = parser.parse_args()

    project_root = Path(__file__).resolve().parents[1]
    asset_dir = project_root / "app" / "assets"
    source = asset_dir / "NewConvert9918.svg"
    output_dir = asset_dir / "icons"
    inkscape = args.inkscape or shutil.which("inkscape") or shutil.which("inkscape.com")
    if inkscape is None:
        raise SystemExit("Inkscape was not found; pass --inkscape with its executable path.")
    if not source.is_file():
        raise SystemExit(f"Application logo was not found: {source}")

    rendered = render_pngs(inkscape, source, output_dir)
    write_ico(rendered, output_dir / "NewConvert9918.ico")
    write_icns(rendered, output_dir / "NewConvert9918.icns")
    print(f"Generated application icons in {output_dir}")


if __name__ == "__main__":
    main()
