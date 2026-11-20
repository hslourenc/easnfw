#!/usr/bin/env python3
"""Minimal host-side CLI for executing EASNFW HIL test cases."""

from __future__ import annotations

import argparse
import json
import re
import sys
import time
from datetime import datetime
from pathlib import Path

import serial
from serial.tools import list_ports

HIL_TEST_CASES = (1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 20, 22, 25, 26, 32, 41, 42, 43, 46, 48, 62, 63)
# Documented cases without any CLI_H automation.
INCOMPLETE_TEST_CASES = (
    14, 15, 16, 17, 18, 19, 21, 23, 27, 28, 29, 30, 31, 33,
    34, 35, 36, 37, 38, 39, 40, 44, 45, 47, 49, 50, 51, 53, 54, 55,
    56, 57, 58, 59, 60, 61, 64, 65, 66, 67, 68,
)
NOT_FULLY_IMPLEMENTED = "not fully implemented"
HUMAN_INTERVENTION_CASES: set[int] = set()
# Implemented in firmware but depending on optional bench hardware.
ENV_SENSOR_CASES = {7, 25, 26, 32, 62, 63}
SIM_CASES = {48}
# Firmware or test is incomplete; these count as not implemented.
PARTIALLY_IMPLEMENTED_HIL_CASES = {11, 13, 63}
STARTUP_OBSERVATION_CASES = {22, 25, 26, 32, 62, 63}


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


ANSI_ESCAPE_RE = re.compile(r"\x1b\[[0-9;?]*[A-Za-z]")


def status_message(status_line: str) -> str:
    """Return the text from ``TCSTATUS_`` onward without terminal escapes."""
    start = status_line.find("TCSTATUS_")
    message = status_line[start:] if start >= 0 else status_line
    return ANSI_ESCAPE_RE.sub("", message).strip()


def result_from_status(status_line: str, test_case: int) -> dict[str, object]:
    """Convert a host-side status line into the documented result schema."""
    passed = "TCSTATUS_FAIL" not in status_line
    inconclusive = "TCSTATUS_INCONCLUSIVE" in status_line
    return {
        "passed": passed,
        "need_further_verif": inconclusive,
        "further_verif_type": (
            "cellular-time" if test_case == 48 else "cloud-backend"
        ) if inconclusive else None,
        "message": status_message(status_line),
        "data": [],
    }


