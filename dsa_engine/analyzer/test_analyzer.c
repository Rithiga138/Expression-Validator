/* test_analyzer.c - integration tests for the whole pipeline

   Raw string -> tokenizer -> bracket validator -> syntax validator ->
   notation detector -> conversion -> evaluation -> ExpressionAnalysisResult
   (and, for invalid input: -> diagnosis -> correction generator ->
   re-validation of every candidate). */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "expression_analyzer.h"

static int failures = 0;

static void check(const char *label, int condition)
{
    if (!condition) failures++;
    printf("  [%s] %s\n", condition ? "PASS" : "FAIL", label);
}

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

static void print_report(const ExpressionAnalysisResult *r)
{
    int i;
    printf("Expression: %s\n", r->expression);
    printf("Tokenization: %s\n", r->tokenize_ok ? "PASS" : "FAIL");
    if (!r->tokenize_ok) return;
    if (r->token_count == 0) {
        printf("Status: INVALID (%s)\n", r->diagnosis.message);
        return;
    }
    printf("Brackets: %s\n", r->brackets.valid ? "VALID" : "INVALID");
    printf("Notation: %s\n", notation_name(r->notation));
    printf("Status: %s\n", r->valid ? "VALID" : "INVALID");

    if (!r->valid) {
        printf("Diagnosis: %s\n", r->diagnosis.message);
        if (r->diagnosis.position >= 0) {
            printf("  Position: %d  Expected: %s  Found: %s\n",
                   r->diagnosis.position, r->diagnosis.expected, r->diagnosis.found);
        }
        for (i = 0; i < r->correction_count; i++) {
            printf("Possible correction: %s  (%s)\n",
                   r->corrections[i].expression, r->corrections[i].reason);
        }
        return;
    }

    if (r->postfix[0]) printf("Postfix: %s\n", r->postfix);
    if (r->prefix[0])  printf("Prefix: %s\n",  r->prefix);
    if (r->infix[0])   printf("Infix: %s\n",   r->infix);
    if (r->variable_count > 0) {
        printf("Variables:");
        for (i = 0; i < r->variable_count; i++) printf(" %c", r->variables[i]);
        printf("\n");
    }
    if (r->evaluation_attempted) {
        if (r->evaluation.success) printf("Result: %.2f\n", r->evaluation.value);
        else                       printf("Evaluation error: %s\n", r->evaluation.message);
    }
}

