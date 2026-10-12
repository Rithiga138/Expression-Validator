# Expression Validator & Evaluator — Backend

A small Flask REST API that will eventually sit between the frontend and
the existing **C DSA engine** (`../dsa_engine/`). It does **not**
tokenize, validate, convert, or evaluate expressions itself — see
"What is temporary vs. real" below.

## 1. Folder structure

```
backend/
├── app.py                        Flask app factory + startup
├── requirements.txt
├── routes/
│   ├── __init__.py
│   └── expression_routes.py      HTTP endpoints only, no DSA logic
├── services/
│   ├── __init__.py
│   └── expression_service.py     The ONLY boundary to the DSA engine
├── models/
│   ├── __init__.py
│   └── response_models.py        JSON response shape helpers
├── tests/
│   └── test_api.py               API tests (Flask test client)
└── README.md
```

## 2. Install dependencies

From inside the `backend/` folder:

```bash
# (recommended) create a virtual environment first
python -m venv venv

# Windows:
venv\Scripts\activate
# macOS / Linux:
source venv/bin/activate

# install dependencies
pip install -r requirements.txt
```

## 3. Run the Flask server

Either of these works (run from inside `backend/`):

```bash
python app.py
```

or, using the Flask CLI:

```bash
flask --app app run --debug
```

By default the server listens on **http://127.0.0.1:5000**.

## 4. Test `GET /api/health`

```bash
curl http://127.0.0.1:5000/api/health
```

Expected response:

```json
{
  "status": "ok",
  "service": "Expression Validator & Evaluator API"
}
```

## 5. Test `POST /api/analyze`

Simple expression, no variables:

```bash
curl -X POST http://127.0.0.1:5000/api/analyze \
  -H "Content-Type: application/json" \
  -d "{\"expression\": \"(25 + 15) * 2\"}"
```

With variables:

```bash
curl -X POST http://127.0.0.1:5000/api/analyze \
  -H "Content-Type: application/json" \
  -d "{\"expression\": \"(A + B) * C\", \"variables\": {\"A\": 10, \"B\": 20, \"C\": 2}}"
```

An intentionally invalid expression (to see diagnosis + corrections):

```bash
curl -X POST http://127.0.0.1:5000/api/analyze \
  -H "Content-Type: application/json" \
  -d "{\"expression\": \"25 + * 2\"}"
```

## 6. Other endpoints

| Method | Path                  | Purpose                                   |
|--------|------------------------|--------------------------------------------|
| GET    | `/api/health`          | Liveness check                             |
| GET    | `/api/examples`        | Sample expressions for the frontend        |
| GET    | `/api/notation-info`   | Explains infix / prefix / postfix          |
| POST   | `/api/analyze`         | Analyze one expression (see above)         |

## 7. Run the tests

From inside `backend/`:

```bash
python -m unittest tests.test_api -v
```

or, if you have `pytest` installed:

```bash
pytest -v
```

Both work — the tests are written with Python's built-in `unittest`
module, which `pytest` can also discover and run directly, so no test
framework beyond Flask itself is a hard requirement.

## 8. How the backend talks to the C engine

`services/expression_service.py` runs `build/analyzer_cli(.exe)` (source:
`dsa_engine/integration/analyzer_cli.c`), a thin adapter that calls the
existing `analyze_expression()` and prints JSON. Flask only reads that JSON;
no DSA logic exists in Python or JavaScript, and there is no mock.

- If the executable is missing, Flask compiles it automatically with `gcc`.
- Override the location with the `EXPRESSION_ENGINE_PATH` environment variable.
- Extra routes: `GET /api/notations` (list form of `/api/notation-info`).
- If the engine cannot be built or run, `/api/analyze` returns HTTP 503 with a
  JSON `error` message.

## 9. Troubleshooting: two different answers from port 5000
`python app.py` refuses to start if something already listens on port 5000.
Every response carries an `X-Backend-Instance: pid=... started=...` header
(`curl -i http://127.0.0.1:5000/api/health`) that shows which process answered.
Find and stop a stale process (Windows):
`netstat -ano | findstr :5000`  then  `taskkill /PID <pid> /F`.
