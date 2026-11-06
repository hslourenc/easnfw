import unittest

import easncli


class StartupObservationTests(unittest.TestCase):
    def test_tc062_requires_wait_commit_and_preservation_evidence(self):
        lines = [
            "HIL_START case=62",
            "HIL: ignoring unavailable cloud self-test for pipeline verification",
            "HIL_STARTUP_SELFTEST result=0",
            "HIL_STORAGE_SPACE_SHORTAGE injected=2",
            "HIL_STORAGE_WAIT",
            "HIL_STORAGE_WAIT",
            "HIL_STORAGE_SPACE_AVAILABLE",
            "HIL: cloud transmission disabled for synthetic records",
            "HIL_TRACK_START uptime_ms=1000",
            "HIL_TRACK_END uptime_ms=2000",
            "HIL_TRACK_COMMITTED case=62 count=1",
            "HIL_TRACK_COMMITTED case=62 count=2",
            "HIL_RECORD_PRESERVATION_PASSED committed=2 temporary=0 quarantined=0",
            "HIL_WATCHDOG case=62",
            "HIL_CLOUD_RESET_COMPLETE",
            "TCSTATUS_RESETDONE",
        ]

        self.assertTrue(easncli.result_from_startup_observations(62, lines)["passed"])
        lines.remove("HIL_STORAGE_WAIT")
        self.assertFalse(
            easncli.result_from_startup_observations(62, lines)["passed"]
        )

    def test_tc063_requires_record_retention_and_diagnostic_context(self):
        record_id = "0123456789abcdef0123456789abcdef"
        lines = [
            "HIL_START case=63",
            "HIL: ignoring unavailable cloud self-test for pipeline verification",
            "HIL_STARTUP_SELFTEST result=0",
            "HIL_STORAGE_PERMANENT_FAILURE_INJECTED error=-5",
            f"HIL_RUNTIME_RECORD_RETAINED id={record_id}",
            "HIL_SAMPLING_STOP_REQUESTED",
            f"HIL_DIAGNOSTIC_REASON_STORED reason=-5 record_id={record_id}",
            "Diagnostic state entered (reason=-5)",
            "HIL_DIAGNOSTIC_ENTERED case=63",
            "HIL_WATCHDOG case=63",
            "HIL_CLOUD_RESET_COMPLETE",
            "TCSTATUS_RESETDONE",
            "HIL: cloud transmission disabled for synthetic records",
            "HIL_SAMPLING_STOPPED",
        ]

        self.assertTrue(easncli.result_from_startup_observations(63, lines)["passed"])
        lines[5] = (
            "HIL_DIAGNOSTIC_REASON_STORED reason=-5 "
            "record_id=ffffffffffffffffffffffffffffffff"
        )
        self.assertFalse(
            easncli.result_from_startup_observations(63, lines)["passed"]
        )


if __name__ == "__main__":
    unittest.main()
