/* test_validation.c - tests for bracket and syntax validators */
#include <stdio.h>
#include "../tokenizer/tokenizer.h"
#include "bracket_validator.h"
#include "syntax_validator.h"

static int failures = 0;

static void check(const char *label, int condition)
{
    if (!condition) failures++;
    printf("  [%s] %s\n", condition ? "PASS" : "FAIL", label);
}

static int brackets_ok(const char *expr)
{
    Token t[MAX_TOKENS];
    int n = tokenize(expr, t, MAX_TOKENS);
    return validate_brackets(t, n).valid;
}

static int syntax_ok(const char *expr)
{
    Token t[MAX_TOKENS];
    int n = tokenize(expr, t, MAX_TOKENS);
    return validate_syntax(t, n).valid;
}

int main(void)
{
    Token t[MAX_TOKENS];
    int n;
    ValidationResult r;

    printf("=== VALIDATION TESTS ===\n\n");

    printf("TEST 1: Balanced brackets\n");
    check("(A+B)", brackets_ok("(A+B)"));
    check("[(A+B)*{C-D}]", brackets_ok("[(A+B)*{C-D}]"));
    check("no brackets at all", brackets_ok("A+B"));
    printf("\n");

    printf("TEST 2: Unbalanced brackets\n");
    check("(A+B  is invalid", !brackets_ok("(A+B"));
    check("A+B)  is invalid", !brackets_ok("A+B)"));
    check("(A+B]  is invalid (wrong type)", !brackets_ok("(A+B]"));
    check(")(     is invalid", !brackets_ok(")("));
    n = tokenize("(A+(B*C)", t, MAX_TOKENS);
    r = validate_brackets(t, n);
    check("unclosed '(' reported at position 0", !r.valid && r.position == 0);
    printf("\n");

    printf("TEST 3: Valid syntax\n");
    check("A", syntax_ok("A"));
    check("A+B*C", syntax_ok("A+B*C"));
    check("(A+B)*(C-D)", syntax_ok("(A+B)*(C-D)"));
    check("((3))", syntax_ok("((3))"));
    printf("\n");

    printf("TEST 4: Invalid syntax\n");
    check("A+       (trailing operator)", !syntax_ok("A+"));
    check("+A       (leading operator)", !syntax_ok("+A"));
    check("A++B     (double operator)", !syntax_ok("A++B"));
    check("A B      (missing operator)", !syntax_ok("A B"));
    check("()       (empty brackets)", !syntax_ok("()"));
    check("2(3)     (implicit multiply)", !syntax_ok("2(3)"));
    check("(A+)     (operand missing)", !syntax_ok("(A+)"));
    check("A $ B    (invalid character)", !syntax_ok("A $ B"));
    check("empty input", !syntax_ok(""));
    printf("\n");

    printf("%s (%d failure(s))\n", failures ? "SOME TESTS FAILED" : "ALL TESTS PASSED", failures);
    return failures ? 1 : 0;
}
