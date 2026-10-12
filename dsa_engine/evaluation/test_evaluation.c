/* test_evaluation.c - tests for the evaluator and the variable manager */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../tokenizer/tokenizer.h"
#include "../notation/notation_detector.h"
#include "variable_manager.h"
#include "evaluator.h"

static int failures = 0;

static void check(const char *label, int condition)
{
    if (!condition) failures++;
    printf("  [%s] %s\n", condition ? "PASS" : "FAIL", label);
}

/* Builds a table from up to 3 (name, value) pairs; name 0 = unused. */
static VariableTable make_vars(char n1, double v1, char n2, double v2,
                               char n3, double v3)
{
    VariableTable t;
    var_table_init(&t);
    if (n1) var_set(&t, n1, v1);
    if (n2) var_set(&t, n2, v2);
    if (n3) var_set(&t, n3, v3);
    return t;
}

/* Tokenizes expr, evaluates it with the function that matches `declared`,
   prints a readable report and checks the outcome.
     expect_status : the EvalStatus we must get
     expect_value  : the number we must get (only used when EVAL_OK)       */
static void run_case(const char *title, const char *expr,
                     NotationType declared, const VariableTable *vars,
                     int show_steps, EvalStatus expect_status,
                     double expect_value)
{
    Token t[MAX_TOKENS];
    EvaluationResult r;
    char names[VAR_TABLE_SIZE];
    int  n, i, nv, ok;

    printf("%s\n", title);
    printf("Expression: %s\n", expr);
    printf("Notation: %s\n", notation_name(declared));

    n = tokenize(expr, t, MAX_TOKENS);
    if (n < 0) {
        check("tokenize failed", 0);
        printf("\n");
        return;
    }

    nv = var_collect(t, n, names, VAR_TABLE_SIZE);
    for (i = 0; i < nv; i++) {
        double v;
        if (var_get(vars, names[i], &v) == VAR_OK) {
            printf("%c = %g\n", names[i], v);
        } else {
            printf("%c = (no value)\n", names[i]);
        }
    }

    if (declared == NOTATION_INFIX)        r = evaluate_infix(t, n, vars, show_steps);
    else if (declared == NOTATION_PREFIX)  r = evaluate_prefix(t, n, vars, show_steps);
    else                                   r = evaluate_postfix(t, n, vars, show_steps);

    if (declared == NOTATION_INFIX && r.postfix[0] != '\0') {
        printf("Converted Postfix: %s\n", r.postfix);
    }
    if (r.success) printf("Result: %.2f\n", r.value);
    else           printf("ERROR: %s\n", r.message);

    ok = (r.status == expect_status) && (r.success == (expect_status == EVAL_OK));
    if (ok && expect_status == EVAL_OK) ok = fabs(r.value - expect_value) < 1e-9;
    /* Batch 3 cross-check: whenever the structure is valid, the detector must
       agree with the notation we claimed. */
    if (ok && expect_status != EVAL_INVALID_EXPRESSION &&
        expect_status != EVAL_ERROR) {
        ok = detect_notation(t, n) == declared;
    }
    check(title, ok);
    printf("\n");
}

