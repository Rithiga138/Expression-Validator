/* ==========================================================================
   api.js – the ONLY file that talks to the Flask backend.

   Public object:  Api
     Api.analyzeExpression(expression, variables)  -> normalized analysis
     Api.getExamples()                             -> [{expression, label}]
     Api.getNotationInfo()                         -> [{name, example, description}] | null
     Api.checkHealth()                             -> { online: boolean, mock: boolean }

   No validation, conversion or evaluation happens here. This file only sends
   the text to the backend and cleans up the JSON so the UI never has to worry
   about missing optional fields.
   ========================================================================== */

/* ---------- Configuration (change the backend address ONLY here) ---------- */
const API_BASE_URL = "http://127.0.0.1:5000";   // Flask backend (see backend/app.py)

const API_ENDPOINTS = {
  analyze:   "/api/analyze",    // POST { expression, variables? }
  examples:  "/api/examples",   // GET  (optional endpoint)
  notations: "/api/notations",  // GET  (optional endpoint)
  health:    "/api/health"      // GET  (any 2xx response = online)
};

const REQUEST_TIMEOUT_MS = 8000;

/* Open index.html?mock=1 to preview the UI without a backend (see the
   clearly-marked MOCK section at the bottom of this file). */
const MOCK_MODE = new URLSearchParams(window.location.search).get("mock") === "1";

/* ---------- Error type used by app.js to choose the right UI state ---------- */
class ApiError extends Error {
  /* kind: "unavailable" (server not reachable / timeout)
           "http"        (server answered with an error status)
           "bad_response"(server answered, but not with usable JSON) */
  constructor(kind, message, details) {
    super(message);
    this.name = "ApiError";
    this.kind = kind;
    this.details = details || null;
  }
}

/* ---------- Low-level request helper ---------- */
async function requestJson(path, options) {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), REQUEST_TIMEOUT_MS);
  let response;

  try {
    response = await fetch(API_BASE_URL + path, { ...options, signal: controller.signal });
  } catch (err) {
    // fetch() only rejects when the server cannot be reached (or on timeout / CORS block).
    throw new ApiError("unavailable", "Backend connection unavailable", String(err && err.message || err));
  } finally {
    clearTimeout(timer);
  }

  let body = null;
  try {
    body = await response.json();
  } catch (err) {
    body = null;
  }

  if (!response.ok) {
    const serverMessage = body && (body.error || body.message);
    throw new ApiError("http", serverMessage || ("Server responded with status " + response.status), "HTTP " + response.status);
  }
  if (body === null) {
    throw new ApiError("bad_response", "The backend did not return valid JSON.");
  }
  return body;
}

/* ---------- Normalizers: tolerate missing optional fields ---------- */
function asText(value) {
  return typeof value === "string" ? value : "";
}

function normalizeDiagnosis(raw) {
  if (!raw) return null;
  if (typeof raw === "string") return { message: raw, position: null, expected: "", found: "", category: "" };
  const position = Number.isInteger(raw.position) && raw.position >= 0 ? raw.position : null;
  return {
    message:  asText(raw.message) || "No further details were provided.",
    position: position,
    expected: asText(raw.expected),
    found:    asText(raw.found),
    category: asText(raw.category)
  };
}

function normalizeCorrections(raw) {
  if (!Array.isArray(raw)) return [];
  return raw
    .map((item) => {
      if (typeof item === "string") return { expression: item, reason: "" };
      return {
        expression: asText(item && item.expression),
        // Engine field is "reason"; "explanation"/"description" accepted as aliases.
        reason: asText(item && (item.reason || item.explanation || item.description))
      };
    })
    .filter((c) => c.expression !== "");
}

/* Backend may list required variables as an array (["A","B"]) or as a string ("AB"). */
function normalizeVariableList(raw) {
  const list = raw.variables_required || raw.required_variables || raw.variablesRequired ||
               (Array.isArray(raw.variables) || typeof raw.variables === "string" ? raw.variables : []);
  const letters = typeof list === "string" ? list.split("") : (Array.isArray(list) ? list : []);
  const unique = [];
  letters.forEach((name) => {
    const n = String(name).trim();
    if (n && !unique.includes(n)) unique.push(n);
  });
  return unique;
}

