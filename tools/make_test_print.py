#!/usr/bin/env python3
"""Generate raw 384-dot MiniPrinter test images.

Output format is raw binary lines:
- 384 dots per line
- 48 bytes per line
- monochrome, MSB-left inside each byte
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path


DOTS_PER_LINE = 384
BYTES_PER_LINE = 48


def dot_is_black(pattern: str, x: int, y: int) -> bool:
    """Return whether one dot should be black."""
    if pattern == "blank":
        return False
    if pattern == "checker":
        return ((x // 8) + (y // 8)) % 2 == 0
    if pattern == "vertical_lines":
        return (x % 16) < 2
    if pattern == "low_density_black":
        return ((x + y * 7) % 32) == 0
    raise ValueError(f"unsupported pattern: {pattern}")


def make_line(pattern: str, y: int) -> bytes:
    """Generate one 384-dot line as 48 MSB-left bytes."""
    data = bytearray(BYTES_PER_LINE)
    for x in range(DOTS_PER_LINE):
        if dot_is_black(pattern, x, y):
            data[x // 8] |= 0x80 >> (x % 8)
    return bytes(data)


def make_image(pattern: str, lines: int) -> bytes:
    """Generate a raw image containing N fixed-width lines."""
    if lines <= 0:
        raise ValueError("lines must be greater than 0")
    return b"".join(make_line(pattern, y) for y in range(lines))


def write_output(data: bytes, output: str | None) -> None:
    """Write binary output to file or stdout."""
    if output:
        Path(output).write_bytes(data)
        return
    sys.stdout.buffer.write(data)


def build_arg_parser() -> argparse.ArgumentParser:
    """Create CLI parser."""
    parser = argparse.ArgumentParser(description="Generate 384-dot raw test print data")
    parser.add_argument(
        "--pattern",
        choices=["blank", "checker", "vertical_lines", "low_density_black"],
        default="checker",
        help="test pattern",
    )
    parser.add_argument("--lines", type=int, default=64, help="number of 48-byte lines")
    parser.add_argument("--output", "-o", help="output .bin file; stdout is used if omitted")
    parser.add_argument("--summary", action="store_true", help="print a short summary to stderr")
    return parser


def main(argv: list[str] | None = None) -> int:
    """Program entry."""
    parser = build_arg_parser()
    args = parser.parse_args(argv)

    data = make_image(args.pattern, args.lines)
    write_output(data, args.output)

    if args.summary:
        print(
            f"pattern={args.pattern} lines={args.lines} bytes={len(data)} "
            f"line_bytes={BYTES_PER_LINE}",
            file=sys.stderr,
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())
