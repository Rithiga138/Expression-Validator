"""
expression_routes.py

All HTTP endpoints for the Expression Validator & Evaluator API.

Each route function is intentionally short: it reads the request, calls
ExpressionService (the ONLY place allowed to talk to the DSA engine), and
formats the result with a helper from models/response_models.py. There is
no parsing, validating, converting, or evaluating of expressions here -
that separation is the whole point of this file existing.
"""

from flask import Blueprint, jsonify, request

import config

from services.expression_service import EngineUnavailableError, ExpressionService
from models.response_models import build_analysis_response, build_error_response

# A Blueprint groups related routes together so app.py can register them
# with one line (app.register_blueprint(expression_bp)) instead of
# defining every route directly on the Flask app object.
expression_bp = Blueprint("expression_routes", __name__)

# One shared, stateless ExpressionService (it runs the compiled C engine).
_service = ExpressionService()


@expression_bp.route("/api/health", methods=["GET"])
def health():
    """A simple liveness check - useful for the frontend, load balancers,
    or just confirming the server started correctly."""
    return jsonify({
        "status": "ok",
        "service": config.SERVICE_NAME,
    })


@expression_bp.route("/api/examples", methods=["GET"])
def examples():
    """A handful of example expressions the frontend can offer as
    clickable samples, covering infix, prefix, postfix, and one
    deliberately invalid expression."""
    return jsonify({
        "examples": [
            "(25 + 15) * 2",
            "A + B * C",
            "25 15 + 2 *",
            "* + 25 15 2",
            "25 + * 2",
        ]
    })


@expression_bp.route("/api/notation-info", methods=["GET"])
def notation_info():
    """Short, beginner-friendly explanations of the three notations, for
    an "About" panel or tooltip in the frontend."""
    return jsonify({
        "infix": {
            "name": "Infix",
            "description": (
                "The operator sits BETWEEN its two operands, e.g. "
                "'A + B'. This is how people normally write math, and "
                "it needs parentheses and operator precedence rules to "
                "stay unambiguous."
            ),
            "example": "(A + B) * C",
        },
        "prefix": {
            "name": "Prefix (Polish notation)",
            "description": (
                "The operator comes BEFORE its two operands, e.g. "
                "'+ A B'. No parentheses are ever needed, and it is "
                "evaluated by scanning from RIGHT to LEFT."
            ),
            "example": "* + A B C",
        },
        "postfix": {
            "name": "Postfix (Reverse Polish notation)",
            "description": (
                "The operator comes AFTER its two operands, e.g. "
                "'A B +'. No parentheses are ever needed, and it is "
                "evaluated by scanning from LEFT to RIGHT using a stack."
            ),
            "example": "A B + C *",
        },
    })


@expression_bp.route("/api/notations", methods=["GET"])
def notations():
    """Same text as /api/notation-info, as a list (the shape the frontend reads)."""
    info = notation_info().get_json()
    return jsonify({"notations": [
        {"name": key, "example": value["example"], "description": value["description"]}
        for key, value in info.items()
    ]})


@expression_bp.route("/api/analyze", methods=["POST"])
def analyze():
    """
    Body:
        {"expression": "(25 + 15) * 2"}
        {"expression": "(A + B) * C", "variables": {"A": 10, "B": 20, "C": 2}}

    This route never evaluates anything itself. It:
      1. reads and lightly sanity-checks the JSON body,
      2. calls ExpressionService.analyze(...) (the DSA-engine boundary),
      3. formats whatever came back with build_analysis_response().
    """
    body = request.get_json(silent=True)
    if body is None or not isinstance(body, dict):
        return build_error_response(
            "Request body must be a JSON object, e.g. "
            '{"expression": "(25 + 15) * 2"}.'
        )

    expression = body.get("expression")
    variables = body.get("variables")

    try:
        analysis = _service.analyze(expression, variables)
    except ValueError as exc:
        # Raised by ExpressionService for a malformed REQUEST (missing
        # expression, bad variable types, etc.) - that is a client error.
        return build_error_response(str(exc))
    except EngineUnavailableError as exc:
        # The C engine could not be found/built/run - a server-side problem.
        return build_error_response(str(exc), status_code=503)

    return jsonify(build_analysis_response(analysis))
