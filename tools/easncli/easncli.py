#!/usr/bin/env python3
"""Minimal host-side CLI for executing EASNFW HIL test cases."""

from __future__ import annotations

import argparse
import json
import sys
import time
from datetime import datetime
from pathlib import Path

import serial
from serial.tools import list_ports

HIL_TEST_CASES = (1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 20, 42, 46, 48)
INCOMPLETE_TEST_CASES = (
    "005", "007", "011", "013", "014", "015", "016", "016a", "017",
    "018", "019", "021", "022", "023", "023a", "023b", "023c", "025",
    "025a", "026", "027", "028", "029", "030", "031", "032", "033",
    "034", "035", "035a", "035b", "036", "037", "038", "039", "039a",
    "039b", "040", "040a", "041", "043", "044", "044a", "045",
    "047", "048", "049", "049a", "050", "051", "053", "054",
    "055", "056",
)
HUMAN_INTERVENTION_CASES = {5, 7}
PARTIALLY_IMPLEMENTED_HIL_CASES = {5, 7, 11, 13, 48}


def select_port(requested: str | None) -> str:
    """Select the requested port or the only port visible to the host."""
    if requested:
        return requested
    ports = [port.device for port in list_ports.comports()]
    if len(ports) != 1:
        raise RuntimeError("specify --port when zero or multiple serial ports exist")
    return ports[0]