def result_from_startup_observations(test_case: int,
                                     lines: list[str]) -> dict[str, object]:
    """Evaluate startup observations only after the watchdog reset completes."""
    if test_case not in STARTUP_OBSERVATION_CASES:
        raise ValueError(f"no startup observation evaluator for TC-{test_case:03d}")
    expected_22 = (
        "HIL_START case=22",
        "HIL_STARTUP_SELFTEST result=-1004 failure_mask_writes=0",
        "Diagnostic state entered (reason=-1004)",
        "HIL_WATCHDOG case=22",
        "HIL_CLOUD_RESET_COMPLETE",
        "TCSTATUS_RESETDONE",
    )
    expected_25 = (
        "HIL_START case=25",
        "HIL: ignoring unavailable cloud self-test for pipeline verification",
        "HIL_STARTUP_SELFTEST result=0",
        "HIL_TRACK_START uptime_ms=",
        "HIL_TRACK_END uptime_ms=",
        "HIL_TRACK_START uptime_ms=",
        "HIL_WATCHDOG case=25",
        "HIL_CLOUD_RESET_COMPLETE",
        "TCSTATUS_RESETDONE",
    )
    expected_26 = (
        "HIL_START case=26",
        "HIL: ignoring unavailable cloud self-test for pipeline verification",
        "HIL_STARTUP_SELFTEST result=0",
        "HIL_ENV_SAMPLE temp=",
        "HIL_TRACK_START uptime_ms=",
        "HIL_TRACK_END uptime_ms=",
        "HIL_WATCHDOG case=26",
        "HIL_CLOUD_RESET_COMPLETE",
        "TCSTATUS_RESETDONE",
    )
    expected_32 = (
        "HIL_START case=32",
        *expected_26[1:6],
        "HIL_PERSISTED_ENV ",
        "HIL_PERSISTED_AUDIO ",
        "HIL_WATCHDOG case=32",
        "HIL_CLOUD_RESET_COMPLETE",
        "TCSTATUS_RESETDONE",
    )
    expected_62 = (
        "HIL_START case=62",
        "HIL: ignoring unavailable cloud self-test for pipeline verification",
        "HIL_STARTUP_SELFTEST result=0",
        "HIL_STORAGE_SPACE_SHORTAGE injected=2",
        "HIL_STORAGE_WAIT",
        "HIL_STORAGE_WAIT",
        "HIL_STORAGE_SPACE_AVAILABLE",
        "HIL_TRACK_START uptime_ms=",
        "HIL_TRACK_END uptime_ms=",
        "HIL_TRACK_COMMITTED case=62 count=1",
        "HIL_TRACK_COMMITTED case=62 count=2",
        "HIL_RECORD_PRESERVATION_PASSED ",
        "HIL_WATCHDOG case=62",
        "HIL_CLOUD_RESET_COMPLETE",
        "TCSTATUS_RESETDONE",
    )
    expected_63 = (
        "HIL_START case=63",
        "HIL: ignoring unavailable cloud self-test for pipeline verification",
        "HIL_STARTUP_SELFTEST result=0",
        "HIL_STORAGE_PERMANENT_FAILURE_INJECTED error=-5",
        "HIL_RUNTIME_RECORD_RETAINED id=",
        "HIL_SAMPLING_STOP_REQUESTED",
        "HIL_DIAGNOSTIC_REASON_STORED reason=-5 record_id=",
        "Diagnostic state entered (reason=-5)",
        "HIL_DIAGNOSTIC_ENTERED case=63",
        "HIL_WATCHDOG case=63",
        "HIL_CLOUD_RESET_COMPLETE",
        "TCSTATUS_RESETDONE",
    )
    expected = {22: expected_22, 25: expected_25, 26: expected_26,
                32: expected_32, 62: expected_62,
                63: expected_63}[test_case]
    index = 0
    failure_markers = ("HIL_RESET_FAILED", "HIL_OBSERVATION_TIMEOUT",
                       "ZEPHYR FATAL ERROR", "TCSTATUS_FAIL",
                       "HIL_ENV_SAMPLE_ERROR", "HIL_RECORD_READ_FAILED",
                       "HIL_RECORD_PRESERVATION_FAILED",
                       "HIL_RUNTIME_RECORD_NOT_RETAINED")
    track_times: list[tuple[str, int]] = []
    for line in lines:
        if any(marker in line for marker in failure_markers):
            break
        match = re.search(r"HIL_TRACK_(START|END) uptime_ms=(\d+)", line)
        if match:
            track_times.append((match.group(1), int(match.group(2))))
        if index < len(expected) and expected[index] in line:
            index += 1
    passed = index == len(expected) and not any(
        marker in line for line in lines for marker in failure_markers
    )
    if test_case in (25, 26, 32, 62, 63):
        passed = passed and any(
            "HIL: cloud transmission disabled for synthetic records" in line
            for line in lines
        )
    if test_case == 25:
        passed = passed and len(track_times) >= 3 and [
            item[0] for item in track_times[:3]
        ] == ["START", "END", "START"]
        if passed:
            start, end, next_start = (item[1] for item in track_times[:3])
            passed = start < end and 2000 <= next_start - end <= 4000
        passed = passed and not any("incomplete and was not committed" in line
                                    for line in lines)
    if test_case == 62:
        passed = passed and not any("incomplete and was not committed" in line
                                    for line in lines)
    if test_case == 63 and passed:
        retained = next(
            (line for line in lines if "HIL_RUNTIME_RECORD_RETAINED id=" in line),
            "",
        )
        stored = next(
            (line for line in lines if "HIL_DIAGNOSTIC_REASON_STORED " in line),
            "",
        )
        retained_id = re.search(r"id=([0-9a-f]{32})", retained)
        stored_id = re.search(r"record_id=([0-9a-f]{32})", stored)
        passed = (retained_id is not None and stored_id is not None
                  and retained_id.group(1) == stored_id.group(1)
                  and any("HIL_SAMPLING_STOPPED" in line for line in lines))
    if test_case in (26, 32) and passed:
        startup = next(i for i, line in enumerate(lines) if expected_26[2] in line)
        first_end = next(i for i in range(startup + 1, len(lines))
                         if expected_26[5] in lines[i])
        first_track = lines[startup + 1:first_end + 1]
        samples = [line for line in first_track if "HIL_ENV_SAMPLE " in line]
        starts = [line for line in first_track if "HIL_TRACK_START " in line]
        ends = [line for line in first_track if "HIL_TRACK_END " in line]
        sample_pattern = r"HIL_ENV_SAMPLE temp=(-?\d+) pressure=(\d+) humidity=(\d+) gas=(\d+)"
        timestamp = r"(\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d\.\d{3}Z)"
        start_match = (re.search(r"env_utc=" + timestamp + r" start_utc=" + timestamp,
                                 starts[0]) if len(starts) == 1 else None)
        end_match = (re.search(r"end_utc=" + timestamp, ends[0])
                     if len(ends) == 1 else None)
        passed = (len(samples) == 1 and re.search(sample_pattern, samples[0]) is not None
                  and start_match is not None and end_match is not None)
        if passed:
            try:
                env_time, start_time, end_time = (
                    datetime.strptime(value, "%Y-%m-%dT%H:%M:%S.%fZ")
                    for value in (*start_match.groups(), end_match.group(1))
                )
                passed = env_time <= start_time < end_time
            except ValueError:
                passed = False
        if test_case == 32 and passed:
            captured = re.search(sample_pattern, samples[0])
            persisted_env = next(line for line in lines if "HIL_PERSISTED_ENV " in line)
            persisted_audio = next(line for line in lines if "HIL_PERSISTED_AUDIO " in line)
            env_match = re.search(
                r"HIL_PERSISTED_ENV temp=(-?\d+) pressure=(\d+) humidity=(\d+) gas=(\d+) env_utc="
                + timestamp, persisted_env)
            audio_match = re.search(
                r"HIL_PERSISTED_AUDIO start_utc=" + timestamp + r" end_utc=" + timestamp,
                persisted_audio)
            passed = (env_match is not None and audio_match is not None
                      and captured.groups() == env_match.groups()[:4]
                      and start_match.group(1) == env_match.group(5)
                      and (start_match.group(2), end_match.group(1)) == audio_match.groups())
        passed = passed and not any("incomplete and was not committed" in line
                                    for line in lines)
    detail = ("NVS failure prevented mask write and entered diagnostic state"
              if test_case == 22 else
              "inter-track interval within 2000-4000 ms" if test_case == 25 else
              "temperature, pressure, humidity and gas resistance observed" if test_case == 26 else
              "stored environmental values and timestamps match acquisition" if test_case == 32 else
              "space shortage deferred acquisition and preserved record states" if test_case == 62 else
              "permanent storage failure retained the record and entered diagnostic state")
    message = (f"TCSTATUS_SUCCESS - {detail}" if passed else
               "TCSTATUS_FAIL - startup observation or coordinated reset was missing or invalid")
    return {
        "passed": passed,
        "need_further_verif": False,
        "further_verif_type": None,
        "message": message,
        "data": [],
    }


