"""
expression_service.py

THE ONE BOUNDARY between Flask and the C DSA engine.

Routes call ExpressionService().analyze(expression, variables). This file
then runs the compiled C adapter (dsa_engine/integration/analyzer_cli.c),
which calls the existing analyze_expression() in dsa_engine/analyzer/.

Nothing here tokenizes, validates, detects notation, converts or evaluates
anything - every value in the response is read from the C program's output.
There is no mock and no Python re-implementation of the DSA logic.

How the executable is found
---------------------------
1. environment variable EXPRESSION_ENGINE_PATH (if set), else
2. <project>/build/analyzer_cli(.exe)
If it does not exist yet, gcc is asked to build it from the C sources (the
same source list as the Makefile). If gcc is missing, EngineUnavailableError
is raised and the API answers with a clear JSON error.
"""

import json
import math
import os
import subprocess
import sys

from services.trace_parser import parse_trace

BACKEND_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROJECT_DIR = os.path.dirname(BACKEND_DIR)
ENGINE_DIR = os.path.join(PROJECT_DIR, "dsa_engine")
BUILD_DIR = os.path.join(PROJECT_DIR, "build")
EXE_NAME = "analyzer_cli.exe" if os.name == "nt" else "analyzer_cli"

# Same C sources the Makefile / build_engine.bat use.
_C_SOURCES = [
    "integration/analyzer_cli.c",
    "tokenizer/tokenizer.c",
    "stack/stack.c",
    "validation/bracket_validator.c",
    "validation/syntax_validator.c",
    "notation/notation_detector.c",
    "notation/conversion.c",
    "evaluation/evaluator.c",
    "evaluation/variable_manager.c",
    "analyzer/expression_analyzer.c",
    "analyzer/diagnosis.c",
    "analyzer/correction_generator.c",
]

MAX_EXPRESSION_CHARS = 255      # the C analyzer copies at most 255 characters
ENGINE_TIMEOUT_SECONDS = 5


class EngineUnavailableError(RuntimeError):
    """The C engine executable is missing and could not be built, or it crashed."""


def engine_path():
    return os.environ.get("EXPRESSION_ENGINE_PATH") or os.path.join(BUILD_DIR, EXE_NAME)


def build_engine():
    """Compiles dsa_engine/integration/analyzer_cli.c + the existing modules."""
    os.makedirs(BUILD_DIR, exist_ok=True)
    out = os.path.join(BUILD_DIR, EXE_NAME)
    cmd = ["gcc", "-Wall", "-Wextra", "-std=c99", "-O2", "-o", out]
    cmd += [os.path.join(ENGINE_DIR, src) for src in _C_SOURCES]
    cmd += ["-lm"]
    try:
        proc = subprocess.run(cmd, capture_output=True, timeout=120)
    except FileNotFoundError:
        raise EngineUnavailableError(
            "The C engine is not compiled and gcc was not found. Run "
            "build_engine.bat (Windows) or 'make engine' in the project "
            "folder, then try again.")
    if proc.returncode != 0:
        raise EngineUnavailableError(
            "Compiling the C engine failed:\n" +
            proc.stderr.decode("utf-8", "replace")[-1500:])
    return out


def _run_engine(exe, mode, request_text):
    try:
        proc = subprocess.run(
            [exe, mode], input=request_text.encode("utf-8"),
            capture_output=True, timeout=ENGINE_TIMEOUT_SECONDS)
    except subprocess.TimeoutExpired:
        raise EngineUnavailableError("The C engine did not answer in time.")
    except OSError as exc:
        raise EngineUnavailableError("Could not start the C engine: %s" % exc)
    if proc.returncode != 0:
        raise EngineUnavailableError(
            "The C engine exited with code %d: %s" %
            (proc.returncode, proc.stderr.decode("utf-8", "replace").strip()))
    return proc.stdout.decode("utf-8", "replace")


class ExpressionService:
    """What the routes use. Validates the REQUEST, then asks the C engine."""

    def analyze(self, expression, variables=None):
        if not isinstance(expression, str) or not expression.strip():
            raise ValueError("'expression' must be a non-empty string.")
        if "\n" in expression or "\r" in expression:
            raise ValueError("'expression' must be a single line.")
        if len(expression) > MAX_EXPRESSION_CHARS:
            raise ValueError("'expression' is too long (max %d characters)."
                             % MAX_EXPRESSION_CHARS)

        lines = [expression]
        if variables is not None:
            if not isinstance(variables, dict):
                raise ValueError("'variables' must be a JSON object of "
                                 "{letter: number} pairs.")
            for name, value in variables.items():
                if not isinstance(name, str) or len(name) != 1 or not (
                        name.isascii() and name.isalpha()):
                    raise ValueError("Variable name '%s' is invalid - variables "
                                     "must be single letters (A-Z or a-z)." % name)
                if (not isinstance(value, (int, float)) or isinstance(value, bool)
                        or not math.isfinite(value)):
                    raise ValueError("Value for variable '%s' must be a finite "
                                     "number." % name)
                lines.append("%s=%s" % (name, format(value, ".17g")))
        request_text = "\n".join(lines) + "\n"

        exe = engine_path()
        if not os.path.isfile(exe):
            exe = build_engine()

        raw = _run_engine(exe, "--json", request_text)
        try:
            analysis = json.loads(raw)
        except ValueError:
            raise EngineUnavailableError(
                "The C engine returned output that is not valid JSON: %r" % raw[:200])

        # Stack trace: printed by the C evaluator itself (show_steps = 1).
        analysis["steps"] = []
        evaluation = analysis.get("evaluation") or {}
        if analysis.get("valid") and evaluation.get("attempted"):
            analysis["steps"] = parse_trace(_run_engine(exe, "--trace", request_text))

        return analysis
