"""
test_api.py

API tests for the Flask backend, written with Python's built-in
`unittest` module (no extra test framework required to run these - though
they also work fine if you have pytest installed, since pytest can run
unittest.TestCase classes directly).

Run from the backend/ folder with either:
    python -m unittest tests.test_api -v
or, if you have pytest installed:
    pytest -v

These tests exercise the HTTP layer only (via Flask's built-in test
client, so no real server needs to be running) and check the CURRENT
mock adapter's behaviour. Once CEngineAdapter is implemented, the
"valid expression" tests below should still pass unchanged, since they
only assert on the response SHAPE and the example values documented in
the project spec - not on any internal mock detail.
"""

import os
import sys
import unittest

# Make sure "backend/" (the parent of this tests/ folder) is on sys.path,
# so `import app` works no matter which directory this file is run from.
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from app import create_app  # noqa: E402  (import after sys.path fix, on purpose)


class ExpressionApiTestCase(unittest.TestCase):
    def setUp(self):
        # A fresh Flask test client for every test - cheap, and keeps
        # tests independent of each other.
        app = create_app()
        app.testing = True
        self.client = app.test_client()

    # ---- GET /api/health ---------------------------------------------

    def test_health(self):
        resp = self.client.get("/api/health")
        self.assertEqual(resp.status_code, 200)
        data = resp.get_json()
        self.assertEqual(data["status"], "ok")
        self.assertEqual(data["service"], "Expression Validator & Evaluator API")

    # ---- GET /api/examples ---------------------------------------------

    def test_examples(self):
        resp = self.client.get("/api/examples")
        self.assertEqual(resp.status_code, 200)
        data = resp.get_json()
        self.assertIn("examples", data)
        self.assertIsInstance(data["examples"], list)
        self.assertIn("(25 + 15) * 2", data["examples"])
        self.assertGreaterEqual(len(data["examples"]), 5)

    # ---- GET /api/notation-info ---------------------------------------------

    def test_notation_info(self):
        resp = self.client.get("/api/notation-info")
        self.assertEqual(resp.status_code, 200)
        data = resp.get_json()
        for key in ("infix", "prefix", "postfix"):
            self.assertIn(key, data)
            self.assertIn("description", data[key])
            self.assertIn("example", data[key])

    # ---- POST /api/analyze - valid expressions ---------------------------------------------

    def test_analyze_valid_infix(self):
        resp = self.client.post("/api/analyze", json={"expression": "(25 + 15) * 2"})
        self.assertEqual(resp.status_code, 200)
        data = resp.get_json()
        self.assertTrue(data["success"])
        self.assertTrue(data["valid"])
        self.assertEqual(data["notation"], "INFIX")
        self.assertEqual(data["postfix"], "25 15 + 2 *")
        self.assertEqual(data["prefix"], "* + 25 15 2")
        self.assertEqual(data["result"], 80)
        self.assertIsNone(data["diagnosis"])
        self.assertEqual(data["corrections"], [])
        self.assertEqual(len(data["steps"]), 5)          # real trace from the C engine
        self.assertEqual(data["steps"][-1]["stack"], [80])

    def test_analyze_valid_postfix(self):
        resp = self.client.post("/api/analyze", json={"expression": "25 15 + 2 *"})
        data = resp.get_json()
        self.assertTrue(data["valid"])
        self.assertEqual(data["notation"], "POSTFIX")
        self.assertEqual(data["result"], 80)

    def test_analyze_valid_prefix(self):
        resp = self.client.post("/api/analyze", json={"expression": "* + 25 15 2"})
        data = resp.get_json()
        self.assertTrue(data["valid"])
        self.assertEqual(data["notation"], "PREFIX")
        self.assertEqual(data["result"], 80)

    def test_analyze_with_variables(self):
        resp = self.client.post("/api/analyze", json={
            "expression": "(A + B) * C",
            "variables": {"A": 10, "B": 20, "C": 2},
        })
        data = resp.get_json()
        self.assertTrue(data["valid"])
        self.assertEqual(data["result"], 60)

    # ---- POST /api/analyze - invalid expressions ---------------------------------------------

    def test_analyze_invalid_consecutive_operators(self):
        resp = self.client.post("/api/analyze", json={"expression": "25 + * 2"})
        self.assertEqual(resp.status_code, 200)   # the API call itself succeeded
        data = resp.get_json()
        self.assertTrue(data["success"])
        self.assertFalse(data["valid"])
        self.assertEqual(data["notation"], "INVALID")
        self.assertIsNotNone(data["diagnosis"])
        self.assertEqual(data["diagnosis"]["position"], 5)
        self.assertIn("consecutive", data["diagnosis"]["message"].lower())
        self.assertGreaterEqual(len(data["corrections"]), 1)

    def test_analyze_division_by_zero(self):
        resp = self.client.post("/api/analyze", json={"expression": "25 / 0"})
        data = resp.get_json()
        # Structurally valid - division by zero is an evaluation problem,
        # not a syntax problem - so `valid` stays True and `result` is null.
        self.assertTrue(data["valid"])
        self.assertIsNone(data["result"])
        self.assertIsNotNone(data["diagnosis"])
        self.assertIn("zero", data["diagnosis"]["message"].lower())

    # ---- POST /api/analyze - request validation ---------------------------------------------

    def test_analyze_missing_expression_field(self):
        resp = self.client.post("/api/analyze", json={})
        self.assertEqual(resp.status_code, 400)
        data = resp.get_json()
        self.assertFalse(data["success"])
        self.assertIn("error", data)

    def test_analyze_empty_expression_string(self):
        resp = self.client.post("/api/analyze", json={"expression": "   "})
        self.assertEqual(resp.status_code, 400)
        self.assertFalse(resp.get_json()["success"])

    def test_analyze_non_json_body(self):
        resp = self.client.post("/api/analyze", data="not json",
                                content_type="text/plain")
        self.assertEqual(resp.status_code, 400)
        self.assertFalse(resp.get_json()["success"])

    def test_analyze_invalid_variable_name(self):
        resp = self.client.post("/api/analyze", json={
            "expression": "A + B",
            "variables": {"AB": 10},   # not a single letter
        })
        self.assertEqual(resp.status_code, 400)

    def test_analyze_invalid_variable_value(self):
        resp = self.client.post("/api/analyze", json={
            "expression": "A + B",
            "variables": {"A": "ten"},   # not a number
        })
        self.assertEqual(resp.status_code, 400)

    def test_analyze_bracket_mismatch(self):
        data = self.client.post("/api/analyze", json={"expression": "(25 + 15] * 2"}).get_json()
        self.assertFalse(data["valid"])
        self.assertEqual(data["notation"], "INVALID")
        self.assertIn("bracket", data["diagnosis"]["message"].lower())

    def test_analyze_missing_variable_is_reported(self):
        data = self.client.post("/api/analyze", json={"expression": "A + B * C"}).get_json()
        self.assertTrue(data["valid"])
        self.assertIsNone(data["result"])
        self.assertEqual(data["variables_required"], ["A", "B", "C"])

    def test_analyze_variables_a_plus_b_times_c(self):
        data = self.client.post("/api/analyze", json={
            "expression": "A + B * C", "variables": {"A": 10, "B": 20, "C": 2}}).get_json()
        self.assertEqual(data["result"], 50)

    # ---- error handling ---------------------------------------------

    def test_unknown_route_returns_json_404(self):
        resp = self.client.get("/api/does-not-exist")
        self.assertEqual(resp.status_code, 404)
        data = resp.get_json()
        self.assertFalse(data["success"])
        self.assertIn("error", data)

    def test_wrong_method_returns_json_405(self):
        resp = self.client.get("/api/analyze")   # analyze is POST-only
        self.assertEqual(resp.status_code, 405)
        data = resp.get_json()
        self.assertFalse(data["success"])


if __name__ == "__main__":
    unittest.main()