function normalizeAnalysis(raw, sentExpression) {
  if (!raw || typeof raw !== "object") {
    throw new ApiError("bad_response", "The backend response was not a JSON object.");
  }
  const valid = raw.valid === true;
  const hasResult = raw.result !== null && raw.result !== undefined && raw.result !== "";

  return {
    success:    raw.success !== false,                // false = backend-level failure
    error:      asText(raw.error) || (raw.success === false ? asText(raw.message) : ""),
    expression: typeof raw.expression === "string" ? raw.expression : sentExpression,
    valid:      valid,
    notation:   asText(raw.notation).toUpperCase() || (valid ? "UNKNOWN" : "INVALID"),
    infix:      asText(raw.infix),
    postfix:    asText(raw.postfix),
    prefix:     asText(raw.prefix),
    result:     hasResult ? raw.result : null,
    diagnosis:  normalizeDiagnosis(raw.diagnosis),
    corrections: normalizeCorrections(raw.corrections),
    steps:      Array.isArray(raw.steps) ? raw.steps : [],
    variablesRequired: normalizeVariableList(raw)
  };
}

/* ---------- Public API ---------- */
const Api = {
  /* Read-only info for the UI (so no other file hard-codes the address). */
  baseUrl: API_BASE_URL,
  mockMode: MOCK_MODE,

  /* variables: optional object such as { A: 10, B: 20 } */
  async analyzeExpression(expression, variables) {
    if (MOCK_MODE) return MockBackend.analyze(expression, variables);

    const payload = { expression: expression };
    if (variables && Object.keys(variables).length > 0) payload.variables = variables;

    const raw = await requestJson(API_ENDPOINTS.analyze, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload)
    });
    return normalizeAnalysis(raw, expression);
  },

  /* Optional endpoint. Falls back to the built-in list if it is missing. */
  async getExamples() {
    if (MOCK_MODE) return DEFAULT_EXAMPLES;
    try {
      const raw = await requestJson(API_ENDPOINTS.examples, { method: "GET" });
      const list = Array.isArray(raw) ? raw : raw.examples;
      if (!Array.isArray(list)) return DEFAULT_EXAMPLES;
      const cleaned = list
        .map((e) => typeof e === "string" ? { expression: e, label: e }
                                          : { expression: asText(e && e.expression), label: asText(e && (e.label || e.expression)) })
        .filter((e) => e.expression !== "");
      return cleaned.length ? cleaned : DEFAULT_EXAMPLES;
    } catch (err) {
      return DEFAULT_EXAMPLES;
    }
  },

  /* Optional endpoint. Returns null when unavailable; index.html already has text. */
  async getNotationInfo() {
    if (MOCK_MODE) return null;
    try {
      const raw = await requestJson(API_ENDPOINTS.notations, { method: "GET" });
      const list = Array.isArray(raw) ? raw : raw.notations;
      return Array.isArray(list) ? list.map((n) => ({
        name: asText(n.name).toUpperCase(), example: asText(n.example), description: asText(n.description)
      })) : null;
    } catch (err) {
      return null;
    }
  },

  /* Online only when /api/health answers 2xx with {"status":"ok"}. A 404 means
     something else (or an old copy of the backend) is listening on that port. */
  async checkHealth() {
    if (MOCK_MODE) return { online: true, mock: true };
    try {
      const body = await requestJson(API_ENDPOINTS.health, { method: "GET" });
      return { online: !!body && body.status === "ok", mock: false };
    } catch (err) {
      return { online: false, mock: false };
    }
  }
};

/* ==========================================================================
   STATIC UI CONTENT (not mock analysis)
   Used when the optional /api/examples endpoint is not available.
   ========================================================================== */
const DEFAULT_EXAMPLES = [
  { expression: "(25 + 15) * 2",  label: "(25 + 15) * 2" },
  { expression: "A + B * C",      label: "A + B * C" },
  { expression: "25 15 + 2 *",    label: "25 15 + 2 *" },
  { expression: "* + 25 15 2",    label: "* + 25 15 2" },
  { expression: "25 + * 2",       label: "25 + * 2" },
  { expression: "(A + B) * C",    label: "(A + B) * C" }
];

/* ==========================================================================
   ██  TEMPORARY UI DEVELOPMENT DATA – MOCK SECTION  ██
   Replace with actual API response.

   - Only active when the page is opened as  index.html?mock=1
   - It is a lookup table of CANNED responses for the six demo expressions.
     It does NOT parse, validate, convert or evaluate anything.
   - The values below were copied from the real C engine's output
     (analyze_expression with A=10, B=20, C=2) so the UI is designed
     around the true response shape.
   - Any other expression returns a clear "mock data only" error.
   - Safe to delete this whole block (and the MOCK_MODE lines above).
   ========================================================================== */
