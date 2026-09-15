#!/usr/bin/env python3
"""Generate the deterministic portion of the Phase 2 source-image corpus."""

from __future__ import annotations

import hashlib
import json
import struct
import zlib
from collections.abc import Iterable
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CORPUS = ROOT / "tests" / "golden"
SOURCE = CORPUS / "source"
MALFORMED = CORPUS / "malformed"
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


GENERATED_FIXTURES = {
    "photo-landscape.png": {
        "purpose": ["photographic", "broad-color-range", "landscape", "large-source"],
        "prompt": (
            "Original photographic landscape of an outdoor public garden after rain, "
            "with broad color range, natural texture, highlights, shadows, and no people, "
            "logos, readable text, watermark, frame, or border."
        ),
    },
    "pixel-art-portrait.png": {
        "purpose": ["pixel-art", "hard-edges", "portrait"],
        "prompt": (
            "Original portrait pixel-art coastal town with a lighthouse, boats, clouds, "
            "rooftops, and water; crisp hard edges, limited varied palette, no antialiasing, "
            "gradients, franchises, logos, text, watermark, frame, or border."
        ),
    },
    "flat-cartoon-square.png": {
        "purpose": ["cartoon", "flat-regions", "square"],
        "prompt": (
            "Original flat-color cartoon of a whimsical robot watering oversized plants in "
            "a greenhouse; clean outlines, mostly solid fills, simple shadows, no logos, "
            "readable text, watermark, frame, or border."
        ),
    },
}


def png_chunk(kind: bytes, payload: bytes) -> bytes:
    body = kind + payload
    return struct.pack(">I", len(payload)) + body + struct.pack(">I", zlib.crc32(body))


def write_png(
    path: Path,
    width: int,
    height: int,
    bit_depth: int,
    color_type: int,
    rows: Iterable[bytes],
) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    header = struct.pack(">IIBBBBB", width, height, bit_depth, color_type, 0, 0, 0)
    compressor = zlib.compressobj(level=9)

    with path.open("wb") as output:
        output.write(PNG_SIGNATURE)
        output.write(png_chunk(b"IHDR", header))
        for row in rows:
            compressed = compressor.compress(row)
            if compressed:
                output.write(png_chunk(b"IDAT", compressed))
        compressed = compressor.flush()
        if compressed:
            output.write(png_chunk(b"IDAT", compressed))
        output.write(png_chunk(b"IEND", b""))


