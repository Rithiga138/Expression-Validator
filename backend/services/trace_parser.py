"""
trace_parser.py

Turns the plain-text step trace that the C evaluator prints (evaluator.c,
show_steps = 1) into the list of step objects the frontend animates:

    {"step": 3, "operations": ["POP 15", "POP 25", "APPLY 25 + 15 = 40",
                               "PUSH 40"], "stack": [40], "note": ""}

This is ONLY text -> structure reformatting of lines the C engine already
produced. It calculates nothing: every number, operator and stack content
below is copied out of the engine's own output.
"""

import re

_STEP_PUSH = re.compile(r"^\s*(\d+)\.\s+Push\s+(\S+)(?:\s+\(=\s*(\S+)\))?\s*$")
_STEP_OP = re.compile(r"^\s*(\d+)\.\s+(\S)\s+Pop\s+(\S+)\s+and\s+(\S+)\s*$")
_CALC = re.compile(r"^\s*Calculate\s+(.+?),\s*Push\s+(\S+)\s*$")
_CANNOT = re.compile(r"^\s*Cannot calculate\s+(.+?)\s*$")
_STACK = re.compile(r"^\s*Stack:\s*\[(.*)\]\s*$")


def _number(text):
    """'80' -> 80, '2.5' -> 2.5 (copied from the engine's %g output)."""
    try:
        as_float = float(text)
    except ValueError:
        return text
    return int(as_float) if as_float.is_integer() else as_float


def parse_trace(text):
    """Returns a list of step dicts (empty list if there is no trace)."""
    steps = []
    current = None

    for line in text.splitlines():
        if not line.strip():
            continue

        m = _STEP_PUSH.match(line)
        if m:
            current = {"step": int(m.group(1)), "operations": [], "stack": None, "note": ""}
            if m.group(3) is not None:
                current["operations"].append("PUSH %s (= %s)" % (m.group(2), m.group(3)))
            else:
                current["operations"].append("PUSH %s" % m.group(2))
            steps.append(current)
            continue

        m = _STEP_OP.match(line)
        if m:
            current = {"step": int(m.group(1)), "operations": [
                "POP %s" % m.group(3), "POP %s" % m.group(4)], "stack": None, "note": ""}
            steps.append(current)
            continue

        if current is None:
            continue   # header lines such as "Converted postfix: ..."

        m = _CALC.match(line)
        if m:
            current["operations"].append("APPLY %s" % m.group(1))
            current["operations"].append("PUSH %s" % m.group(2))
            continue

        m = _CANNOT.match(line)
        if m:
            current["operations"].append("CANNOT CALCULATE %s" % m.group(1))
            current["note"] = "Evaluation stopped at this step."
            continue

        m = _STACK.match(line)
        if m:
            inner = m.group(1).strip()
            current["stack"] = [_number(p.strip()) for p in inner.split(",")] if inner else []
            continue

    return steps
