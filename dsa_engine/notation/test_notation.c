/* test_notation.c - tests for notation detection, conversion and evaluation */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../tokenizer/tokenizer.h"
#include "notation_detector.h"
#include "conversion.h"

static int failures = 0;

static void check(const char *label, int condition)
{
    if (!condition) failures++;
    printf("  [%s] %s\n", condition ? "PASS" : "FAIL", label);
}

static NotationType detect(const char *expr)
{
    Token t[MAX_TOKENS];
    int n = tokenize(expr, t, MAX_TOKENS);
    return detect_notation(t, n);
}

static int convert_is(int (*fn)(const Token[], int, char *, int),
                      const char *expr, const char *expected)
{
    Token t[MAX_TOKENS];
    char out[256];
    int n = tokenize(expr, t, MAX_TOKENS);
    if (fn(t, n, out, sizeof out) != CONV_OK) return 0;
    return strcmp(out, expected) == 0;
}

static int eval_is(const char *expr, double expected)
{
    Token t[MAX_TOKENS];
    char err[80];
    double v;
    int n = tokenize(expr, t, MAX_TOKENS);
    NotationType type = detect_notation(t, n);
    if (evaluate_expression(t, n, type, &v, err, sizeof err) != CONV_OK) return 0;
    return fabs(v - expected) < 1e-9;
}

static int eval_fails(const char *expr)
{
    Token t[MAX_TOKENS];
    char err[80];
    double v;
    int n = tokenize(expr, t, MAX_TOKENS);
    NotationType type = detect_notation(t, n);
    return evaluate_expression(t, n, type, &v, err, sizeof err) == CONV_ERROR;
}

int main(void)
{
    printf("=== NOTATION TESTS ===\n\n");

    printf("TEST 1: Detection\n");
    check("A+B*C        -> INFIX",   detect("A+B*C") == NOTATION_INFIX);
    check("(A+B)*C      -> INFIX",   detect("(A+B)*C") == NOTATION_INFIX);
    check("+AB          -> PREFIX",  detect("+AB") == NOTATION_PREFIX);
    check("*+AB-CD      -> PREFIX",  detect("*+AB-CD") == NOTATION_PREFIX);
    check("AB+          -> POSTFIX", detect("AB+") == NOTATION_POSTFIX);
    check("AB+CD-*      -> POSTFIX", detect("AB+CD-*") == NOTATION_POSTFIX);
    check("A            -> AMBIGUOUS", detect("A") == NOTATION_AMBIGUOUS);
    check("25           -> AMBIGUOUS", detect("25") == NOTATION_AMBIGUOUS);
    check("A+           -> INVALID", detect("A+") == NOTATION_INVALID);
    check("(A+B         -> INVALID", detect("(A+B") == NOTATION_INVALID);
    check("empty        -> INVALID", detect("") == NOTATION_INVALID);
    check("notation_name works", strcmp(notation_name(NOTATION_POSTFIX), "POSTFIX") == 0);
    printf("\n");

    printf("TEST 2: Infix -> Postfix\n");
    check("A+B*C",     convert_is(infix_to_postfix, "A+B*C", "A B C * +"));
    check("(A+B)*C",   convert_is(infix_to_postfix, "(A+B)*C", "A B + C *"));
    check("A-B-C",     convert_is(infix_to_postfix, "A-B-C", "A B - C -"));
    check("A^B^C",     convert_is(infix_to_postfix, "A^B^C", "A B C ^ ^"));
    check("(A+B)*(C-D)", convert_is(infix_to_postfix, "(A+B)*(C-D)", "A B + C D - *"));
    printf("\n");

    printf("TEST 3: Infix -> Prefix\n");
    check("A+B*C",     convert_is(infix_to_prefix, "A+B*C", "+ A * B C"));
    check("(A+B)*C",   convert_is(infix_to_prefix, "(A+B)*C", "* + A B C"));
    check("A-B-C",     convert_is(infix_to_prefix, "A-B-C", "- - A B C"));
    check("A^B^C",     convert_is(infix_to_prefix, "A^B^C", "^ A ^ B C"));
    printf("\n");

    printf("TEST 4: Postfix / Prefix -> Infix\n");
    check("AB+C*",  convert_is(postfix_to_infix, "AB+C*", "(A + B) * C"));
    check("*+ABC",  convert_is(prefix_to_infix, "*+ABC", "(A + B) * C"));
    check("AB-",    convert_is(postfix_to_infix, "AB-", "A - B"));
    check("-AB",    convert_is(prefix_to_infix, "-AB", "A - B"));
    printf("\n");

    printf("TEST 5: Evaluation\n");
    check("2+3*4        = 14", eval_is("2+3*4", 14));
    check("(2+3)*4      = 20", eval_is("(2+3)*4", 20));
    check("2 3 4 * +    = 14", eval_is("2 3 4 * +", 14));
    check("+ 2 * 3 4    = 14", eval_is("+ 2 * 3 4", 14));
    check("2^3^2        = 512", eval_is("2^3^2", 512));
    check("10-4-3       = 3", eval_is("10-4-3", 3));
    check("7%3          = 1", eval_is("7%3", 1));
    check("42           = 42", eval_is("42", 42));
    check("1/0 fails",       eval_fails("1/0"));
    check("A+2 fails (variable)", eval_fails("A+2"));
    check("2+ fails (invalid)",   eval_fails("2+"));
    printf("\n");

    printf("%s (%d failure(s))\n", failures ? "SOME TESTS FAILED" : "ALL TESTS PASSED", failures);
    return failures ? 1 : 0;
}
