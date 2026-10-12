"""
response_models.py

Builds the JSON that POST /api/analyze returns. It only reshapes the dict
produced by services/expression_service.py (which came from the C engine);
it computes nothing.
"""


def build_analysis_response(analysis: dict) -> dict:
    return {
        "success": True,                       # the API call itself worked
        "expression": analysis.get("expression", ""),
        "valid": bool(analysis.get("valid", False)),
        "notation": analysis.get("notation", "INVALID"),
        "infix": analysis.get("infix"),
        "postfix": analysis.get("postfix"),
        "prefix": analysis.get("prefix"),
        "result": analysis.get("result"),      # null unless the C engine evaluated it
        "evaluation": analysis.get("evaluation") or {"attempted": False},
        "diagnosis": analysis.get("diagnosis"),
        "corrections": analysis.get("corrections") or [],
        # These fields are consumed by the redesigned frontend after the user
        # submits the expression. Keep stable empty values so the UI can render
        # every result state without special-casing missing JSON properties.
        "variables_used": analysis.get("variables_used") or [],
        "variables_required": analysis.get("variables_required") or [],
        "steps": analysis.get("steps") or [],
    }


def build_error_response(message: str, status_code: int = 400) -> tuple:
    return {"success": False, "error": message}, status_code
