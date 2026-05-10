#!/usr/bin/env python3
"""Smoke tests for the CLI-backed Python automation preview."""

from __future__ import annotations

import os
import unittest
from pathlib import Path
from unittest import mock

from topoexec_preview import GraphDocument, TopoExecClient, TopoExecCommandError


class PythonPreviewSmoke(unittest.TestCase):
    def setUp(self) -> None:
        executable = os.environ["TOPOEXEC_PYTHON_PREVIEW_EXE"]
        source_dir = os.environ["TOPOEXEC_SOURCE_DIR"]
        self.client = TopoExecClient(executable)
        self.minimal_graph = Path(source_dir) / "examples" / "minimal.yaml"

    def test_validate_plan_metrics_and_trace_from_file(self) -> None:
        validation = self.client.validate(self.minimal_graph)
        self.assertTrue(validation.ok)
        self.assertEqual(validation.data["region_order"], ["source", "transform", "sink"])

        plan = self.client.plan(self.minimal_graph)
        self.assertTrue(plan.ok)
        self.assertEqual(plan.data["graph"], "minimal")
        self.assertGreaterEqual(len(plan.data["components"]), 3)

        run = self.client.run(self.minimal_graph, steps=1)
        self.assertTrue(run.ok)
        self.assertEqual(run.data["graph_name"], "minimal")
        self.assertGreaterEqual(run.data["component_count"], 3)

        metrics = self.client.metrics(self.minimal_graph, steps=1)
        metric_names = {sample["name"] for sample in metrics.data["metrics"]}
        self.assertIn("runtime.component.execution_count", metric_names)

        trace = self.client.trace(self.minimal_graph, steps=1)
        self.assertTrue(trace.ok)
        self.assertGreater(len(trace.data["trace"]), 0)

    def test_text_graph_materialization_and_check_false_errors(self) -> None:
        text_graph = GraphDocument.from_text(
            self.minimal_graph.read_text(encoding="utf-8"), name="minimal-copy.yaml"
        )
        self.assertTrue(self.client.validate(text_graph).ok)

        invalid_graph = GraphDocument.from_text("schema_version: 1\ngraph:\n  name: invalid\n", name="invalid.yaml")
        result = self.client.validate(invalid_graph, check=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(result.data["ok"])
        self.assertGreater(len(result.data["errors"]), 0)


class PythonPreviewUnit(unittest.TestCase):
    def test_check_true_raises_when_json_ok_false_with_zero_exit(self) -> None:
        client = TopoExecClient("topoexec")
        completed = mock.Mock(returncode=0, stdout='{"ok": false, "errors": ["x"]}', stderr="")
        with mock.patch("topoexec_preview.client.subprocess.run", return_value=completed):
            with self.assertRaises(TopoExecCommandError):
                client.validate("graph.yaml", check=True)

    def test_check_false_returns_result_when_json_ok_false_with_zero_exit(self) -> None:
        client = TopoExecClient("topoexec")
        completed = mock.Mock(returncode=0, stdout='{"ok": false, "errors": ["x"]}', stderr="")
        with mock.patch("topoexec_preview.client.subprocess.run", return_value=completed):
            result = client.validate("graph.yaml", check=False)
        self.assertEqual(result.returncode, 0)
        self.assertFalse(result.ok)
        self.assertFalse(result.data["ok"])


if __name__ == "__main__":
    unittest.main()