def greyscale_rows(width: int, height: int) -> Iterable[bytes]:
    for y in range(height):
        row = bytearray([0])
        for x in range(width):
            ramp = (3 * x * 255 // (width - 1) + y * 255 // (height - 1)) // 4
            checker = 22 if ((x // 16) + (y // 16)) % 2 else -22
            row.append(max(0, min(255, ramp + checker)))
        yield bytes(row)


def monochrome_rows(width: int, height: int) -> Iterable[bytes]:
    for y in range(height):
        row = bytearray([0])
        packed = 0
        bits = 0
        for x in range(width):
            value = ((x // 3) ^ (y // 5) ^ ((x + y) // 17)) & 1
            packed = (packed << 1) | value
            bits += 1
            if bits == 8:
                row.append(packed)
                packed = 0
                bits = 0
        if bits:
            row.append(packed << (8 - bits))
        yield bytes(row)


def tiny_rgba_rows(width: int, height: int) -> Iterable[bytes]:
    palette = (
        (0, 0, 0, 0),
        (255, 255, 255, 255),
        (255, 0, 0, 255),
        (0, 255, 0, 192),
        (0, 0, 255, 128),
        (255, 255, 0, 64),
    )
    for y in range(height):
        row = bytearray([0])
        for x in range(width):
            row.extend(palette[(x + 2 * y) % len(palette)])
        yield bytes(row)


def transparency_rows(width: int, height: int) -> Iterable[bytes]:
    center_x = (width - 1) / 2
    center_y = (height - 1) / 2
    max_distance = (center_x**2 + center_y**2) ** 0.5
    for y in range(height):
        row = bytearray([0])
        for x in range(width):
            distance = ((x - center_x) ** 2 + (y - center_y) ** 2) ** 0.5
            alpha = max(0, min(255, round(255 * (1 - distance / max_distance))))
            row.extend((x * 255 // (width - 1), y * 255 // (height - 1), (x ^ y) & 0xFF, alpha))
        yield bytes(row)


def large_rgb_rows(width: int, height: int) -> Iterable[bytes]:
    red = [x * 255 // (width - 1) for x in range(width)]
    for y in range(height):
        green = y * 255 // (height - 1)
        row = bytearray([0])
        for x in range(width):
            blue = ((x // 16) ^ (y // 16)) & 0xFF
            row.extend((red[x], green, blue))
        yield bytes(row)


def read_png_header(path: Path) -> dict[str, int | str]:
    data = path.read_bytes()
    if len(data) < 33 or data[:8] != PNG_SIGNATURE or data[12:16] != b"IHDR":
        raise ValueError(f"{path} is not a complete PNG with an IHDR chunk")
    width, height, bit_depth, color_type = struct.unpack(">IIBB", data[16:26])
    color_names = {0: "greyscale", 2: "rgb", 3: "indexed", 4: "greyscale-alpha", 6: "rgba"}
    return {
        "width": width,
        "height": height,
        "bit_depth": bit_depth,
        "color_type": color_names.get(color_type, f"unknown-{color_type}"),
    }


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def generate() -> None:
    SOURCE.mkdir(parents=True, exist_ok=True)
    MALFORMED.mkdir(parents=True, exist_ok=True)

    write_png(SOURCE / "greyscale-gradient.png", 257, 193, 8, 0, greyscale_rows(257, 193))
    write_png(SOURCE / "black-white-pattern.png", 257, 193, 1, 0, monochrome_rows(257, 193))
    write_png(SOURCE / "tiny-rgba.png", 8, 6, 8, 6, tiny_rgba_rows(8, 6))
    write_png(SOURCE / "transparency-rgba.png", 320, 240, 8, 6, transparency_rows(320, 240))
    write_png(SOURCE / "large-pattern.png", 4096, 3072, 8, 2, large_rgb_rows(4096, 3072))

    (MALFORMED / "empty.png").write_bytes(b"")
    (MALFORMED / "invalid-signature.png").write_bytes(b"not a png\r\n")
    truncated_header = png_chunk(b"IHDR", struct.pack(">IIBBBBB", 16, 16, 8, 6, 0, 0, 0))
    (MALFORMED / "truncated.png").write_bytes(
        PNG_SIGNATURE + truncated_header + struct.pack(">I", 128) + b"IDAT" + b"\x78\x9c"
    )

    entries: list[dict[str, object]] = []
    for filename, metadata in GENERATED_FIXTURES.items():
        path = SOURCE / filename
        if not path.exists():
            raise FileNotFoundError(
                f"Missing generated fixture {path}; see tests/golden/README.md for its prompt."
            )
        entries.append(
            {
                "path": path.relative_to(ROOT).as_posix(),
                "origin": "OpenAI image generation for New Convert 9918",
                **metadata,
                **read_png_header(path),
                "sha256": sha256(path),
            }
        )

    procedural = {
        "greyscale-gradient.png": ["greyscale", "gradient", "non-power-of-two"],
        "black-white-pattern.png": ["black-and-white", "one-bit", "hard-edges"],
        "tiny-rgba.png": ["very-small", "rgba", "alpha-extremes"],
        "transparency-rgba.png": ["transparency", "alpha-gradient"],
        "large-pattern.png": ["very-large", "rgb", "allocation-boundary"],
    }
    for filename, purpose in procedural.items():
        path = SOURCE / filename
        entries.append(
            {
                "path": path.relative_to(ROOT).as_posix(),
                "origin": "Deterministically generated by tools/generate_test_corpus.py",
                "purpose": purpose,
                **read_png_header(path),
                "sha256": sha256(path),
            }
        )

    malformed = []
    for filename, failure in (
        ("empty.png", "empty input"),
        ("invalid-signature.png", "invalid PNG signature"),
        ("truncated.png", "truncated IDAT chunk"),
    ):
        path = MALFORMED / filename
        malformed.append(
            {
                "path": path.relative_to(ROOT).as_posix(),
                "expected": "reject",
                "failure_case": failure,
                "size": path.stat().st_size,
                "sha256": sha256(path),
            }
        )

    manifest = {
        "schema_version": 1,
        "license": "Project LICENSE; all fixtures were created specifically for New Convert 9918.",
        "source_images": entries,
        "malformed_inputs": malformed,
    }
    (CORPUS / "corpus.json").write_text(
        json.dumps(manifest, indent=2, ensure_ascii=True) + "\n", encoding="utf-8"
    )


if __name__ == "__main__":
    generate()
