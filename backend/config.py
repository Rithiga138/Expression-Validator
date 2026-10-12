"""
config.py - the ONE place for backend settings.

Change HOST/PORT here only. The frontend (frontend/js/api.js) must point at
the same address: http://127.0.0.1:5000
"""

SERVICE_NAME = "Expression Validator & Evaluator API"
HOST = "127.0.0.1"
PORT = 5000

# Browsers on these origins may call the API (the frontend is served on 5500).
ALLOWED_ORIGINS = (
    "http://127.0.0.1:5500",
    "http://localhost:5500",
    "http://127.0.0.1:5501",
    "http://localhost:5501",
)