def run_tc(test_case: int, requested_port: str | None,
           result_dir: Path | None = None) -> dict[str, object]:
    """Execute one HIL test case and write the documented result directory."""
    if test_case not in HIL_TEST_CASES:
        raise ValueError(
            "supported HIL test cases are " +
            ", ".join(str(case) for case in HIL_TEST_CASES)
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
            if test_case in STARTUP_OBSERVATION_CASES:
                observation_start = len(log_lines)
                result_line = wait_for_status(port, ("RESETDONE", "FAIL"), 90.0, log_lines)
                result = result_from_startup_observations(
                    test_case, log_lines[observation_start:]
                )
            else:
                timeout = 960.0 if test_case == 48 else 120.0
                result_line = wait_for_status(
                    port, ("SUCCESS", "FAIL", "INCONCLUSIVE"), timeout, log_lines
                )
                result = result_from_status(result_line, test_case)
    except (TimeoutError, serial.SerialException) as exc:
        device_log.write_text(
            "\n".join(log_lines + [f"error={exc}"]) + "\n",
            encoding="utf-8",
        )
        cli_log.write_text(
            f"port={port_name}\nerror={exc}\n", encoding="utf-8"
        )
        raise

    (result_dir / "result.json").write_text(
        json.dumps(result, indent=2) + "\n", encoding="utf-8"
    )
    cli_log.write_text(
        "\n".join(log_lines[:2] + [
            f"reset={reset_line}",
            f"result={result['message']}",
        ]) + "\n",
        encoding="utf-8",
    )
    device_log.write_text("\n".join(log_lines) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2))
    return result


