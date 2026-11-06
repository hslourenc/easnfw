import contextlib
import io
import json
import os
import tempfile
import unittest
from unittest.mock import patch

import easncli


class CatalogSelectionTests(unittest.TestCase):
    def test_completed_audio_case_is_in_catalog(self):
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            easncli.print_test_case_catalog()
        cases = json.loads(output.getvalue())["hil_test_cases"]
        audio_case = next(case for case in cases if case["test_case"] == 5)
        self.assertTrue(audio_case["completely_implemented"])
        self.assertFalse(audio_case["requires_human_intervention"])
        atomic_record_case = next(
            case for case in cases if case["test_case"] == 41
        )
        self.assertTrue(atomic_record_case["completely_implemented"])
        nvs_case = next(case for case in cases if case["test_case"] == 22)
        self.assertTrue(nvs_case["completely_implemented"])
        self.assertFalse(next(
            case for case in cases if case["test_case"] == 63
        )["completely_implemented"])

    def test_implemented_selection_includes_audio_and_environment(self):
        result = {
            "passed": True,
            "need_further_verif": False,
            "message": "TCSTATUS_SUCCESS",
        }
        original_dir = os.getcwd()
        with tempfile.TemporaryDirectory() as directory:
            try:
                os.chdir(directory)
                with patch.object(easncli, "run_tc", return_value=result) as run_tc:
                    with contextlib.redirect_stdout(io.StringIO()):
                        self.assertEqual(easncli.run_tcfull(
                            "COM3", implemented_only=True, no_human=True,
                            no_sim=True), 0)
                executed = {call.args[0] for call in run_tc.call_args_list}
                self.assertIn(5, executed)
                self.assertIn(7, executed)
                self.assertIn(22, executed)
                self.assertIn(25, executed)
                self.assertIn(26, executed)
                self.assertIn(32, executed)
                self.assertIn(41, executed)
                self.assertIn(62, executed)
                self.assertNotIn(63, executed)
                self.assertNotIn(11, executed)
                self.assertNotIn(13, executed)
                self.assertNotIn(48, executed)
            finally:
                os.chdir(original_dir)

    def test_no_environment_sensor_excludes_case_seven(self):
        result = {
            "passed": True,
            "need_further_verif": False,
            "message": "TCSTATUS_SUCCESS",
        }
        original_dir = os.getcwd()
        with tempfile.TemporaryDirectory() as directory:
            try:
                os.chdir(directory)
                with patch.object(easncli, "run_tc", return_value=result) as run_tc:
                    with contextlib.redirect_stdout(io.StringIO()):
                        self.assertEqual(easncli.run_tcfull(
                            "COM3", implemented_only=False,
                            no_env_sensor=True), 0)
                executed = {call.args[0] for call in run_tc.call_args_list}
                self.assertIn(5, executed)
                self.assertNotIn(7, executed)
                self.assertNotIn(25, executed)
                self.assertNotIn(26, executed)
                self.assertNotIn(32, executed)
                self.assertNotIn(62, executed)
                self.assertNotIn(63, executed)
            finally:
                os.chdir(original_dir)


if __name__ == "__main__":
    unittest.main()
