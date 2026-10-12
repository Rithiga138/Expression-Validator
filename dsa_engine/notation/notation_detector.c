/* notation_detector.c - token-based notation detection */
#include <stddef.h>
#include "notation_detector.h"
#include "../validation/bracket_validator.h"   /* validate_brackets (Batch 2) */
#include "../validation/syntax_validator.h"    /* validate_syntax   (Batch 2) */

static int is_operand(TokenType t)
{
    return t == TOKEN_NUMBER || t == TOKEN_VARIABLE;
}

/* INFIX: brackets balance AND token order follows the infix grammar.
   We reuse the Batch 2 validators instead of repeating their logic. */
static int is_infix(const Token tokens[], int count)
{
    ValidationResult r = validate_brackets(tokens, count);
    if (!r.valid) {
        return 0;
    }
    r = validate_syntax(tokens, count);
    return r.valid;
}

/* POSTFIX: scan LEFT to RIGHT.
   n = number of finished sub-expressions available so far.
   operand  -> n grows by 1
   operator -> needs two sub-expressions, gives back one, so n shrinks by 1
   A valid postfix expression ends with exactly one sub-expression. */
static int is_postfix(const Token tokens[], int count)
{
    int n = 0;
    int i;

    for (i = 0; i < count; i++) {
        if (is_operand(tokens[i].type)) {
            n++;
        } else if (tokens[i].type == TOKEN_OPERATOR) {
            if (n < 2) {
                return 0;          /* operator with too few operands before it */
            }
            n--;
        } else {
            return 0;              /* parentheses / invalid tokens not allowed */
        }
    }
    return n == 1;
}

/* PREFIX: the same idea, but scan RIGHT to LEFT, because in prefix an
   operator comes BEFORE its operands. */
static int is_prefix(const Token tokens[], int count)
{
    int n = 0;
    int i;

    for (i = count - 1; i >= 0; i--) {
        if (is_operand(tokens[i].type)) {
            n++;
        } else if (tokens[i].type == TOKEN_OPERATOR) {
            if (n < 2) {
                return 0;
            }
            n--;
        } else {
            return 0;
        }
    }
    return n == 1;
}

/* Why can a sequence be AMBIGUOUS?
   A single operand ("A", "25") is a valid infix expression, a valid prefix
   expression (an operand with no operator) and a valid postfix expression.
   Nothing in the tokens tells us which one the user meant, so we report
   NOTATION_AMBIGUOUS instead of guessing.
   For longer input the notations exclude each other:
     - prefix must START with an operator,
     - postfix must END with an operator,
     - infix never starts with an operator, and only infix allows '(' ')'. */
NotationType detect_notation(const Token tokens[], int count)
{
    int matches = 0;
    NotationType found = NOTATION_INVALID;

    if (tokens == NULL || count <= 0) {
        return NOTATION_INVALID;
    }

    if (is_infix(tokens, count)) {
        matches++;
        found = NOTATION_INFIX;
    }
    if (is_prefix(tokens, count)) {
        matches++;
        found = NOTATION_PREFIX;
    }
    if (is_postfix(tokens, count)) {
        matches++;
        found = NOTATION_POSTFIX;
    }

    if (matches == 0) {
        return NOTATION_INVALID;
    }
    if (matches > 1) {
        return NOTATION_AMBIGUOUS;
    }
    return found;
}

const char *notation_name(NotationType type)
{
    switch (type) {
        case NOTATION_INFIX:     return "INFIX";
        case NOTATION_PREFIX:    return "PREFIX";
        case NOTATION_POSTFIX:   return "POSTFIX";
        case NOTATION_AMBIGUOUS: return "AMBIGUOUS";
        default:                 return "INVALID";
    }
}