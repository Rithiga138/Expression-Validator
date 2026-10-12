"""
app.py

Entry point for the Expression Validator & Evaluator Flask backend.

Run it directly during development:
    python app.py

Or with the Flask CLI (see README.md for the exact commands):
    flask --app app run --debug

This file only wires things together (create the app, enable CORS,
register routes, register error handlers) - it contains no DSA logic and
no HTTP-endpoint logic of its own; those live in routes/ and services/.
"""

import os
import socket
import sys
import time

from flask import Flask, jsonify, request

import config

from routes.expression_routes import expression_bp
from models.response_models import build_error_response


def create_app() -> Flask:
    """
    Application factory. Using a function (instead of a bare module-level
    `app = Flask(__name__)`) makes the app easy to import and re-create
    from tests/test_api.py without any import-order surprises.
    """
    app = Flask(__name__)

    # CORS (no extra package). The frontend runs on another origin
    # (http://127.0.0.1:5500 / http://localhost:5500), so every response -
    # including the browser's OPTIONS preflight - must carry these headers.
    # No cookies or credentials are used.
    started_at = time.strftime("%Y-%m-%d %H:%M:%S")

    @app.after_request
    def add_headers(response):
        origin = request.headers.get("Origin", "")
        if origin in config.ALLOWED_ORIGINS:
            response.headers["Access-Control-Allow-Origin"] = origin
            response.headers["Vary"] = "Origin"
        response.headers["Access-Control-Allow-Headers"] = "Content-Type"
        response.headers["Access-Control-Allow-Methods"] = "GET, POST, OPTIONS"
        # Tells you WHICH backend process answered (see README: port conflicts).
        response.headers["X-Backend-Instance"] = "pid=%d started=%s" % (os.getpid(), started_at)
        return response

    # All of the actual endpoints live in this Blueprint.
    app.register_blueprint(expression_bp)

    # ---- JSON error handling -------------------------------------------------
    # By default, Flask returns HTML error pages (e.g. for a 404 or an
    # unhandled exception). Since this is a JSON API, every error response
    # should be JSON too, so the frontend never has to guess the format.

    @app.errorhandler(404)
    def not_found(_error):
        body, status = build_error_response(
            "This endpoint does not exist. See README.md for the full "
            "list of routes.", status_code=404
        )
        return jsonify(body), status

    @app.errorhandler(405)
    def method_not_allowed(_error):
        body, status = build_error_response(
            "This HTTP method is not allowed on this endpoint.",
            status_code=405
        )
        return jsonify(body), status

    @app.errorhandler(Exception)
    def unexpected(error):
        from werkzeug.exceptions import HTTPException
        if isinstance(error, HTTPException):
            return error
        app.logger.exception("Unhandled error")
        body, status = build_error_response(
            "Unexpected server error: %s" % error, status_code=500)
        return jsonify(body), status

    @app.errorhandler(500)
    def internal_error(_error):
        body, status = build_error_response(
            "An unexpected server error occurred.", status_code=500
        )
        return jsonify(body), status

    @app.route("/", methods=["GET"])
    def index():
        return jsonify({
            "service": config.SERVICE_NAME,
            "routes": sorted(
                "%s %s" % (",".join(sorted(r.methods - {"HEAD", "OPTIONS"})), r.rule)
                for r in app.url_map.iter_rules() if r.endpoint != "static"),
        })

    return app


# A module-level `app` is required by `flask --app app run` and by some
# WSGI servers, so we create one instance here using the factory above.
app = create_app()


def _port_in_use(host, port):
    """True if something already accepts connections on host:port."""
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
        probe.settimeout(1)
        return probe.connect_ex((host, port)) == 0


def _print_startup_banner():
    print("=" * 68)
    print(" %s" % config.SERVICE_NAME)
    print(" backend file : %s" % os.path.abspath(__file__))
    print(" listening on : http://%s:%d   (pid %d)" % (config.HOST, config.PORT, os.getpid()))
    print(" allowed CORS : %s" % ", ".join(config.ALLOWED_ORIGINS))
    print(" registered API routes:")
    for rule in sorted(app.url_map.iter_rules(), key=lambda r: r.rule):
        if rule.endpoint == "static":
            continue
        methods = ",".join(sorted(rule.methods - {"HEAD", "OPTIONS"}))
        print("   %-6s %s" % (methods, rule.rule))
    print("=" * 68)


if __name__ == "__main__":
    # Windows lets a second server bind a port that is already taken, which
    # makes two different backends answer on the same port. Refuse to start.
    if _port_in_use(config.HOST, config.PORT):
        print("ERROR: port %d is already in use, so another program (probably an old "
              "copy of this backend) is listening there." % config.PORT)
        print("Find it:  netstat -ano | findstr :%d" % config.PORT)
        print("Stop it:  taskkill /PID <pid> /F     then run  python app.py  again.")
        sys.exit(1)

    _print_startup_banner()
    # use_reloader=False: the reloader starts a second process, which is how
    # stale backend processes get left behind.
    app.run(host=config.HOST, port=config.PORT, debug=True, use_reloader=False)
