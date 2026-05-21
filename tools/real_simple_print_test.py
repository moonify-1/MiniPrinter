#!/usr/bin/env python3
"""MiniPrinterRTOS real simple print test.

This script follows the short print-test path, but it refuses to run as an API
only test. It requires both real stepper and real thermal-head firmware flags,
then runs a guarded motor test before starting a small visible print job.
"""

from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path
from types import SimpleNamespace

import api_client


REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_BASE_URL = "http://192.168.0.168"
DEFAULT_PAYLOAD = REPO_ROOT / "docs" / "apifox" / "payloads" / "print_simple_1line.bin"
DEFAULT_REPEAT_LINES = 16


class TestAbort(RuntimeError):
    """Raised when the test must stop to avoid a fake or unsafe print result."""


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    """Build command-line options for the one-click real print test."""
    parser = argparse.ArgumentParser(
        description="Run a real MiniPrinterRTOS simple print test over WiFi."
    )
    parser.add_argument("--base", default=DEFAULT_BASE_URL, help="device base URL")
    parser.add_argument("--timeout", type=float, default=8.0, help="HTTP timeout seconds")
    parser.add_argument(
        "--payload",
        default=str(DEFAULT_PAYLOAD),
        help="48-byte simple raw line used as the visible-print source",
    )
    parser.add_argument(
        "--repeat-lines",
        type=int,
        default=DEFAULT_REPEAT_LINES,
        help="repeat the 1-line sample to make the physical mark easier to see",
    )
    parser.add_argument("--motor-steps", type=int, default=4, help="real motor test steps")
    parser.add_argument("--density", type=int, default=25, help="print density 0..100")
    parser.add_argument("--heat", type=int, default=25, help="print heat 0..100")
    parser.add_argument("--poll-count", type=int, default=12, help="job status polls")
    parser.add_argument("--poll-delay", type=float, default=0.5, help="seconds between polls")
    parser.add_argument(
        "--keep-file",
        action="store_true",
        help="do not delete the uploaded print file after the test",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="only check local payload generation; do not contact the device",
    )
    return parser.parse_args(argv)


def resolve_payload(path_text: str) -> Path:
    """Resolve a payload path from either the repo root or current directory."""
    path = Path(path_text)
    if path.is_absolute():
        return path

    repo_path = REPO_ROOT / path
    if repo_path.exists():
        return repo_path
    return path.resolve()


def make_visible_payload(path: Path, repeat_lines: int) -> bytes:
    """Load the simple 48-byte line and repeat it into a visible real print."""
    if repeat_lines <= 0:
        raise TestAbort("--repeat-lines must be positive")

    data = path.read_bytes()
    if len(data) != api_client.BYTES_PER_LINE:
        raise TestAbort(
            f"payload must be exactly {api_client.BYTES_PER_LINE} bytes; got {len(data)}"
        )
    return data * repeat_lines


def pretty_json(value: object) -> str:
    """Format response JSON so failures can be copied into notes or Apifox."""
    return json.dumps(value, ensure_ascii=False, indent=2)


def request_json(
    client: SimpleNamespace,
    label: str,
    method: str,
    path: str,
    *,
    query: dict[str, object] | None = None,
    data: bytes | None = None,
    required: bool = True,
) -> dict[str, object]:
    """Send one firmware API request and validate the HTTP/JSON success fields."""
    url = api_client.api_url(client.base, path, query)
    print(f"\n[{label}] {method} {url}")
    status, body = api_client.http_request(
        client, method, path, query=query, data=data
    )

    try:
        parsed = json.loads(body)
    except json.JSONDecodeError as exc:
        print(f"HTTP {status}\n{body}")
        if required:
            raise TestAbort(f"{label} did not return JSON") from exc
        return {}

    print(f"HTTP {status}")
    print(pretty_json(parsed))

    ok_field = parsed.get("ok", True)
    if required and (status < 200 or status >= 300 or ok_field is False):
        code = parsed.get("code", "UNKNOWN")
        raise TestAbort(f"{label} failed with code={code}")
    return parsed


def require_real_firmware(info: dict[str, object]) -> None:
    """Stop when the firmware would only prove API behavior instead of hardware."""
    features = info.get("features")
    if not isinstance(features, dict):
        raise TestAbort("GET /info response has no features object")

    hw_stepper = features.get("hw_stepper") is True
    hw_head = features.get("hw_thermal_head") is True
    if not (hw_stepper and hw_head):
        raise TestAbort(
            "this is not a real-print firmware: "
            f"hw_stepper={hw_stepper}, hw_thermal_head={hw_head}"
        )


def require_safe_system_state(status: dict[str, object]) -> None:
    """Refuse to start a real print while the device is in SAFE_MODE."""
    system_state = status.get("system_state")
    if system_state == "SAFE_MODE":
        raise TestAbort("device is in SAFE_MODE; clear the hardware fault first")


def create_file(client: SimpleNamespace, data: bytes) -> int:
    """Create a firmware upload slot and return the assigned file_id."""
    crc = api_client.crc32_ieee(data)
    response = request_json(
        client,
        "04 create print file",
        "POST",
        "/api/v1/print/files",
        query={"size": len(data), "crc32": crc},
    )

    file_info = response.get("file")
    if not isinstance(file_info, dict):
        raise TestAbort("create file response has no file object")
    return int(file_info["file_id"])