def wait_for_status(port: serial.Serial, statuses: tuple[str, ...],
                    timeout: float, log_lines: list[str]) -> str:
    """Read shell output until the requested TCSTATUS line is observed."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        raw = port.readline()
        if not raw:
            continue
        line = raw.decode(errors="replace").strip()
        log_lines.append(line)
        print(line)
        if any(f"TCSTATUS_{status}" in line for status in statuses):
            return line
    raise TimeoutError(
        f"timed out waiting for TCSTATUS_{'/'.join(statuses)}"
    )


def result_from_status(status_line: str) -> dict[str, object]:
    """Convert a host-side status line into the documented result schema."""
    passed = "TCSTATUS_FAIL" not in status_line
    inconclusive = "TCSTATUS_INCONCLUSIVE" in status_line
    return {
        "passed": passed,
        "need_further_verif": inconclusive,
        "further_verif_type": "cloud-backend" if inconclusive else None,
        "message": status_line,
        "data": [],
    }


def run_tc(test_case: int, requested_port: str | None,
           result_dir: Path | None = None) -> dict[str, object]:
    """Execute one HIL test case and write the documented result directory."""
    if test_case not in HIL_TEST_CASES:
        raise ValueError(
            "supported HIL test cases are 1 through 13, 20, 42, 46, and 48"
        )

    if result_dir is None:
        timestamp = datetime.now().strftime("%Y%m%d-%H%M%S")
        result_dir = Path(f"tc-{test_case:03d}-{timestamp}")
    result_dir.mkdir(parents=True, exist_ok=False)
    device_log = result_dir / f"tc-{test_case:03d}.log"
    cli_log = result_dir / f"tc-{test_case:03d}_cli_c.log"
    port_name = select_port(requested_port)
    log_lines: list[str] = []

    try:
        with serial.Serial(port_name, 115200, timeout=0.2) as port:
            log_lines.append(f"port={port_name}")
            log_lines.append("command=easnfwcli2_reset")
            port.write(b"easnfwcli2_reset\r\n")
            reset_line = wait_for_status(port, ("RESETDONE",), 15.0, log_lines)
            command = f"easnfwcli2_tc {test_case}"
            log_lines.append(f"command={command}")
            port.write(f"{command}\r\n".encode())
            result_line = wait_for_status(
                port, ("SUCCESS", "FAIL", "INCONCLUSIVE"), 120.0, log_lines
            )
    except (TimeoutError, serial.SerialException) as exc:
        device_log.write_text(
            "\n".join(log_lines + [f"error={exc}"]) + "\n",
            encoding="utf-8",
        )
        cli_log.write_text(
            f"port={port_name}\nerror={exc}\n", encoding="utf-8"
        )
        raise

    result = result_from_status(result_line)
    (result_dir / "result.json").write_text(
        json.dumps(result, indent=2) + "\n", encoding="utf-8"
    )
    cli_log.write_text(
        "\n".join(log_lines[:2] + [
            f"reset={reset_line}",
            f"result={result_line}",
        ]) + "\n",
        encoding="utf-8",
    )
    device_log.write_text("\n".join(log_lines) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2))
    return result


def run_tcfull(requested_port: str | None, implemented_only: bool = False,
               no_human: bool = False) -> int:
    """Run selected HIL cases and aggregate only cases that were executed."""
    timestamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    result_dir = Path(f"tc-full-{timestamp}")
    result_dir.mkdir()
    test_cases = [
        case for case in HIL_TEST_CASES
        if not implemented_only or case not in PARTIALLY_IMPLEMENTED_HIL_CASES
    ]
    if no_human:
        test_cases = [
            case for case in test_cases
            if case not in HUMAN_INTERVENTION_CASES
        ]

    individual_results: list[dict[str, object]] = []
    skipped = []
    for case in HIL_TEST_CASES:
        reasons = []
        if implemented_only and case in PARTIALLY_IMPLEMENTED_HIL_CASES:
            reasons.append("not completely implemented")
        if no_human and case in HUMAN_INTERVENTION_CASES:
            reasons.append("requires human intervention")
        if reasons:
            skipped.append({
                "test_case": case,
                "reason": "; ".join(reasons),
            })

    for test_case in test_cases:
        case_dir = result_dir / f"tc-{test_case:03d}"
        try:
            result = run_tc(test_case, requested_port, case_dir)
        except (RuntimeError, TimeoutError, ValueError,
                serial.SerialException) as exc:
            result = {
                "passed": False,
                "need_further_verif": False,
                "further_verif_type": None,
                "message": str(exc),
                "data": [],
            }
        individual_results.append({
            "test_case": test_case,
            "passed": result["passed"],
            "need_further_verif": result["need_further_verif"],
            "message": result["message"],
        })

    aggregate = {
        "success": bool(individual_results) and
        all(item["passed"] for item in individual_results),
        "individual_results": individual_results,
        "skipped": skipped,
    }
    (result_dir / "result.json").write_text(
        json.dumps(aggregate, indent=2) + "\n", encoding="utf-8"
    )
    print(json.dumps(aggregate, indent=2))
    return 0 if aggregate["success"] else 1


def print_test_case_catalog() -> None:
    """Print HIL coverage, intervention requirements, and incomplete cases."""
    catalog = {
        "hil_test_cases": [
            {
                "test_case": case,
                "completely_implemented":
                    case not in PARTIALLY_IMPLEMENTED_HIL_CASES,
                "requires_human_intervention":
                    case in HUMAN_INTERVENTION_CASES,
            }
            for case in HIL_TEST_CASES
        ],
        "not_completely_implemented": list(INCOMPLETE_TEST_CASES),
    }
    print(json.dumps(catalog, indent=2))


def main() -> int:
    """Parse the primary command and execute it."""
    parser = argparse.ArgumentParser(prog="easncli")
    subparsers = parser.add_subparsers(dest="command", required=True)
    tc_parser = subparsers.add_parser("tc", help="execute a HIL test case")
    tc_parser.add_argument("number", type=int)
    tc_parser.add_argument("--port")
    full_parser = subparsers.add_parser(
        "tcfull", help="execute all HIL test cases"
    )
    full_parser.add_argument("--port")
    full_parser.add_argument(
        "--implemented-only", action="store_true",
        help="exclude HIL cases that are not completely implemented",
    )
    full_parser.add_argument(
        "--no-human", action="store_true",
        help="exclude HIL cases that require human intervention",
    )
    subparsers.add_parser(
        "cases", help="list HIL cases and cases not completely implemented"
    )
    args = parser.parse_args()
    try:
        if args.command == "tc":
            result = run_tc(args.number, args.port)
            return 0 if result["passed"] else 1
        if args.command == "cases":
            print_test_case_catalog()
            return 0
        return run_tcfull(
            args.port,
            implemented_only=args.implemented_only,
            no_human=args.no_human,
        )
    except (RuntimeError, TimeoutError, ValueError, serial.SerialException) as exc:
        print(f"easncli: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
