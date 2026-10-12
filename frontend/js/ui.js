/* ==========================================================================
   ui.js – everything that changes what is on screen.

   It renders the states of the result panel (initial, loading, valid,
   invalid, variables needed, backend error, backend unavailable) and the
   small pieces of page furniture (status pill, examples, messages).
   It never calls the backend and never calculates anything.

   All text from the backend is inserted with textContent (via UI.el), never
   as HTML, so an expression such as "<b>" cannot break or inject into the page.

   Public object:  UI
   ========================================================================== */
const UI = (function () {
  let dom = {};          // cached elements
  let handlers = {};     // callbacks supplied by app.js

  /* ---------- tiny DOM helpers (shared with visualization.js) ---------- */

  function appendChildren(node, children) {
    children.forEach((child) => {
      if (child === null || child === undefined || child === false) return;
      if (Array.isArray(child)) appendChildren(node, child);
      else node.append(child);
    });
  }

  /* el("div", { class: "x", text: "hi", on: { click: fn } }, child1, child2) */
  function el(tag, opts, ...children) {
    const node = document.createElement(tag);
    Object.entries(opts || {}).forEach(([key, value]) => {
      if (value === null || value === undefined || value === false) return;
      if (key === "class") node.className = value;
      else if (key === "text") node.textContent = value;
      else if (key === "on") Object.entries(value).forEach(([evt, fn]) => node.addEventListener(evt, fn));
      else node.setAttribute(key, value === true ? "" : value);
    });
    appendChildren(node, children);
    return node;
  }

  /* Display formatting only (removes float noise such as 0.30000000000000004). */
  function formatNumber(value) {
    if (typeof value === "number" && Number.isFinite(value)) return String(Number(value.toPrecision(12)));
    return String(value);
  }

  /* ---------- setup ---------- */

  function init(callbacks) {
    handlers = callbacks || {};
    dom = {
      input:     document.getElementById("expression-input"),
      message:   document.getElementById("input-message"),
      analyzeBtn: document.getElementById("analyze-btn"),
      results:   document.getElementById("result-container"),
      stack:     document.getElementById("stack-container"),
      status:    document.getElementById("backend-status"),
      examples:  document.getElementById("example-select")
    };
    dom.status.addEventListener("click", () => handlers.onRecheckBackend && handlers.onRecheckBackend());
    renderInitial();
    renderStack(null, "initial");
  }

  /* ---------- small page pieces ---------- */

  function setBackendStatus(state) {
    const labels = {
      checking: "Checking backend…",
      online:   "Backend connected",
      offline:  "Backend offline",
      mock:     "Demo data (mock)"
    };
    dom.status.dataset.state = state;
    dom.status.querySelector(".backend-pill-text").textContent = labels[state] || labels.checking;
  }

  function populateExamples(examples) {
    dom.examples.replaceChildren(el("option", { value: "", text: "Try Example" }));
    examples.forEach((e) => dom.examples.append(el("option", { value: e.expression, text: e.label })));
  }

  /* Optional: let the backend override the text of the three notation cards. */
  function applyNotationInfo(list) {
    if (!list) return;
    list.forEach((n) => {
      const card = document.querySelector('[data-notation-card="' + n.name + '"]');
      if (!card) return;
      const example = card.querySelector('[data-field="example"]');
      const description = card.querySelector('[data-field="description"]');
      if (example && n.example) example.textContent = n.example;
      if (description && n.description) description.textContent = n.description;
    });
  }

  function highlightNotation(name) {
    const card = document.querySelector('[data-notation-card="' + name + '"]');
    if (!card) return;
    card.classList.remove("is-highlighted");
    void card.offsetWidth;                       // restart the CSS animation
    card.classList.add("is-highlighted");
    setTimeout(() => card.classList.remove("is-highlighted"), 1800);
  }

  function showInputMessage(text) {
    dom.message.textContent = text;
    dom.message.hidden = false;
    dom.input.setAttribute("aria-invalid", "true");
  }

  function clearInputMessage() {
    dom.message.hidden = true;
    dom.message.textContent = "";
    dom.input.removeAttribute("aria-invalid");
  }

  function setLoading(isLoading) {
    dom.analyzeBtn.disabled = isLoading;
    dom.analyzeBtn.classList.toggle("is-loading", isLoading);
    dom.analyzeBtn.querySelector(".btn-label").textContent = isLoading ? "Analyzing…" : "Analyze";
    dom.results.setAttribute("aria-busy", String(isLoading));
  }

  /* Replaces the content of the result panel. */
  function setResult(...nodes) {
    dom.results.replaceChildren(...nodes);
  }

  /* ---------- result states ---------- */

  function mockBanner() {
    return Api.mockMode
      ? el("div", { class: "mock-banner", text: "Demo mode: showing canned sample responses, not live engine output." })
      : null;
  }

  function renderInitial() {
    setResult(el("div", { class: "empty-state" },
      el("strong", { text: "No analysis yet" }),
      el("p", { text: "Type an expression, use the keypad, or pick an example, then press Analyze. Infix, prefix and postfix are all accepted." }),
      el("p", { class: "fineprint", text: "Variables such as A, B and C will be requested after the expression is checked." }),
      mockBanner()
    ));
  }

  function renderLoading() {
    setResult(el("div", { class: "skeleton", role: "status", "aria-label": "Analyzing expression" },
      el("div"), el("div"), el("div")));
  }

  function statusBanner(kind, title, ...extra) {
    return el("div", { class: "status-banner " + kind },
      el("span", { class: "status-title", text: title }), ...extra);
  }

  function notationTag(name) {
    return el("div", { class: "notation-tag" }, "Detected Notation", el("b", { text: name }));
  }

  function resultCard(label, value, extraClass) {
    return el("div", { class: "result-card " + (extraClass || "") },
      el("div", { class: "card-label", text: label }),
      el("div", { class: "card-value", text: value }));
  }

  /* Decides which screen to show for an analysis returned by the backend. */
  function renderAnalysis(a, context) {
    if (!a.success) {
      renderBackendError({ message: a.error || "The backend reported a failure.", details: "" });
    } else if (!a.valid) {
      renderInvalid(a);
    } else {
      renderValid(a, context || {});
    }
  }

  /* ----- valid expression ----- */
  function renderValid(a, context) {
    const isAmbiguous = a.notation === "AMBIGUOUS";
    const needsVariables = a.variablesRequired.length > 0 && a.result === null && !isAmbiguous;
    const nodes = [];

    nodes.push(mockBanner());
    nodes.push(statusBanner("valid", "✓ VALID EXPRESSION", notationTag(a.notation)));
    nodes.push(el("div", { class: "echo-label", text: "Expression" }));
    nodes.push(el("pre", { class: "echo", text: a.expression }));

    // Conversion cards: only those the backend actually sent.
    const grid = el("div", { class: "result-grid" });
    if (a.postfix) grid.append(resultCard("Postfix", a.postfix));
    if (a.prefix)  grid.append(resultCard("Prefix", a.prefix));
    if (a.infix && a.notation !== "INFIX") grid.append(resultCard("Infix (fully bracketed)", a.infix, "wide"));

    // Result card: value, waiting for variables, evaluation error, or not evaluated.
    if (a.result !== null) {
      grid.append(resultCard("Result", formatNumber(a.result), "wide answer"));
    } else if (needsVariables) {
      grid.append(resultCard("Result", "Enter values for the variables below to evaluate.", "wide answer pending"));
    } else if (a.diagnosis && !isAmbiguous) {
      grid.append(resultCard("Result — could not be evaluated", a.diagnosis.message, "wide answer failed"));
    } else {
      grid.append(resultCard("Result", isAmbiguous ? "Not evaluated: the notation is ambiguous." : "No result was returned.", "wide answer pending"));
    }
    nodes.push(grid);

    // Ambiguous notation: show the engine's explanation.
    if (isAmbiguous && a.diagnosis) {
      nodes.push(el("div", { class: "diagnosis", style: "border-left-color: var(--warn)" },
        el("h3", { text: "Note" }),
        el("p", { class: "diag-message", text: a.diagnosis.message })));
    }

    if (needsVariables) nodes.push(buildVariablesCard(a, context));

    setResult(...nodes);

    if (needsVariables) {
      const fields = Array.from(dom.results.querySelectorAll(".var-field input"));
      (fields.find((f) => f.value === "") || fields[0])?.focus();
    }
  }

  /* ----- variables card ----- */
  function buildVariablesCard(a, context) {
    const previous = context.submittedVariables || {};
    const errorLine = el("p", { class: "var-error", role: "alert", hidden: true });
    // Backend asked again after values were sent (e.g. one was still missing).
    if (context.submittedVariables && a.diagnosis) {
      errorLine.textContent = a.diagnosis.message;
      errorLine.hidden = false;
    }

    const inputs = a.variablesRequired.map((name) => {
      const id = "var-" + name;
      const input = el("input", {
        id: id, type: "text", inputmode: "decimal", autocomplete: "off", spellcheck: "false",
        "data-var": name, placeholder: "value",
        value: previous[name] !== undefined ? String(previous[name]) : ""
      });
      // setAttribute("value") above only sets the default; keep property in sync too.
      if (previous[name] !== undefined) input.value = String(previous[name]);
      return { name: name, input: input,
               field: el("div", { class: "var-field" }, el("label", { for: id, text: name }), input) };
    });

    const form = el("form", { class: "variables", novalidate: true, "aria-label": "Variable values" },
      el("h3", { text: "Variables detected" }),
      el("p", { class: "var-note", text: "This expression uses variables. Enter a number for each; the backend does the evaluation." }),
      errorLine,
      el("div", { class: "var-grid" }, inputs.map((i) => i.field)),
      el("button", { type: "submit", class: "btn-primary" }, el("span", { class: "btn-label", text: "Evaluate Expression" }))
    );

    form.addEventListener("submit", (event) => {
      event.preventDefault();
      const values = {};
      let firstBad = null;
      inputs.forEach(({ name, input }) => {
        const text = input.value.trim();
        const number = Number(text);
        const bad = text === "" || !Number.isFinite(number);   // input check only, no math
        if (bad) input.setAttribute("aria-invalid", "true"); else input.removeAttribute("aria-invalid");
        if (bad && !firstBad) firstBad = input;
        else if (!bad) values[name] = number;
      });
      if (firstBad) {
        errorLine.textContent = "Enter a valid number for every variable.";
        errorLine.hidden = false;
        firstBad.focus();
        return;
      }
      errorLine.hidden = true;
      handlers.onSubmitVariables && handlers.onSubmitVariables(values);
    });

    return form;
  }

  /* ----- invalid expression ----- */
  function pointerBlock(expression, position) {
    const pos = Math.min(position, expression.length);
    const badChar = expression.charAt(pos) || " ";
    return el("pre", { class: "pointer", "aria-hidden": "true" },
      el("span", {}, expression.slice(0, pos), el("span", { class: "err-char", text: badChar }), expression.slice(pos + 1)),
      "\n",
      el("span", { class: "caret", text: " ".repeat(pos) + "↑" }));
  }

  function renderInvalid(a) {
    const d = a.diagnosis;
    const nodes = [mockBanner(), statusBanner("invalid", "✕ INVALID EXPRESSION")];

    const diagnosisCard = el("section", { class: "diagnosis", "aria-label": "Error diagnosis" },
      el("h3", { text: "Error Diagnosis" }),
      el("p", { class: "diag-message", text: d ? d.message : "The backend did not provide a diagnosis." }));

    if (d) {
      const meta = el("div", { class: "diag-meta" });
      if (d.position !== null) meta.append(el("span", {}, "Position: ", el("code", { text: String(d.position) })));
      if (d.found)    meta.append(el("span", {}, "Found: ", el("code", { text: d.found })));
      if (d.expected) meta.append(el("span", {}, "Expected: ", el("code", { text: d.expected })));
      if (meta.children.length) diagnosisCard.append(meta);
      if (d.position !== null) diagnosisCard.append(pointerBlock(a.expression, d.position));
    }
    if (!d || d.position === null) diagnosisCard.append(el("pre", { class: "echo", text: a.expression }));
    nodes.push(diagnosisCard);

    // Corrections: shown exactly as the backend sent them.
    const section = el("section", { class: "corrections", "aria-label": "Possible corrections" },
      el("h3", { text: "Possible Corrections" }));
    if (a.corrections.length === 0) {
      section.append(el("p", { class: "none", text: "The engine found no safe automatic correction for this expression." }));
    } else {
      section.append(el("ul", { class: "correction-list" }, a.corrections.map((c) =>
        el("li", { class: "correction" },
          el("div", {},
            el("code", { text: c.expression }),
            c.reason ? el("div", { class: "reason", text: c.reason }) : null),
          el("button", { type: "button", class: "btn-secondary", "aria-label": "Use correction " + c.expression,
                          on: { click: () => handlers.onUseCorrection && handlers.onUseCorrection(c.expression) } },
            "Use correction")))));
    }
    nodes.push(section);
    setResult(...nodes);
  }

  /* ----- backend problems ----- */
  function retryButton() {
    return el("button", { type: "button", class: "btn-secondary",
                           on: { click: () => handlers.onRetry && handlers.onRetry() } }, "Try again");
  }

  function renderBackendUnavailable(error) {
    setResult(el("div", { class: "error-state" },
      statusBanner("notice", "Backend connection unavailable"),
      el("p", { text: "Make sure the Flask server is running." }),
      el("p", {}, "This page looks for it at ", el("code", { text: Api.baseUrl }),
        ". If it is running, check that Flask allows requests from this page (CORS)."),
      error && error.details ? el("p", {}, "Browser said: ", el("code", { text: error.details })) : null,
      el("p", {}, "To preview the interface without a backend, open ", el("code", { text: "index.html?mock=1" }), "."),
      retryButton()));
  }

  function renderBackendError(error) {
    setResult(el("div", { class: "error-state" },
      statusBanner("invalid", "✕ BACKEND ERROR"),
      el("p", { text: error.message || "The backend returned an error." }),
      error.details ? el("p", {}, el("code", { text: error.details })) : null,
      retryButton()));
  }

  /* ---------- stack section ---------- */

  /* analysis === null shows a message instead of a stack. */
  function renderStack(a, reason) {
    if (!a) {
      Visualization.showMessage(dom.stack, reason === "initial"
        ? "Analyze an expression to see its stack operations here."
        : "Stack execution details are not available for this analysis.");
      return;
    }
    if (!a.valid || a.steps.length === 0) {
      Visualization.showMessage(dom.stack, "Stack execution details are not available for this analysis.");
      return;
    }
    Visualization.render(dom.stack, a.steps, { result: a.result });
  }

  return {
    el: el, formatNumber: formatNumber, init: init,
    setBackendStatus: setBackendStatus, populateExamples: populateExamples,
    applyNotationInfo: applyNotationInfo, highlightNotation: highlightNotation,
    showInputMessage: showInputMessage, clearInputMessage: clearInputMessage,
    setLoading: setLoading, renderInitial: renderInitial, renderLoading: renderLoading,
    renderAnalysis: renderAnalysis, renderBackendUnavailable: renderBackendUnavailable,
    renderBackendError: renderBackendError, renderStack: renderStack
  };
})();
