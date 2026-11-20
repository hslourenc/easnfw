import unittest
import contextlib
import io
import json
import tempfile
from pathlib import Path
from unittest.mock import patch

import easncli


class StartupObservationTests(unittest.TestCase):
    def setUp(self):
        self.lines = [
            "HIL_START case=22",
            "HIL_STARTUP_SELFTEST result=-1004 failure_mask_writes=0",
            "Diagnostic state entered (reason=-1004)",
            "HIL_WATCHDOG case=22",
            "HIL_CLOUD_RESET_COMPLETE",
            "TCSTATUS_RESETDONE - reset complete",
        ]

    def test_complete_ordered_observations_pass(self):
        self.assertTrue(easncli.result_from_startup_observations(22, self.lines)["passed"])

    def test_each_missing_observation_fails(self):
        for index in range(len(self.lines)):
            with self.subTest(index=index):
                lines = self.lines[:index] + self.lines[index + 1:]
                self.assertFalse(easncli.result_from_startup_observations(22, lines)["passed"])

    def test_write_attempt_wrong_error_or_reset_failure_fails(self):
        for old, new in (
            ("failure_mask_writes=0", "failure_mask_writes=1"),
            ("result=-1004", "result=0"),
            ("HIL_CLOUD_RESET_COMPLETE", "HIL_RESET_FAILED error=-1"),
        ):
            with self.subTest(change=old):
                lines = [line.replace(old, new) for line in self.lines]
                self.assertFalse(easncli.result_from_startup_observations(22, lines)["passed"])

    def test_out_of_order_observations_fail(self):
        lines = list(self.lines)
        lines[2], lines[3] = lines[3], lines[2]
        self.assertFalse(easncli.result_from_startup_observations(22, lines)["passed"])

    def test_fatal_error_fails_even_if_markers_follow(self):
        lines = self.lines[:3] + ["ZEPHYR FATAL ERROR"] + self.lines[3:]
        self.assertFalse(easncli.result_from_startup_observations(22, lines)["passed"])

    def test_inter_track_interval_bounds(self):
        prefix = [
            "HIL_START case=25",
            "HIL: ignoring unavailable cloud self-test for pipeline verification",
            "HIL_STARTUP_SELFTEST result=0",
            "HIL: cloud transmission disabled for synthetic records",
            "HIL_TRACK_START uptime_ms=1000",
            "HIL_TRACK_END uptime_ms=2000",
        ]
        suffix = ["HIL_WATCHDOG case=25", "HIL_CLOUD_RESET_COMPLETE",
                  "TCSTATUS_RESETDONE"]
        for interval, expected in ((1999, False), (2000, True), (4000, True),
                                   (4001, False)):
            with self.subTest(interval=interval):
                lines = prefix + [f"HIL_TRACK_START uptime_ms={2000 + interval}"] + suffix
                self.assertEqual(easncli.result_from_startup_observations(25, lines)["passed"],
                                 expected)

    def test_inter_track_missing_or_failed_observations(self):
        lines = [
            "HIL_START case=25",
            "HIL: ignoring unavailable cloud self-test for pipeline verification",
            "HIL_STARTUP_SELFTEST result=0",
            "HIL: cloud transmission disabled for synthetic records",
            "HIL_TRACK_START uptime_ms=1000",
            "HIL_TRACK_END uptime_ms=2000",
            "HIL_TRACK_START uptime_ms=4000",
            "HIL_WATCHDOG case=25",
            "HIL_CLOUD_RESET_COMPLETE",
            "TCSTATUS_RESETDONE",
        ]
        for index in range(len(lines)):
            with self.subTest(missing=index):
                self.assertFalse(easncli.result_from_startup_observations(
                    25, lines[:index] + lines[index + 1:])["passed"])
        for failure in ("Track 1 is incomplete and was not committed",
                        "ZEPHYR FATAL ERROR", "HIL_RESET_FAILED error=-1",
                        "HIL_OBSERVATION_TIMEOUT",
                        "TCSTATUS_FAIL - setup failed"):
            with self.subTest(failure=failure):
                self.assertFalse(easncli.result_from_startup_observations(
                    25, lines[:6] + [failure] + lines[6:])["passed"])
        out_of_order = list(lines)
        out_of_order[4], out_of_order[5] = out_of_order[5], out_of_order[4]
        self.assertFalse(easncli.result_from_startup_observations(25, out_of_order)["passed"])

    def test_environment_sample_and_timestamps(self):
        lines = [
            "HIL_START case=26",
            "HIL: ignoring unavailable cloud self-test for pipeline verification",
            "HIL_STARTUP_SELFTEST result=0",
            "HIL: cloud transmission disabled for synthetic records",
            "HIL_ENV_SAMPLE temp=21000 pressure=100000 humidity=45000 gas=1234",
            "HIL_TRACK_START uptime_ms=1000 env_utc=2000-01-01T00:00:00.900Z "
            "start_utc=2000-01-01T00:00:01.000Z",
            "HIL_TRACK_END uptime_ms=2000 end_utc=2000-01-01T00:00:02.000Z",
            "HIL_WATCHDOG case=26",
            "HIL_CLOUD_RESET_COMPLETE",
            "TCSTATUS_RESETDONE",
        ]
        self.assertTrue(easncli.result_from_startup_observations(26, lines)["passed"])
        variants = [
            lines[:4] + lines[5:],
            lines[:5] + [lines[4]] + lines[5:],
            [line.replace("gas=1234", "gas=bad") for line in lines],
            [line.replace("00:00:02.000Z", "00:00:01.000Z") for line in lines],
            [line.replace("00:00:00.900Z", "00:00:01.100Z") for line in lines],
            lines[:6] + ["HIL_ENV_SAMPLE_ERROR error=-1"] + lines[6:],
            lines[:-1],
        ]
        for variant in variants:
            with self.subTest(variant=variant):
                self.assertFalse(easncli.result_from_startup_observations(26, variant)["passed"])

    def test_persisted_metadata_matches_acquisition(self):
        lines = [
            "HIL_START case=32",
            "HIL: ignoring unavailable cloud self-test for pipeline verification",
            "HIL_STARTUP_SELFTEST result=0",
            "HIL: cloud transmission disabled for synthetic records",
            "HIL_ENV_SAMPLE temp=21000 pressure=100000 humidity=45000 gas=1234",
            "HIL_TRACK_START uptime_ms=1000 env_utc=2000-01-01T00:00:00.900Z "
            "start_utc=2000-01-01T00:00:01.000Z",
            "HIL_TRACK_END uptime_ms=2000 end_utc=2000-01-01T00:00:02.000Z",
            "HIL_PERSISTED_ENV temp=21000 pressure=100000 humidity=45000 gas=1234 "
            "env_utc=2000-01-01T00:00:00.900Z",
            "HIL_PERSISTED_AUDIO start_utc=2000-01-01T00:00:01.000Z "
            "end_utc=2000-01-01T00:00:02.000Z",
            "HIL_WATCHDOG case=32",
            "HIL_CLOUD_RESET_COMPLETE",
            "TCSTATUS_RESETDONE",
        ]
        self.assertTrue(easncli.result_from_startup_observations(32, lines)["passed"])
        for old, new in (("temp=21000", "temp=21001"),
                         ("pressure=100000", "pressure=100001"),
                         ("humidity=45000", "humidity=45001"), ("gas=1234", "gas=1235"),
                         ("00:00:00.900Z", "00:00:00.901Z")):
            altered = list(lines)
            altered[7] = altered[7].replace(old, new)
            with self.subTest(field=old):
                self.assertFalse(easncli.result_from_startup_observations(32, altered)["passed"])
        for index in (7, 8, 10):
            with self.subTest(missing=index):
                self.assertFalse(easncli.result_from_startup_observations(
                    32, lines[:index] + lines[index + 1:])["passed"])
        altered = list(lines)
        altered[8] = altered[8].replace("00:00:02.000Z", "00:00:02.001Z")
        self.assertFalse(easncli.result_from_startup_observations(32, altered)["passed"])
        self.assertFalse(easncli.result_from_startup_observations(
            32, lines[:8] + ["HIL_RECORD_READ_FAILED error=-5"] + lines[8:])["passed"])

    def test_runner_uses_observations_and_saves_result(self):
        class FakeSerial:
            def __init__(self, *args, **kwargs):
                self.commands = []

            def __enter__(self):
                return self

            def __exit__(self, *args):
                pass

            def write(self, data):
                self.commands.append(data)

        calls = []

        def wait(port, statuses, timeout, log_lines):
            calls.append(statuses)
            lines = (["TCSTATUS_RESETDONE - reset complete"]
                     if len(calls) == 1 else self.lines)
            log_lines.extend(lines)
            return lines[-1]

        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / "result"
            with patch.object(easncli.serial, "Serial", FakeSerial), \
                 patch.object(easncli, "wait_for_status", side_effect=wait), \
                 contextlib.redirect_stdout(io.StringIO()):
                result = easncli.run_tc(22, "COM3", target)
            self.assertTrue(result["passed"])
            self.assertEqual(calls, [("RESETDONE",), ("RESETDONE", "FAIL")])
            self.assertIn("TCSTATUS_SUCCESS", (target / "tc-022_cli_c.log").read_text())

    def test_runner_allows_longer_cellular_time_acquisition(self):
        class FakeSerial:
            def __init__(self, *args, **kwargs):
                pass

            def __enter__(self):
                return self

            def __exit__(self, *args):
                pass

            def write(self, data):
                pass

        timeouts = []

        def wait(port, statuses, timeout, log_lines):
            timeouts.append(timeout)
            return ("TCSTATUS_RESETDONE" if len(timeouts) == 1 else
                    "TCSTATUS_INCONCLUSIVE - cellular UTC was unavailable")

        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / "result"
            with patch.object(easncli.serial, "Serial", FakeSerial), \
                 patch.object(easncli, "wait_for_status", side_effect=wait), \
                 contextlib.redirect_stdout(io.StringIO()):
                result = easncli.run_tc(48, "COM3", target)
            self.assertEqual(json.loads((target / "result.json").read_text()), result)
        self.assertEqual(timeouts, [15.0, 960.0])
        self.assertTrue(result["passed"])
        self.assertTrue(result["need_further_verif"])
        self.assertEqual(result["further_verif_type"], "cellular-time")

    def test_backend_inconclusive_and_cellular_success_classification(self):
        backend = easncli.result_from_status(
            "TCSTATUS_INCONCLUSIVE - backend unavailable", 11
        )
        self.assertEqual(backend["further_verif_type"], "cloud-backend")
        success = easncli.result_from_status("TCSTATUS_SUCCESS - UTC restored", 48)
        self.assertFalse(success["need_further_verif"])
        self.assertIsNone(success["further_verif_type"])


if __name__ == "__main__":
    unittest.main()
