/* test_tokenizer.c - tests for the tokenizer */
#include <stdio.h>
#include <string.h>
#include "tokenizer.h"

static int failures = 0;

static void check(const char *label, int condition)
{
    if (!condition) failures++;
    printf("  [%s] %s\n", condition ? "PASS" : "FAIL", label);
}

int main(void)
{
    Token t[MAX_TOKENS];
    int n;

    printf("=== TOKENIZER TESTS ===\n\n");

    printf("TEST 1: Simple infix\n");
    n = tokenize("A + 25", t, MAX_TOKENS);
    check("3 tokens", n == 3);
    check("A is VARIABLE", t[0].type == TOKEN_VARIABLE && strcmp(t[0].text, "A") == 0);
    check("+ is OPERATOR", t[1].type == TOKEN_OPERATOR);
    check("25 is NUMBER", t[2].type == TOKEN_NUMBER && strcmp(t[2].text, "25") == 0);
    check("positions recorded", t[0].position == 0 && t[1].position == 2 && t[2].position == 4);
    printf("\n");

    printf("TEST 2: Brackets and decimals\n");
    n = tokenize("(3.14*[r])", t, MAX_TOKENS);
    check("7 tokens", n == 7);
    check("first is LPAREN", t[0].type == TOKEN_LPAREN);
    check("3.14 is one NUMBER", t[1].type == TOKEN_NUMBER && strcmp(t[1].text, "3.14") == 0);
    check("last is RPAREN", t[6].type == TOKEN_RPAREN);
    printf("\n");

    printf("TEST 3: Whitespace and empty input\n");
    check("empty string gives 0 tokens", tokenize("", t, MAX_TOKENS) == 0);
    check("spaces only gives 0 tokens", tokenize("   \t ", t, MAX_TOKENS) == 0);
    check("no spaces needed: AB+ gives 3", tokenize("AB+", t, MAX_TOKENS) == 3);
    printf("\n");

    printf("TEST 4: Invalid input\n");
    n = tokenize("A $ B", t, MAX_TOKENS);
    check("'$' is INVALID", n == 3 && t[1].type == TOKEN_INVALID);
    n = tokenize("1.2.3", t, MAX_TOKENS);
    check("1.2.3 is INVALID", n == 1 && t[0].type == TOKEN_INVALID);
    n = tokenize(".", t, MAX_TOKENS);
    check("lone '.' is INVALID", n == 1 && t[0].type == TOKEN_INVALID);
    printf("\n");

    printf("TEST 5: Bad arguments and overflow\n");
    check("NULL expression fails", tokenize(NULL, t, MAX_TOKENS) == -1);
    check("too many tokens fails", tokenize("A+B+C", t, 3) == -1);
    printf("\n");

    printf("%s (%d failure(s))\n", failures ? "SOME TESTS FAILED" : "ALL TESTS PASSED", failures);
    return failures ? 1 : 0;
}
