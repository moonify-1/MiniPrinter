#!/usr/bin/env python3
"""MiniPrinterRTOS UART protocol sender.

This tool builds the same frame format used by firmware:
magic(2) + proto_ver(1) + header_len(1) + device_id(2) + seq(2) +
cmd(2) + flags(1) + source(1) + payload_len(2) + payload + crc16(2).
All multi-byte fields are little-endian.
"""

from __future__ import annotations

import argparse
import itertools
import struct
import sys
import time
from pathlib import Path


MAGIC = b"\xA5\x5A"
PROTO_VERSION = 1
HEADER_LEN = 14
SOURCE_HOST = 1

CMD = {
    "PING": 0x0001,
    "GET_STATUS": 0x0003,
    "PRINT_START": 0x0200,
    "PRINT_LINE": 0x0201,
    "PRINT_END": 0x0202,
    "SAFE_OFF": 0x0300,
    "SENSOR_TEST": 0x0301,
}


def crc16_ccitt_false(data: bytes) -> int:
    """Calculate CRC-16/CCITT-FALSE, matching src/protocol/proto_crc.cpp."""
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def build_frame(
    *,
    cmd: int,
    payload: bytes = b"",
    seq: int = 1,
    device_id: int = 1,
    flags: int = 0,
) -> bytes:
    """Build one binary protocol frame."""
    if len(payload) > 496:
        raise ValueError("payload too large; max payload is 496 bytes")

    header = bytearray()
    header += MAGIC
    header += struct.pack("<BBHHHBBH", PROTO_VERSION, HEADER_LEN, device_id, seq, cmd, flags, SOURCE_HOST, len(payload))
    body = bytes(header) + payload
    crc = crc16_ccitt_false(body)
    return body + struct.pack("<H", crc)