int main(void)
{
    VariableTable none, abc, onlyA, xy;
    Token t[MAX_TOKENS];
    EvaluationResult r;
    int n;

    var_table_init(&none);
    abc   = make_vars('A', 10, 'B', 20, 'C', 2);
    onlyA = make_vars('A', 10, 0, 0, 0, 0);
    xy    = make_vars('x', 3, 'y', 4, 0, 0);

    printf("=== EVALUATION TEST ===\n\n");

    /* ---- the 12 required tests ---- */
    run_case("TEST 1: Postfix", "25 15 + 2 *", NOTATION_POSTFIX, &none, 1, EVAL_OK, 80);
    run_case("TEST 2: Prefix", "* + 25 15 2", NOTATION_PREFIX, &none, 1, EVAL_OK, 80);
    run_case("TEST 3: Infix", "(25 + 15) * 2", NOTATION_INFIX, &none, 1, EVAL_OK, 80);
    run_case("TEST 4: Infix with variables", "A + B * C", NOTATION_INFIX, &abc, 0, EVAL_OK, 50);
    run_case("TEST 5: Infix with variables and brackets", "(A + B) * C", NOTATION_INFIX, &abc, 0, EVAL_OK, 60);
    run_case("TEST 6: Postfix division", "10 2 / 3 +", NOTATION_POSTFIX, &none, 0, EVAL_OK, 8);
    run_case("TEST 7: Prefix division", "+ / 10 2 3", NOTATION_PREFIX, &none, 0, EVAL_OK, 8);
    run_case("TEST 8: Decimal", "(25.5 + 14.5) * 2", NOTATION_INFIX, &none, 0, EVAL_OK, 80);
    run_case("TEST 9: Division by zero", "25 0 /", NOTATION_POSTFIX, &none, 1, EVAL_DIVISION_BY_ZERO, 0);
    run_case("TEST 10: Missing variable value", "A B +", NOTATION_POSTFIX, &onlyA, 0, EVAL_MISSING_VARIABLE, 0);
    run_case("TEST 11: Invalid postfix", "25 +", NOTATION_POSTFIX, &none, 0, EVAL_INVALID_EXPRESSION, 0);
    run_case("TEST 12: Invalid prefix", "+ 25", NOTATION_PREFIX, &none, 0, EVAL_INVALID_EXPRESSION, 0);

    /* ---- extra tests ---- */
    printf("--- EXTRA TESTS ---\n\n");

    run_case("TEST 13: Postfix with variables", "A B C * +", NOTATION_POSTFIX, &abc, 0, EVAL_OK, 50);
    run_case("TEST 14: Prefix with variables", "* + A B C", NOTATION_PREFIX, &abc, 0, EVAL_OK, 60);
    run_case("TEST 15: Operand order postfix (10 4 -)", "10 4 -", NOTATION_POSTFIX, &none, 0, EVAL_OK, 6);
    run_case("TEST 16: Operand order prefix (- 10 4)", "- 10 4", NOTATION_PREFIX, &none, 0, EVAL_OK, 6);
    run_case("TEST 17: Prefix division order (/ 20 4)", "/ 20 4", NOTATION_PREFIX, &none, 0, EVAL_OK, 5);
    run_case("TEST 18: Modulo", "25 4 %", NOTATION_POSTFIX, &none, 0, EVAL_OK, 1);
    run_case("TEST 19: Decimal modulo", "5.5 2 %", NOTATION_POSTFIX, &none, 0, EVAL_OK, 1.5);
    run_case("TEST 20: Modulo by zero", "7 0 %", NOTATION_POSTFIX, &none, 0, EVAL_DIVISION_BY_ZERO, 0);
    run_case("TEST 21: Infix division by zero", "25 / 0", NOTATION_INFIX, &none, 0, EVAL_DIVISION_BY_ZERO, 0);
    run_case("TEST 22: Too many operands", "1 2 3 +", NOTATION_POSTFIX, &none, 0, EVAL_INVALID_EXPRESSION, 0);
    run_case("TEST 23: Infix missing variable", "(A + B) * C", NOTATION_INFIX, &onlyA, 0, EVAL_MISSING_VARIABLE, 0);
    run_case("TEST 24: Invalid infix (open bracket)", "(A + B", NOTATION_INFIX, &abc, 0, EVAL_INVALID_EXPRESSION, 0);
    run_case("TEST 25: Lowercase variables", "x * y", NOTATION_INFIX,
             &xy, 0, EVAL_OK, 12);
    run_case("TEST 25b: 'X' is not 'x' (case-sensitive)", "X y +", NOTATION_POSTFIX,
             &xy, 0, EVAL_MISSING_VARIABLE, 0);
    run_case("TEST 26: Power operator (extension)", "2 ^ 3 ^ 2", NOTATION_INFIX, &none, 0, EVAL_OK, 512);
    run_case("TEST 27: Left-to-right minus", "10 - 4 - 3", NOTATION_INFIX, &none, 0, EVAL_OK, 3);

    /* ---- error handling without the helper ---- */
    printf("TEST 28: Special error cases\n");
    r = evaluate_postfix(t, 0, &none, 0);
    check("empty token list -> INVALID_EXPRESSION", r.status == EVAL_INVALID_EXPRESSION);
    r = evaluate_postfix(NULL, 3, &none, 0);
    check("NULL tokens -> INVALID_EXPRESSION", r.status == EVAL_INVALID_EXPRESSION);
    n = tokenize("1 2 +", t, MAX_TOKENS);
    t[2].text[0] = '&';                       /* pretend the operator is '&' */
    r = evaluate_postfix(t, n, &none, 0);
    check("unknown operator '&' -> INVALID_EXPRESSION", r.status == EVAL_INVALID_EXPRESSION);
    n = tokenize("( 1 2 +", t, MAX_TOKENS);
    r = evaluate_postfix(t, n, &none, 0);
    check("bracket inside postfix -> INVALID_EXPRESSION", r.status == EVAL_INVALID_EXPRESSION);
    n = tokenize("1 2 +", t, MAX_TOKENS);
    r = evaluate_postfix(t, n, NULL, 0);
    check("NULL variable table is fine without variables", r.success && r.value == 3.0);
    n = tokenize("A 1 +", t, MAX_TOKENS);
    r = evaluate_postfix(t, n, NULL, 0);
    check("NULL variable table + variable -> MISSING_VARIABLE", r.status == EVAL_MISSING_VARIABLE);
    n = tokenize("9 9 ^ 9 ^ 9 ^ 9 ^", t, MAX_TOKENS);
    r = evaluate_postfix(t, n, &none, 0);
    check("overflow -> EVAL_ERROR", r.status == EVAL_ERROR);
    printf("\n");

    /* ---- automatic notation detection ---- */
    printf("TEST 29: evaluate_tokens (auto-detect)\n");
    n = tokenize("(A + B) * C", t, MAX_TOKENS);
    r = evaluate_tokens(t, n, &abc, 0);
    check("infix   (A + B) * C  = 60", r.success && r.value == 60.0);
    n = tokenize("* + A B C", t, MAX_TOKENS);
    r = evaluate_tokens(t, n, &abc, 0);
    check("prefix  * + A B C    = 60", r.success && r.value == 60.0);
    n = tokenize("A B + C *", t, MAX_TOKENS);
    r = evaluate_tokens(t, n, &abc, 0);
    check("postfix A B + C *    = 60", r.success && r.value == 60.0);
    n = tokenize("42", t, MAX_TOKENS);
    r = evaluate_tokens(t, n, &none, 0);
    check("single operand 42    = 42", r.success && r.value == 42.0);
    n = tokenize("25 +", t, MAX_TOKENS);
    r = evaluate_tokens(t, n, &none, 0);
    check("25 + -> INVALID_EXPRESSION", r.status == EVAL_INVALID_EXPRESSION);
    check("status names", strcmp(eval_status_name(EVAL_DIVISION_BY_ZERO), "DIVISION_BY_ZERO") == 0);
    printf("\n");

    /* ---- variable manager ---- */
    printf("TEST 30: Variable manager\n");
    {
        VariableTable v;
        double x = -1;
        char names[VAR_TABLE_SIZE];

        var_table_init(&v);
        check("new table: A has no value", !var_exists(&v, 'A'));
        check("var_get on empty slot fails", var_get(&v, 'A', &x) == VAR_FAIL);
        check("var_set A = 10.5", var_set(&v, 'A', 10.5) == VAR_OK);
        check("A now exists", var_exists(&v, 'A'));
        check("var_get A = 10.5", var_get(&v, 'A', &x) == VAR_OK && x == 10.5);
        check("overwrite A = 3", var_set(&v, 'A', 3) == VAR_OK &&
              var_get(&v, 'A', &x) == VAR_OK && x == 3.0);
        check("'a' is a different variable", !var_exists(&v, 'a'));
        check("set lowercase x", var_set(&v, 'x', 7) == VAR_OK && var_exists(&v, 'x'));
        check("set 'Z' and 'z' independently",
              var_set(&v, 'Z', 1) == VAR_OK && var_set(&v, 'z', 2) == VAR_OK &&
              var_get(&v, 'Z', &x) == VAR_OK && x == 1.0 &&
              var_get(&v, 'z', &x) == VAR_OK && x == 2.0);
        check("non-letter name rejected", var_set(&v, '1', 5) == VAR_FAIL);
        check("NaN value rejected", var_set(&v, 'B', NAN) == VAR_FAIL);
        check("NULL table is safe", var_set(NULL, 'A', 1) == VAR_FAIL &&
              !var_exists(NULL, 'A'));

        n = tokenize("(B + A) * B - c", t, MAX_TOKENS);
        n = var_collect(t, n, names, VAR_TABLE_SIZE);
        check("var_collect finds distinct variables in order: B A c",
              n == 3 && names[0] == 'B' && names[1] == 'A' && names[2] == 'c');
    }
    printf("\n");

    printf("%s (%d failure(s))\n",
           failures ? "SOME TESTS FAILED" : "ALL TESTS PASSED", failures);
    return failures ? 1 : 0;
}