def upload_chunks(client: SimpleNamespace, file_id: int, data: bytes) -> None:
    """Upload the raw print bytes using the firmware 512-byte chunk rule."""
    for index, offset in enumerate(range(0, len(data), api_client.DEFAULT_CHUNK_SIZE)):
        chunk = data[offset : offset + api_client.DEFAULT_CHUNK_SIZE]
        request_json(
            client,
            f"05 upload chunk {index}",
            "PUT",
            f"/api/v1/print/files/{file_id}/chunks/{index}",
            data=chunk,
        )


def complete_file(client: SimpleNamespace, file_id: int) -> None:
    """Ask firmware to verify total bytes and CRC32 before printing."""
    response = request_json(
        client,
        "06 complete print file",
        "POST",
        f"/api/v1/print/files/{file_id}/complete",
    )
    file_info = response.get("file")
    if not isinstance(file_info, dict) or file_info.get("state") != "COMPLETE":
        raise TestAbort("print file did not reach COMPLETE state")


def start_print(client: SimpleNamespace, file_id: int, args: argparse.Namespace) -> int:
    """Start the real low-energy print job from the completed upload file."""
    response = request_json(
        client,
        "07 start real print",
        "POST",
        "/api/v1/print/jobs",
        query={
            "file_id": file_id,
            "copies": 1,
            "density": args.density,
            "heat": args.heat,
        },
    )
    return int(response["job_id"])


def wait_for_done(client: SimpleNamespace, args: argparse.Namespace, lines: int) -> None:
    """Poll the print job until it finishes or reports a firmware error."""
    for attempt in range(1, args.poll_count + 1):
        response = request_json(
            client,
            f"08 poll print job {attempt}",
            "GET",
            "/api/v1/print/jobs/current",
        )
        state = response.get("state")
        line_done = int(response.get("line_done", 0))
        error = int(response.get("error", 0))

        if state == "DONE" and line_done >= lines and error == 0:
            return
        if state in {"FAILED", "CANCELLED"} or error != 0:
            raise TestAbort(
                f"print job ended with state={state}, line_done={line_done}, error={error}"
            )
        time.sleep(args.poll_delay)

    raise TestAbort("print job did not finish before poll timeout")


def best_effort_safe_off(client: SimpleNamespace) -> None:
    """Always try to shut down VH, STB, and motor outputs after the test."""
    try:
        request_json(
            client, "cleanup safe-off", "POST", "/api/v1/safe-off", required=False
        )
    except Exception as exc:  # noqa: BLE001 - cleanup must not hide the root error.
        print(f"\n[cleanup warning] safe-off failed: {exc}", file=sys.stderr)


def best_effort_delete_file(client: SimpleNamespace, file_id: int | None) -> None:
    """Free the firmware upload slot after the print job has been started."""
    if file_id is None:
        return
    try:
        request_json(
            client,
            "cleanup delete print file",
            "DELETE",
            f"/api/v1/print/files/{file_id}",
            required=False,
        )
    except Exception as exc:  # noqa: BLE001 - cleanup must not hide the root error.
        print(f"\n[cleanup warning] delete file failed: {exc}", file=sys.stderr)


def run(args: argparse.Namespace) -> int:
    """Run the full real-print path from safety checks to physical output."""
    payload_path = resolve_payload(args.payload)
    data = make_visible_payload(payload_path, args.repeat_lines)
    lines = len(data) // api_client.BYTES_PER_LINE
    crc = api_client.crc32_ieee(data)

    print("MiniPrinterRTOS real simple print test")
    print(f"base        : {args.base}")
    print(f"payload     : {payload_path}")
    print(f"lines/bytes : {lines}/{len(data)}")
    print(f"crc32       : 0x{crc:08X} ({crc})")
    print(f"density/heat: {args.density}/{args.heat}")

    if args.dry_run:
        return 0

    client = SimpleNamespace(base=args.base, timeout=args.timeout)
    file_id: int | None = None
    try:
        info = request_json(client, "00 require real firmware", "GET", "/api/v1/info")
        require_real_firmware(info)
        request_json(client, "01 pre-test safe-off", "POST", "/api/v1/safe-off")
        status = request_json(client, "02 status check", "GET", "/api/v1/status")
        require_safe_system_state(status)
        request_json(
            client,
            "03 real motor test",
            "POST",
            "/api/v1/factory/motor-test",
            query={"steps": args.motor_steps},
        )
        file_id = create_file(client, data)
        upload_chunks(client, file_id, data)
        complete_file(client, file_id)
        job_id = start_print(client, file_id, args)
        wait_for_done(client, args, lines)
        print(f"\nOK: real print job {job_id} finished. Check the paper for visible dots.")
        return 0
    finally:
        best_effort_safe_off(client)
        if not args.keep_file:
            best_effort_delete_file(client, file_id)


def main(argv: list[str] | None = None) -> int:
    """Program entry point used by both Python and PowerShell launchers."""
    try:
        return run(parse_args(argv))
    except (api_client.ApiError, OSError, KeyError, ValueError, TestAbort) as exc:
        print(f"\nABORT: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
