/* main.c - interactive Expression Validator & Evaluator */
#include <stdio.h>
#include <string.h>
#include "tokenizer/tokenizer.h"
#include "validation/bracket_validator.h"
#include "validation/syntax_validator.h"
#include "notation/notation_detector.h"
#include "notation/conversion.h"

static void analyse(const char *line)
{
    Token t[MAX_TOKENS];
    char out[1024], err[128];
    double value;
    int n = tokenize(line, t, MAX_TOKENS);
    NotationType type;

    if (n < 0) { printf("  Error: expression too long.\n"); return; }
    if (n == 0) { printf("  Error: empty expression.\n"); return; }

    type = detect_notation(t, n);
    printf("  Notation : %s\n", notation_name(type));

    if (type == NOTATION_INVALID) {
        ValidationResult r = validate_brackets(t, n);
        if (r.valid) r = validate_syntax(t, n);
        printf("  Problem  : %s", r.message);
        if (r.position >= 0) printf(" (at character %d)", r.position);
        printf("\n");
        return;
    }
    if (type == NOTATION_INFIX) {
        if (infix_to_postfix(t, n, out, sizeof out) == CONV_OK) printf("  Postfix  : %s\n", out);
        if (infix_to_prefix(t, n, out, sizeof out) == CONV_OK)  printf("  Prefix   : %s\n", out);
    } else if (type == NOTATION_POSTFIX) {
        if (postfix_to_infix(t, n, out, sizeof out) == CONV_OK) printf("  Infix    : %s\n", out);
    } else if (type == NOTATION_PREFIX) {
        if (prefix_to_infix(t, n, out, sizeof out) == CONV_OK)  printf("  Infix    : %s\n", out);
    }
    if (evaluate_expression(t, n, type, &value, err, sizeof err) == CONV_OK) {
        printf("  Value    : %g\n", value);
    } else {
        printf("  Value    : (not evaluated: %s)\n", err);
    }
}

int main(void)
{
    char line[1024];
    printf("Expression Validator & Evaluator  (type 'quit' to exit)\n");
    for (;;) {
        printf("\n> ");
        fflush(stdout);
        if (fgets(line, sizeof line, stdin) == NULL) break;
        line[strcspn(line, "\r\n")] = '\0';
        if (strcmp(line, "quit") == 0 || strcmp(line, "exit") == 0) break;
        analyse(line);
    }
    return 0;
}
