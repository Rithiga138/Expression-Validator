/* ==========================================================================
   visualization.js – animated stack view.

   This file ONLY draws steps that the backend supplies. It never creates,
   guesses or fills in stack operations. If a step has no `stack` array, the
   text of that step is shown and the stack is reported as "not provided".

   Expected step shape (all fields optional except that a `stack` is needed
   to draw the picture):
     {
       "step": 3,
       "operations": ["POP 15", "POP 25", "APPLY 25 + 15 = 40", "PUSH 40"],
       "stack": [40],            // contents AFTER this step, bottom -> top
       "note": "optional text"
     }
   Also accepted instead of `operations`: { "action": "PUSH", "value": 25 }
   or a plain text `description` / `label`.

   Uses helpers from ui.js (UI.el, UI.formatNumber) at call time.
   Public object:  Visualization
     Visualization.render(container, steps, context)
     Visualization.showMessage(container, text)
     Visualization.stop()
   ========================================================================== */
const Visualization = (function () {
  const PLAY_INTERVAL_MS = 1100;
  let timer = null;

  function stop() {
    if (timer) { clearInterval(timer); timer = null; }
  }

  /* Convert whatever the backend sent into one predictable shape. */
  function normalizeStep(raw, index) {
    raw = raw || {};
    let operations = [];
    if (Array.isArray(raw.operations)) {
      operations = raw.operations.map(String);
    } else if (raw.action) {
      const detail = raw.value !== undefined ? raw.value : (raw.operator !== undefined ? raw.operator : "");
      operations = [(String(raw.action) + " " + detail).trim()];
    } else if (raw.description || raw.label) {
      operations = [String(raw.description || raw.label)];
    }
    return {
      number: raw.step !== undefined ? raw.step : index + 1,
      operations: operations,
      stack: Array.isArray(raw.stack) ? raw.stack : null,
      note: typeof raw.note === "string" ? raw.note : ""
    };
  }

  function opClass(text) {
    const word = String(text).trim().split(/\s+/)[0].toUpperCase();
    if (word === "PUSH") return "op-push";
    if (word === "POP") return "op-pop";
    return "op-apply";
  }

  function showMessage(container, text) {
    stop();
    container.replaceChildren(UI.el("p", { class: "stack-empty", text: text }));
  }

  /* How many bottom items are unchanged between two stacks.
     Items above that point were popped (old) or pushed (new). */
  function commonPrefix(a, b) {
    let i = 0;
    while (i < a.length && i < b.length && String(a[i]) === String(b[i])) i++;
    return i;
  }

  function render(container, rawSteps, context) {
    stop();
    const steps = rawSteps.map(normalizeStep);
    let current = 0;
    let shownStack = null;      // stack drawn last time (to animate the difference)

    /* ----- left column: controls + step list ----- */
    const counter = UI.el("span", { class: "counter" });
    const prevBtn  = UI.el("button", { type: "button", class: "btn-secondary", text: "‹ Prev" });
    const nextBtn  = UI.el("button", { type: "button", class: "btn-secondary", text: "Next ›" });
    const playBtn  = UI.el("button", { type: "button", class: "btn-secondary", text: "Play" });
    const resetBtn = UI.el("button", { type: "button", class: "btn-secondary", text: "Reset" });
    const controls = UI.el("div", { class: "stack-controls", role: "group", "aria-label": "Step controls" },
      prevBtn, playBtn, nextBtn, resetBtn, counter);

    const itemButtons = steps.map((s, i) => {
      const ops = s.operations.length ? s.operations : ["(no description)"];
      return UI.el("button", { type: "button", class: "step-item", "aria-label": "Go to step " + s.number,
                                on: { click: () => { stop(); updatePlayLabel(); show(i); } } },
        UI.el("span", { class: "step-no", text: String(s.number) }),
        UI.el("span", { class: "step-ops", text: ops.join("  ·  ") }),
        s.note ? UI.el("span", { class: "step-note", text: s.note }) : null
      );
    });
    const list = UI.el("ul", { class: "step-list", "aria-label": "Execution steps" },
      itemButtons.map((b) => UI.el("li", {}, b)));

    /* ----- right column: the stack itself ----- */
    const stageTitle = UI.el("div", { class: "stage-title" });
    const stageOps   = UI.el("div", { class: "stage-ops" });
    const column     = UI.el("div", { class: "stack-column", role: "list", "aria-label": "Stack contents, top first" });
    const stageNote  = UI.el("p", { class: "stage-note" });
    const finalLine  = UI.el("p", { class: "stack-final" });
    const stage = UI.el("div", { class: "stack-stage", "aria-live": "polite" },
      stageTitle, stageOps, column, stageNote, finalLine);

    container.replaceChildren(UI.el("div", { class: "stack-layout" },
      UI.el("div", {}, controls, list), stage));

    /* ----- drawing ----- */
    function drawStack(stack) {
      column.replaceChildren();
      if (stack === null) {
        column.append(UI.el("p", { class: "stack-empty-note", text: "Stack contents were not provided for this step." }));
        shownStack = null;
        return;
      }
      const previous = shownStack || [];
      const common = commonPrefix(previous, stack);

      // Popped items: shown once as fading ghosts above the stack, top-first.
      previous.slice(common).reverse().forEach((value) => {
        column.append(UI.el("div", { class: "stack-cell is-popped", "aria-hidden": "true", text: UI.formatNumber(value) }));
      });

      if (stack.length === 0) {
        column.append(UI.el("p", { class: "stack-empty-note", text: "Stack is empty" }));
      }
      // Current items, top-first. Anything above the common part was just pushed.
      for (let i = stack.length - 1; i >= 0; i--) {
        const isTop = i === stack.length - 1;
        const cell = UI.el("div", { class: "stack-cell" + (isTop ? " is-top" : "") + (i >= common ? " is-new" : ""), role: "listitem" },
          UI.formatNumber(stack[i]),
          isTop ? UI.el("span", { class: "top-tag", text: "← TOP" }) : null
        );
        column.append(cell);
      }
      shownStack = stack.slice();
    }

    function show(index) {
      current = Math.max(0, Math.min(steps.length - 1, index));
      const s = steps[current];

      stageTitle.replaceChildren(UI.el("b", { text: "Step " + s.number }), " of " + steps.length);
      stageOps.replaceChildren(...s.operations.map((op) => UI.el("span", { class: "op-chip " + opClass(op), text: op })));
      drawStack(s.stack);
      stageNote.textContent = s.note;

      const isLast = current === steps.length - 1;
      finalLine.replaceChildren();
      if (isLast && context && context.result !== null && context.result !== undefined) {
        finalLine.append("Final result: ", UI.el("b", { text: UI.formatNumber(context.result) }));
      }

      itemButtons.forEach((b, i) => {
        if (i === current) b.setAttribute("aria-current", "step"); else b.removeAttribute("aria-current");
      });
      counter.textContent = (current + 1) + " / " + steps.length;
      prevBtn.disabled = current === 0;
      nextBtn.disabled = isLast;
      // Keep the active step visible inside the list WITHOUT scrolling the whole page.
      const active = itemButtons[current];
      if (active.offsetTop < list.scrollTop) list.scrollTop = active.offsetTop - 6;
      else if (active.offsetTop + active.offsetHeight > list.scrollTop + list.clientHeight)
        list.scrollTop = active.offsetTop + active.offsetHeight - list.clientHeight + 6;
    }

    function updatePlayLabel() { playBtn.textContent = timer ? "Pause" : "Play"; }

    prevBtn.addEventListener("click", () => { stop(); updatePlayLabel(); show(current - 1); });
    nextBtn.addEventListener("click", () => { stop(); updatePlayLabel(); show(current + 1); });
    resetBtn.addEventListener("click", () => { stop(); updatePlayLabel(); shownStack = null; show(0); });
    playBtn.addEventListener("click", () => {
      if (timer) { stop(); updatePlayLabel(); return; }
      if (current === steps.length - 1) { shownStack = null; show(0); }   // replay from the start
      timer = setInterval(() => {
        if (current >= steps.length - 1) { stop(); updatePlayLabel(); return; }
        show(current + 1);
      }, PLAY_INTERVAL_MS);
      updatePlayLabel();
    });

    show(0);
  }

  return { render: render, showMessage: showMessage, stop: stop };
})();