def print_banner(title: str) -> None:
    """Print a prominent header that separates sections of the output."""
    bar = "=" * 72
    print(f"\n{bar}\n{title.center(72)}\n{bar}\n")


def format_cases(cases: list[int]) -> str:
    """Format a list of test case numbers for the summary."""
    return ", ".join(str(case) for case in cases) if cases else "none"


def run_tcfull(requested_port: str | None, implemented_only: bool = False,
               no_human: bool = False, no_env_sensor: bool = False,
               no_sim: bool = False) -> int:
    """Run selected HIL cases and aggregate only cases that were executed."""
    timestamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    result_dir = Path(f"tc-full-{timestamp}")
    result_dir.mkdir()
    # Documented cases without CLI_H automation cannot run in any selection.
    skipped = [
        {"test_case": case, "reason": NOT_FULLY_IMPLEMENTED}
        for case in INCOMPLETE_TEST_CASES
        if case not in HIL_TEST_CASES
    ]
    test_cases = []
    for case in HIL_TEST_CASES:
        reasons = []
        if implemented_only and case in PARTIALLY_IMPLEMENTED_HIL_CASES:
            reasons.append(NOT_FULLY_IMPLEMENTED)
        if no_human and case in HUMAN_INTERVENTION_CASES:
            reasons.append("requires human intervention")
        if no_env_sensor and case in ENV_SENSOR_CASES:
            reasons.append("requires environmental sensor")
        if no_sim and case in SIM_CASES:
            reasons.append("requires SIM")
        if reasons:
            skipped.append({"test_case": case, "reason": "; ".join(reasons)})
        else:
            test_cases.append(case)

    individual_results: list[dict[str, object]] = []

    print_banner("TEST CASE LOGS")
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

    skipped.sort(key=lambda item: item["test_case"])
    executed = [item["test_case"] for item in individual_results]
    passed = [item["test_case"] for item in individual_results
              if item["passed"]]
    failed = [item["test_case"] for item in individual_results
              if not item["passed"]]
    skipped_cases = [item["test_case"] for item in skipped]
    aggregate = {
        "success": bool(individual_results) and not failed,
        "executed_count": len(executed),
        "executed": executed,
        "skipped_count": len(skipped_cases),
        "skipped_cases": skipped_cases,
        "passed_count": len(passed),
        "passed": passed,
        "failed_count": len(failed),
        "failed": failed,
        "individual_results": individual_results,
        "skipped": skipped,
    }
    (result_dir / "result.json").write_text(
        json.dumps(aggregate, indent=2) + "\n", encoding="utf-8"
    )
    print_banner("FINAL RESULTS (result.json)")
    print(json.dumps(aggregate, indent=2))
    print_banner("RESULTS SUMMARY")
    for label, cases in (("executed", executed), ("skipped", skipped_cases),
                         ("passed", passed), ("failed", failed)):
        print(f"Number of {label} tests: {len(cases)}")
        print(f"{label.capitalize()} tests: {format_cases(cases)}")
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
        "--no-env-sensor", action="store_true",
        help="exclude HIL cases that require the environmental sensor",
    )
    full_parser.add_argument(
        "--no-sim", action="store_true",
        help="exclude HIL cases that require a SIM and cellular service",
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
            no_env_sensor=args.no_env_sensor,
            no_sim=args.no_sim,
        )
    except (RuntimeError, TimeoutError, ValueError, serial.SerialException) as exc:
        print(f"easncli: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