const MockBackend = (function () {
  // Helper that only builds a step object; it computes nothing.
  const step = (n, ops, stack, note) => ({ step: n, operations: ops, stack: stack, note: note || "" });

  const stepsInfix1 = [
    step(1, ["PUSH 25"], [25]),
    step(2, ["PUSH 15"], [25, 15]),
    step(3, ["POP 15", "POP 25", "APPLY 25 + 15 = 40", "PUSH 40"], [40]),
    step(4, ["PUSH 2"], [40, 2]),
    step(5, ["POP 2", "POP 40", "APPLY 40 * 2 = 80", "PUSH 80"], [80])
  ];
  const stepsPrefix1 = [
    step(1, ["PUSH 2"], [2], "Prefix expressions are scanned right to left."),
    step(2, ["PUSH 15"], [2, 15]),
    step(3, ["PUSH 25"], [2, 15, 25]),
    step(4, ["POP 25", "POP 15", "APPLY 25 + 15 = 40", "PUSH 40"], [2, 40]),
    step(5, ["POP 40", "POP 2", "APPLY 40 * 2 = 80", "PUSH 80"], [80])
  ];
  const stepsABC = [
    step(1, ["PUSH A (= 10)"], [10]),
    step(2, ["PUSH B (= 20)"], [10, 20]),
    step(3, ["PUSH C (= 2)"], [10, 20, 2]),
    step(4, ["POP 2", "POP 20", "APPLY 20 * 2 = 40", "PUSH 40"], [10, 40]),
    step(5, ["POP 40", "POP 10", "APPLY 10 + 40 = 50", "PUSH 50"], [50])
  ];
  const stepsAB_C = [
    step(1, ["PUSH A (= 10)"], [10]),
    step(2, ["PUSH B (= 20)"], [10, 20]),
    step(3, ["POP 20", "POP 10", "APPLY 10 + 20 = 30", "PUSH 30"], [30]),
    step(4, ["PUSH C (= 2)"], [30, 2]),
    step(5, ["POP 2", "POP 30", "APPLY 30 * 2 = 60", "PUSH 60"], [60])
  ];

  const base = { success: true, valid: true, diagnosis: null, corrections: [] };

  // key = expression with whitespace collapsed
  const canned = {
    "(25 + 15) * 2": { ...base, notation: "INFIX",   infix: "( 25 + 15 ) * 2", postfix: "25 15 + 2 *", prefix: "* + 25 15 2", result: 80, steps: stepsInfix1 },
    "25 15 + 2 *":   { ...base, notation: "POSTFIX", infix: "(25 + 15) * 2",   postfix: "25 15 + 2 *", prefix: "* + 25 15 2", result: 80, steps: stepsInfix1 },
    "* + 25 15 2":   { ...base, notation: "PREFIX",  infix: "(25 + 15) * 2",   postfix: "25 15 + 2 *", prefix: "* + 25 15 2", result: 80, steps: stepsPrefix1 },
    "25 + * 2": {
      success: true, valid: false, notation: "INVALID",
      diagnosis: { message: "Two operators appear consecutively.", position: 5, expected: "Operand or '('", found: "*" },
      corrections: [
        { expression: "25 + 2", reason: "Removed the second of two consecutive operators." },
        { expression: "25 * 2", reason: "Removed the first of two consecutive operators." }
      ],
      steps: []
    }
  };
  const needsVars = {
    "A + B * C":   { ...base, notation: "INFIX", infix: "A + B * C",     postfix: "A B C * +", prefix: "+ A * B C", variables_required: ["A", "B", "C"], result: null, steps: [] },
    "(A + B) * C": { ...base, notation: "INFIX", infix: "( A + B ) * C", postfix: "A B + C *", prefix: "* + A B C", variables_required: ["A", "B", "C"], result: null, steps: [] }
  };
  const withVars = {
    "A + B * C":   { ...needsVars["A + B * C"],   variables_required: [], result: 50, steps: stepsABC },
    "(A + B) * C": { ...needsVars["(A + B) * C"], variables_required: [], result: 60, steps: stepsAB_C }
  };

  return {
    analyze(expression, variables) {
      const key = expression.trim().replace(/\s+/g, " ");
      let raw;
      if (canned[key]) {
        raw = canned[key];
      } else if (needsVars[key]) {
        const v = variables || {};
        const matches = v.A === 10 && v.B === 20 && v.C === 2;
        raw = !variables ? needsVars[key]
            : matches    ? withVars[key]
            : { success: false, error: "Mock data only covers A=10, B=20, C=2. Start the Flask backend for other values." };
      } else {
        raw = { success: false, error: "Mock mode only knows the six built-in examples. Start the Flask backend to analyze other expressions." };
      }
      return new Promise((resolve) => setTimeout(() => resolve(normalizeAnalysis(raw, expression)), 450));
    }
  };
})();
