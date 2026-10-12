/* ==========================================================================
   calculator.js – keypad behaviour.

   The keypad ONLY edits the text in the expression box (insert at the caret,
   delete, clear). It never calculates anything; the backend does that.

   Public object:  Calculator
     Calculator.init(inputEl, keypadEl)
     Calculator.setValue(text)   put text in the box and move the caret to the end
     Calculator.getValue()
   ========================================================================== */
const Calculator = (function () {
  let input = null;

  /* Operators are inserted with a space on each side so that typed prefix and
     postfix expressions stay readable ("25 15 +"). The tokenizer ignores
     extra spaces, so this is only for looks. */
  const OPERATORS = ["+", "-", "*", "/", "%", "^"];

  /* Replace the current selection with `text`, then restore the caret. */
  function insertAtCaret(text) {
    const start = input.selectionStart ?? input.value.length;
    const end   = input.selectionEnd   ?? input.value.length;
    const before = input.value.slice(0, start);
    const after  = input.value.slice(end);
    input.value = before + text + after;
    const caret = before.length + text.length;
    input.focus();
    input.setSelectionRange(caret, caret);
    notifyChange();
  }

  function insertOperator(op) {
    const start = input.selectionStart ?? input.value.length;
    const before = input.value.slice(0, start);
    const after  = input.value.slice(input.selectionEnd ?? start);
    const lead  = before === "" || before.endsWith(" ") ? "" : " ";
    const trail = after.startsWith(" ") ? "" : " ";
    insertAtCaret(lead + op + trail);
  }

  function backspace() {
    const start = input.selectionStart ?? input.value.length;
    const end   = input.selectionEnd   ?? input.value.length;
    if (start !== end) {                       // delete the selection
      insertAtCaret("");
      return;
    }
    if (start === 0) { input.focus(); return; }
    input.value = input.value.slice(0, start - 1) + input.value.slice(end);
    input.focus();
    input.setSelectionRange(start - 1, start - 1);
    notifyChange();
  }

  function clearAll() {
    input.value = "";
    input.focus();
    notifyChange();
  }

  /* Tell the rest of the app the text changed (used to hide the "empty" message). */
  function notifyChange() {
    input.dispatchEvent(new Event("input", { bubbles: true }));
  }

  function handleClick(event) {
    const button = event.target.closest("button");
    if (!button) return;

    const action = button.dataset.action;
    const key = button.dataset.key;

    if (action === "clear")          clearAll();
    else if (action === "backspace") backspace();
    else if (action === "space")     insertAtCaret(" ");
    else if (key && OPERATORS.includes(key)) insertOperator(key);
    else if (key)                    insertAtCaret(key);
  }

  return {
    init(inputEl, keypadEl) {
      input = inputEl;
      if (keypadEl) keypadEl.addEventListener("click", handleClick);
    },
    setValue(text) {
      input.value = text;
      input.focus();
      input.setSelectionRange(text.length, text.length);
      notifyChange();
    },
    getValue() {
      return input.value;
    }
  };
})();