def make_line(pattern: str, line_no: int) -> bytes:
    """Generate one 384-dot, 48-byte, MSB-left mono test line."""
    data = bytearray(48)
    for x in range(384):
        black = False
        if pattern == "blank":
            black = False
        elif pattern == "checker":
            black = ((x // 8) + (line_no // 8)) % 2 == 0
        elif pattern == "vertical_lines":
            black = (x % 16) < 2
        elif pattern == "low_density_black":
            black = ((x + line_no * 7) % 32) == 0
        else:
            raise ValueError(f"unsupported pattern: {pattern}")

        if black:
            data[x // 8] |= 0x80 >> (x % 8)
    return bytes(data)


def load_print_lines(args: argparse.Namespace) -> list[bytes]:
    """Load or generate PRINT_LINE payloads."""
    if args.file:
        raw = Path(args.file).read_bytes()
        if len(raw) % 48 != 0:
            raise ValueError("print file length must be a multiple of 48 bytes")
        return [raw[i : i + 48] for i in range(0, len(raw), 48)]

    return [make_line(args.pattern, line_no) for line_no in range(args.lines)]


def open_serial(args: argparse.Namespace):
    """Open pyserial lazily so --help and --dry-run work without pyserial."""
    try:
        import serial  # type: ignore
    except ImportError as exc:
        raise SystemExit("pyserial is required for serial I/O: pip install pyserial") from exc

    return serial.Serial(args.port, args.baud, timeout=args.timeout)


def write_and_read(ser, frame: bytes, timeout: float) -> bytes:
    """Send one frame and collect any immediate response bytes."""
    ser.write(frame)
    ser.flush()

    deadline = time.time() + timeout
    chunks: list[bytes] = []
    while time.time() < deadline:
        waiting = getattr(ser, "in_waiting", 0)
        if waiting:
            chunks.append(ser.read(waiting))
        else:
            time.sleep(0.01)
    return b"".join(chunks)


def print_frame(label: str, frame: bytes) -> None:
    """Print frame bytes in a compact debug-friendly format."""
    print(f"{label}: {frame.hex(' ')}")


def add_common_args(parser: argparse.ArgumentParser) -> None:
    """Add serial and frame fields shared by all subcommands."""
    parser.add_argument("--port", help="serial port, for example COM7 or /dev/ttyUSB0")
    parser.add_argument("--baud", type=int, default=115200, help="serial baud rate")
    parser.add_argument("--timeout", type=float, default=0.5, help="response wait time in seconds")
    parser.add_argument("--device-id", type=int, default=1, help="target device id")
    parser.add_argument("--seq", type=int, default=1, help="initial sequence number")
    parser.add_argument("--dry-run", action="store_true", help="print frames but do not open serial")


def send_frames(args: argparse.Namespace, frames: list[tuple[str, bytes]]) -> int:
    """Send prepared frames or print them in dry-run mode."""
    if args.dry_run:
        for label, frame in frames:
            print_frame(label, frame)
        return 0

    if not args.port:
        raise SystemExit("--port is required unless --dry-run is used")

    with open_serial(args) as ser:
        for label, frame in frames:
            print_frame(label, frame)
            response = write_and_read(ser, frame, args.timeout)
            if response:
                print_frame("RX", response)
    return 0


def cmd_ping(args: argparse.Namespace) -> int:
    """Send PING."""
    frame = build_frame(cmd=CMD["PING"], seq=args.seq, device_id=args.device_id)
    return send_frames(args, [("PING", frame)])


def cmd_status(args: argparse.Namespace) -> int:
    """Send GET_STATUS."""
    frame = build_frame(cmd=CMD["GET_STATUS"], seq=args.seq, device_id=args.device_id)
    return send_frames(args, [("GET_STATUS", frame)])


def cmd_safe_off(args: argparse.Namespace) -> int:
    """Send SAFE_OFF."""
    frame = build_frame(cmd=CMD["SAFE_OFF"], seq=args.seq, device_id=args.device_id)
    return send_frames(args, [("SAFE_OFF", frame)])


def cmd_sensor_test(args: argparse.Namespace) -> int:
    """Send SENSOR_TEST."""
    frame = build_frame(cmd=CMD["SENSOR_TEST"], seq=args.seq, device_id=args.device_id)
    return send_frames(args, [("SENSOR_TEST", frame)])


def cmd_print_test(args: argparse.Namespace) -> int:
    """Send PRINT_START, one or more PRINT_LINE frames, then PRINT_END."""
    seq_counter = itertools.count(args.seq)
    frames: list[tuple[str, bytes]] = []
    frames.append(
        (
            "PRINT_START",
            build_frame(cmd=CMD["PRINT_START"], seq=next(seq_counter), device_id=args.device_id),
        )
    )

    for index, line in enumerate(load_print_lines(args)):
        frames.append(
            (
                f"PRINT_LINE[{index}]",
                build_frame(cmd=CMD["PRINT_LINE"], payload=line, seq=next(seq_counter), device_id=args.device_id),
            )
        )

    frames.append(
        (
            "PRINT_END",
            build_frame(cmd=CMD["PRINT_END"], seq=next(seq_counter), device_id=args.device_id),
        )
    )
    return send_frames(args, frames)


def build_arg_parser() -> argparse.ArgumentParser:
    """Create CLI parser."""
    parser = argparse.ArgumentParser(description="Send MiniPrinterRTOS protocol frames")
    sub = parser.add_subparsers(dest="command", required=True)

    for name, func, help_text in [
        ("ping", cmd_ping, "send PING"),
        ("status", cmd_status, "send GET_STATUS"),
        ("safe-off", cmd_safe_off, "send SAFE_OFF"),
        ("sensor-test", cmd_sensor_test, "send SENSOR_TEST"),
    ]:
        p = sub.add_parser(name, help=help_text)
        add_common_args(p)
        p.set_defaults(func=func)

    p_print = sub.add_parser("print-test", help="send PRINT_START/PRINT_LINE/PRINT_END")
    add_common_args(p_print)
    p_print.add_argument("--file", help="raw print data file; length must be N*48 bytes")
    p_print.add_argument("--lines", type=int, default=8, help="generated line count when --file is not used")
    p_print.add_argument(
        "--pattern",
        choices=["blank", "checker", "vertical_lines", "low_density_black"],
        default="checker",
        help="generated pattern when --file is not used",
    )
    p_print.set_defaults(func=cmd_print_test)

    return parser


def main(argv: list[str] | None = None) -> int:
    """Program entry."""
    parser = build_arg_parser()
    args = parser.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
