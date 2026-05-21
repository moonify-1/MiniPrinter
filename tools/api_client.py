#!/usr/bin/env python3
"""MiniPrinterRTOS WiFi API helper.

This tool is intentionally dependency-free so it can run on a normal Python
install. It talks to the firmware HTTP/JSON API under /api/v1.
"""

from __future__ import annotations

import argparse
import binascii
import json
import sys
from pathlib import Path
from urllib import error, parse, request


DEFAULT_BASE_URL = "http://192.168.0.168"
BYTES_PER_LINE = 48
DEFAULT_CHUNK_SIZE = 512


class ApiError(RuntimeError):
    """Raised when the HTTP request itself fails."""


def crc32_ieee(data: bytes) -> int:
    """Return CRC32/IEEE as an unsigned 32-bit integer."""
    return binascii.crc32(data) & 0xFFFFFFFF


def make_low_density_sample(lines: int = 2) -> bytes:
    """Create a tiny 384-dot raw sample for local checks or shift tests."""
    if lines <= 0:
        raise ValueError("lines must be positive")

    out = bytearray()
    for y in range(lines):
        line = bytearray(BYTES_PER_LINE)
        for x in range(384):
            if ((x + y * 7) % 32) == 0:
                line[x // 8] |= 0x80 >> (x % 8)
        out += line
    return bytes(out)


def api_url(base: str, path: str, query: dict[str, object] | None = None) -> str:
    """Build a full URL from base, API path, and optional query args."""
    url = base.rstrip("/") + path
    if query:
        url += "?" + parse.urlencode(query)
    return url


def http_request(
    args: argparse.Namespace,
    method: str,
    path: str,
    *,
    query: dict[str, object] | None = None,
    data: bytes | None = None,
) -> tuple[int, str]:
    """Send one HTTP request and return status plus decoded body."""
    req = request.Request(
        api_url(args.base, path, query),
        data=data,
        method=method,
        headers={
            "Accept": "application/json",
            "Content-Type": "application/octet-stream",
            "User-Agent": "MiniPrinterRTOS-api-client/1",
        },
    )

    try:
        with request.urlopen(req, timeout=args.timeout) as resp:
            return resp.status, resp.read().decode("utf-8", errors="replace")
    except error.HTTPError as exc:
        body = exc.read().decode("utf-8", errors="replace")
        return exc.code, body
    except OSError as exc:
        raise ApiError(str(exc)) from exc


def print_response(status: int, body: str) -> int:
    """Pretty-print a JSON response when possible."""
    print(f"HTTP {status}")
    try:
        parsed = json.loads(body)
        print(json.dumps(parsed, ensure_ascii=False, indent=2))
    except json.JSONDecodeError:
        print(body)
    return 0 if 200 <= status < 300 else 1


def simple_request(method: str, path: str):
    """Create a command handler for an endpoint without extra arguments."""

    def run(args: argparse.Namespace) -> int:
        status, body = http_request(args, method, path)
        return print_response(status, body)

    return run


def cmd_feed(args: argparse.Namespace) -> int:
    """Request safe feed steps."""
    status, body = http_request(
        args, "POST", "/api/v1/feed", query={"steps": args.steps}
    )
    return print_response(status, body)


def cmd_start(args: argparse.Namespace) -> int:
    """Start a print job from an uploaded file_id."""
    status, body = http_request(
        args,
        "POST",
        "/api/v1/print/jobs",
        query={"file_id": args.file_id, "copies": args.copies},
    )
    return print_response(status, body)


def cmd_motor_test(args: argparse.Namespace) -> int:
    """Run guarded low-speed motor factory test."""
    status, body = http_request(
        args, "POST", "/api/v1/factory/motor-test", query={"steps": args.steps}
    )
    return print_response(status, body)


def cmd_head_stb_test(args: argparse.Namespace) -> int:
    """Run guarded STB pulse test while firmware keeps VH off."""
    status, body = http_request(
        args,
        "POST",
        "/api/v1/factory/head-stb-test",
        query={"group": args.group, "pulse_us": args.pulse_us},
    )
    return print_response(status, body)


def cmd_head_shift_test(args: argparse.Namespace) -> int:
    """Send one 48-byte line to the shift/latch factory API."""
    if args.file:
        data = Path(args.file).read_bytes()[:BYTES_PER_LINE]
    else:
        data = make_low_density_sample(1)

    if len(data) != BYTES_PER_LINE:
        raise ApiError("head-shift-test needs at least one 48-byte raw line")

    status, body = http_request(
        args, "POST", "/api/v1/factory/head-shift-test", data=data
    )
    return print_response(status, body)


def create_upload(args: argparse.Namespace, data: bytes) -> int:
    """Create an upload session and return file_id."""
    status, body = http_request(
        args,
        "POST",
        "/api/v1/print/files",
        query={"size": len(data), "crc32": crc32_ieee(data)},
    )
    if status < 200 or status >= 300:
        print_response(status, body)
        raise ApiError("upload create failed")

    parsed = json.loads(body)
    return int(parsed["file"]["file_id"])


def cmd_upload(args: argparse.Namespace) -> int:
    """Upload and complete one raw 48-byte/line print file."""
    data = Path(args.file).read_bytes()
    if len(data) == 0 or len(data) % BYTES_PER_LINE != 0:
        raise ApiError("raw file size must be a non-zero multiple of 48 bytes")

    file_id = create_upload(args, data)
    chunk_size = args.chunk_size
    for index, offset in enumerate(range(0, len(data), chunk_size)):
        chunk = data[offset : offset + chunk_size]
        status, body = http_request(
            args,
            "PUT",
            f"/api/v1/print/files/{file_id}/chunks/{index}",
            data=chunk,
        )
        if status < 200 or status >= 300:
            return print_response(status, body)

    status, body = http_request(
        args, "POST", f"/api/v1/print/files/{file_id}/complete"
    )
    return print_response(status, body)


def cmd_patch_params(args: argparse.Namespace) -> int:
    """Patch whitelisted params using key=value pairs."""
    query: dict[str, str] = {}
    for item in args.values:
        if "=" not in item:
            raise ApiError(f"bad param {item!r}; expected key=value")
        key, value = item.split("=", 1)
        query[key] = value

    status, body = http_request(args, "PATCH", "/api/v1/params", query=query)
    return print_response(status, body)


def cmd_self_test(args: argparse.Namespace) -> int:
    """Run local dry-run checks without contacting a device."""
    del args
    data = make_low_density_sample(2)
    assert len(data) == 96
    assert len(data) % BYTES_PER_LINE == 0
    chunks = [data[i : i + DEFAULT_CHUNK_SIZE] for i in range(0, len(data), DEFAULT_CHUNK_SIZE)]
    assert len(chunks) == 1
    print(
        json.dumps(
            {
                "ok": True,
                "bytes": len(data),
                "lines": len(data) // BYTES_PER_LINE,
                "crc32": crc32_ieee(data),
                "chunks": len(chunks),
            },
            ensure_ascii=False,
            indent=2,
        )
    )
    return 0


def build_parser() -> argparse.ArgumentParser:
    """Create the CLI parser."""
    parser = argparse.ArgumentParser(description="MiniPrinterRTOS WiFi API client")
    parser.add_argument("--base", default=DEFAULT_BASE_URL, help="base URL")
    parser.add_argument("--timeout", type=float, default=5.0, help="HTTP timeout seconds")

    sub = parser.add_subparsers(dest="command", required=True)
    for name, method, path in [
        ("info", "GET", "/api/v1/info"),
        ("status", "GET", "/api/v1/status"),
        ("key", "GET", "/api/v1/key"),
        ("sensors", "GET", "/api/v1/sensors"),
        ("battery", "GET", "/api/v1/battery"),
        ("error", "GET", "/api/v1/error"),
        ("clear-error", "POST", "/api/v1/error/clear"),
        ("params", "GET", "/api/v1/params"),
        ("save-params", "POST", "/api/v1/params/save"),
        ("factory-reset", "POST", "/api/v1/params/factory-reset"),
        ("health", "GET", "/api/v1/health"),
        ("logs", "GET", "/api/v1/logs/recent"),
        ("safe-off", "POST", "/api/v1/safe-off"),
        ("cancel", "POST", "/api/v1/print/jobs/current/cancel"),
        ("device-self-test", "POST", "/api/v1/self-test"),
    ]:
        sub.add_parser(name).set_defaults(func=simple_request(method, path))

    upload = sub.add_parser("upload", help="upload raw print file and complete it")
    upload.add_argument("file")
    upload.add_argument("--chunk-size", type=int, default=DEFAULT_CHUNK_SIZE)
    upload.set_defaults(func=cmd_upload)

    start = sub.add_parser("start", help="start print job from file_id")
    start.add_argument("file_id", type=int)
    start.add_argument("--copies", type=int, default=1)
    start.set_defaults(func=cmd_start)

    feed = sub.add_parser("feed", help="request safe feed steps")
    feed.add_argument("--steps", type=int, default=1)
    feed.set_defaults(func=cmd_feed)

    patch_params = sub.add_parser("patch-params", help="PATCH /api/v1/params")
    patch_params.add_argument("values", nargs="+", help="key=value pairs")
    patch_params.set_defaults(func=cmd_patch_params)

    motor = sub.add_parser("motor-test", help="factory motor test")
    motor.add_argument("--steps", type=int, default=1)
    motor.set_defaults(func=cmd_motor_test)

    shift = sub.add_parser("head-shift-test", help="factory head shift/latch test")
    shift.add_argument("--file", help="raw file; first 48 bytes are used")
    shift.set_defaults(func=cmd_head_shift_test)

    stb = sub.add_parser("head-stb-test", help="factory STB pulse test")
    stb.add_argument("--group", type=int, required=True)
    stb.add_argument("--pulse-us", type=int, default=5)
    stb.set_defaults(func=cmd_head_stb_test)

    sub.add_parser("self-test", help="local dry-run without device").set_defaults(
        func=cmd_self_test
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    """Program entry."""
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        return args.func(args)
    except (ApiError, ValueError, KeyError, json.JSONDecodeError) as exc:
        print(f"api_client: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