int main(void)
{
    VariableTable none, abc, onlyA;
    ExpressionAnalysisResult r;

    var_table_init(&none);
    abc   = make_vars('A', 10, 'B', 20, 'C', 2);
    onlyA = make_vars('A', 10, 0, 0, 0, 0);

    printf("=== EXPRESSION ANALYZER TESTS ===\n\n");

    /* ---- TEST 1 ---- */
    r = analyze_expression("(25 + 15) * 2", &none, 0);
    print_report(&r); printf("\n");
    check("TEST 1: (25+15)*2 valid infix, postfix/prefix/result correct",
          r.valid && r.notation == NOTATION_INFIX &&
          strcmp(r.postfix, "25 15 + 2 *") == 0 &&
          strcmp(r.prefix, "* + 25 15 2") == 0 &&
          r.evaluation.success && fabs(r.evaluation.value - 80.0) < 1e-9);

    /* ---- TEST 2 ---- */
    r = analyze_expression("A + B * C", &none, 0);
    print_report(&r); printf("\n");
    check("TEST 2: A+B*C valid infix with variables A B C",
          r.valid && r.notation == NOTATION_INFIX && r.variable_count == 3 &&
          r.variables[0] == 'A' && r.variables[1] == 'B' && r.variables[2] == 'C' &&
          !r.evaluation.success && r.evaluation.status == EVAL_MISSING_VARIABLE);

    /* ---- TEST 3 ---- */
    r = analyze_expression("A B C * +", &none, 0);
    print_report(&r); printf("\n");
    check("TEST 3: A B C * + valid postfix", r.valid && r.notation == NOTATION_POSTFIX);

    /* ---- TEST 4 ---- */
    r = analyze_expression("* + A B C", &none, 0);
    print_report(&r); printf("\n");
    check("TEST 4: * + A B C valid prefix", r.valid && r.notation == NOTATION_PREFIX);

    /* ---- TEST 5 ---- */
    r = analyze_expression("(25 + 15", &none, 0);
    print_report(&r); printf("\n");
    {
        int found = 0, i;
        for (i = 0; i < r.correction_count; i++) {
            if (strcmp(r.corrections[i].expression, "(25 + 15)") == 0) found = 1;
        }
        check("TEST 5: (25 + 15 -> INVALID, missing ), correction (25 + 15)",
              !r.valid && r.diagnosis.category == DIAG_UNMATCHED_OPENING_PARENTHESIS && found);
    }

    /* ---- TEST 6 ---- */
    r = analyze_expression("25 + * 2", &none, 0);
    print_report(&r); printf("\n");
    check("TEST 6: 25 + * 2 -> INVALID, consecutive operators",
          !r.valid && r.diagnosis.category == DIAG_CONSECUTIVE_OPERATORS &&
          r.diagnosis.position == 5 && strcmp(r.diagnosis.found, "*") == 0);

    /* ---- TEST 7 ---- */
    r = analyze_expression("25 15", &none, 0);
    print_report(&r); printf("\n");
    check("TEST 7: 25 15 -> INVALID under current grammar",
          !r.valid && r.notation == NOTATION_INVALID);

    /* ---- TEST 8 ---- */
    r = analyze_expression("25 / 0", &none, 0);
    print_report(&r); printf("\n");
    check("TEST 8: 25 / 0 -> valid expression, division by zero",
          r.valid && r.evaluation_attempted && !r.evaluation.success &&
          r.evaluation.status == EVAL_DIVISION_BY_ZERO &&
          r.diagnosis.category == DIAG_DIVISION_BY_ZERO);

    /* ---- TEST 9 ---- */
    r = analyze_expression("(A+B)*C", &abc, 0);
    print_report(&r); printf("\n");
    check("TEST 9: (A+B)*C with A=10 B=20 C=2 -> 60",
          r.valid && r.evaluation.success && fabs(r.evaluation.value - 60.0) < 1e-9);

    /* ---- TEST 10 ---- */
    r = analyze_expression("@", &none, 0);
    print_report(&r); printf("\n");
    check("TEST 10: @ -> invalid character",
          !r.valid && r.diagnosis.category == DIAG_INVALID_CHARACTER);

    /* ---- TEST 11 ---- */
    r = analyze_expression("()", &none, 0);
    print_report(&r); printf("\n");
    check("TEST 11: () -> empty parentheses",
          !r.valid && r.diagnosis.category == DIAG_EMPTY_PARENTHESES);

    /* ---- TEST 12 ---- */
    r = analyze_expression("2(A+B)", &abc, 0);
    print_report(&r); printf("\n");
    check("TEST 12: 2(A+B) -> invalid under current grammar", !r.valid);

    /* ---- TEST 13 ---- */
    r = analyze_expression("(A+B)C", &abc, 0);
    print_report(&r); printf("\n");
    check("TEST 13: (A+B)C -> invalid under current grammar", !r.valid);

    /* ---- TEST 14 ---- */
    r = analyze_expression("25 15 + 2 *", &none, 0);
    print_report(&r); printf("\n");
    check("TEST 14: 25 15 + 2 * -> POSTFIX, result 80",
          r.valid && r.notation == NOTATION_POSTFIX &&
          strcmp(r.infix, "(25 + 15) * 2") == 0 &&
          strcmp(r.prefix, "* + 25 15 2") == 0 &&
          r.evaluation.success && fabs(r.evaluation.value - 80.0) < 1e-9);

    /* ---- TEST 15 ---- */
    r = analyze_expression("* + 25 15 2", &none, 0);
    print_report(&r); printf("\n");
    check("TEST 15: * + 25 15 2 -> PREFIX, result 80",
          r.valid && r.notation == NOTATION_PREFIX &&
          strcmp(r.infix, "(25 + 15) * 2") == 0 &&
          strcmp(r.postfix, "25 15 + 2 *") == 0 &&
          r.evaluation.success && fabs(r.evaluation.value - 80.0) < 1e-9);

    /* ---- extra coverage: decimals ---- */
    printf("TEST 16: Decimals\n");
    r = analyze_expression("(25.5 + 14.5) * 2", &none, 0);
    print_report(&r); printf("\n");
    check("TEST 16: decimal infix -> 80",
          r.valid && r.evaluation.success && fabs(r.evaluation.value - 80.0) < 1e-9);

    /* ---- extra coverage: nested parentheses ---- */
    printf("TEST 17: Nested parentheses\n");
    r = analyze_expression("((A + B) * (C - 1))", &abc, 0);
    print_report(&r); printf("\n");
    check("TEST 17: nested parens -> valid, (10+20)*(2-1) = 30",
          r.valid && r.evaluation.success && fabs(r.evaluation.value - 30.0) < 1e-9);

    /* ---- extra coverage: missing variable via analyzer ---- */
    printf("TEST 18: Missing variable\n");
    r = analyze_expression("(A + B) * C", &onlyA, 0);
    print_report(&r); printf("\n");
    check("TEST 18: (A+B)*C with only A known -> MISSING_VARIABLE",
          r.valid && !r.evaluation.success &&
          r.evaluation.status == EVAL_MISSING_VARIABLE &&
          r.diagnosis.category == DIAG_MISSING_VARIABLE);

    /* ---- extra coverage: ambiguous notation ---- */
    printf("TEST 19: Ambiguous notation\n");
    r = analyze_expression("A", &none, 0);
    print_report(&r); printf("\n");
    check("TEST 19: single operand 'A' -> AMBIGUOUS, structurally valid, not auto-evaluated",
          r.valid && r.notation == NOTATION_AMBIGUOUS &&
          r.diagnosis.category == DIAG_AMBIGUOUS_NOTATION && !r.evaluation_attempted);

    r = analyze_expression("25", &none, 0);
    print_report(&r); printf("\n");
    check("TEST 19b: single operand '25' -> AMBIGUOUS too",
          r.valid && r.notation == NOTATION_AMBIGUOUS);

    /* ---- extra coverage: empty expression ---- */
    printf("TEST 20: Empty expression\n");
    r = analyze_expression("", &none, 0);
    print_report(&r); printf("\n");
    check("TEST 20: empty string -> EMPTY_EXPRESSION",
          !r.valid && r.diagnosis.category == DIAG_EMPTY_EXPRESSION);

    /* ---- extra coverage: missing operator correction chain ---- */
    printf("TEST 21: Missing operator, corrected\n");
    r = analyze_expression("25 15", &none, 0);
    print_report(&r); printf("\n");
    {
        int found = 0, i;
        for (i = 0; i < r.correction_count; i++) {
            if (strcmp(r.corrections[i].expression, "25 +15") == 0) found = 1;
        }
        check("TEST 21: '25 15' correction candidate '25 +15' is offered and re-validated",
              found);
    }

    printf("%s (%d failure(s))\n",
           failures ? "SOME TESTS FAILED" : "ALL TESTS PASSED", failures);
    return failures ? 1 : 0;
}
