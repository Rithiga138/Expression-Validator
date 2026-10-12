/* ==========================================================================
   app.js – coordinates the application.

   Flow:  user clicks Analyze -> read the text -> Api.analyzeExpression()
          -> UI shows loading -> UI renders the backend's answer.

   This file decides WHEN things happen. It contains no expression logic.
   ========================================================================== */
const App = (function () {
  const state = {
    lastExpression: "",      // the text most recently sent to the backend
    lastVariables: null,     // variable values most recently sent (or null)
    requestCounter: 0        // lets us ignore a slow, outdated response
  };
  let inputEl = null;

  /* ---------- analysis ---------- */

  /* variables: optional { A: 10, ... } */
  async function analyze(expression, variables) {
    // Empty check is a UI concern; the backend is not contacted for blank input.
    if (expression.trim() === "") {
      UI.showInputMessage("Enter an expression before analyzing.");
      inputEl.focus();
      return;
    }
    UI.clearInputMessage();

    state.lastExpression = expression;
    state.lastVariables = variables || null;
    const requestId = ++state.requestCounter;

    UI.setLoading(true);
    UI.renderLoading();

    try {
      const analysis = await Api.analyzeExpression(expression, variables);
      if (requestId !== state.requestCounter) return;           // a newer request replaced this one
      UI.setBackendStatus(Api.mockMode ? "mock" : "online");
      document.querySelector(".result-panel").hidden = false;
      UI.renderAnalysis(analysis, { submittedVariables: variables || null });
      UI.renderStack(analysis);
      document.getElementById("stack-section").hidden = false;
      document.getElementById("notations").hidden = false;
    } catch (error) {
      if (requestId !== state.requestCounter) return;
      document.querySelector(".result-panel").hidden = false;
      UI.renderStack(null);
      document.getElementById("stack-section").hidden = false;
      if (error instanceof ApiError && error.kind === "unavailable") {
        UI.setBackendStatus("offline");
        UI.renderBackendUnavailable(error);
      } else if (error instanceof ApiError) {
        UI.renderBackendError({ message: error.message, details: error.details });
      } else {
        // Unexpected bug: show it in the panel instead of crashing silently.
        console.error(error);
        UI.renderBackendError({ message: "Something went wrong while showing the result.", details: String(error && error.message || error) });
      }
    } finally {
      if (requestId === state.requestCounter) UI.setLoading(false);
    }
  }

  /* ---------- callbacks given to the UI ---------- */

  function useCorrection(text) {
    Calculator.setValue(text);
    analyze(text);
  }

  function submitVariables(values) {
    // Re-analyze the expression that the variable card belongs to.
    analyze(state.lastExpression, values);
  }

  function retry() {
    if (state.lastExpression) analyze(state.lastExpression, state.lastVariables || undefined);
    else checkBackend();
  }

  async function checkBackend() {
    UI.setBackendStatus("checking");
    const health = await Api.checkHealth();
    UI.setBackendStatus(health.mock ? "mock" : (health.online ? "online" : "offline"));
  }

  /* ---------- page wiring ---------- */

  function init() {
    inputEl = document.getElementById("expression-input");

    UI.init({
      onUseCorrection: useCorrection,
      onSubmitVariables: submitVariables,
      onRetry: retry,
      onRecheckBackend: checkBackend
    });
    Calculator.init(inputEl, null);

    // Analyze: the Enter key inside the box also submits this form.
    document.getElementById("analyze-form").addEventListener("submit", (event) => {
      event.preventDefault();
      analyze(inputEl.value);                     // sent exactly as typed (error positions refer to it)
    });

    // Typing clears the "empty expression" message.
    inputEl.addEventListener("input", UI.clearInputMessage);

    // Example picker: fill the box and analyze right away.
    const select = document.getElementById("example-select");
    select.addEventListener("change", () => {
      if (!select.value) return;
      Calculator.setValue(select.value);
      select.value = "";                          // back to the "Try Example" label
    });

    // Header links "Infix / Prefix / Postfix" scroll (native anchor) and flash the matching card.
    document.querySelectorAll(".site-nav a[data-notation]").forEach((link) => {
      link.addEventListener("click", () => UI.highlightNotation(link.dataset.notation));
    });

    // Optional data from the backend (each has a built-in fallback).
    Api.getExamples().then(UI.populateExamples);
    Api.getNotationInfo().then(UI.applyNotationInfo);
    checkBackend();
  }

  document.addEventListener("DOMContentLoaded", init);
  return { analyze: analyze };
})();
