/* bracket_validator.c - stack-based bracket matching */
#include <stdio.h>
#include <stdlib.h>
#include "bracket_validator.h"
#include "../stack/stack.h"

static char matching_open(char close)
{
    switch (close) {
        case ')': return '(';
        case ']': return '[';
        default:  return '{';
    }
}

static ValidationResult make_result(int valid, int position, const char *msg)
{
    ValidationResult r;
    r.valid = valid;
    r.position = position;
    snprintf(r.message, sizeof r.message, "%s", msg);
    return r;
}

ValidationResult validate_brackets(const Token tokens[], int count)
{
    Stack s;
    int *open_pos;
    int i;
    char top;
    ValidationResult result;

    if (tokens == NULL || count < 0) {
        return make_result(0, -1, "No tokens supplied");
    }
    if (count == 0) {
        return make_result(1, -1, "Brackets are balanced");
    }
    if (stack_init(&s, count) != STACK_OK) {
        return make_result(0, -1, "Out of memory");
    }
    open_pos = (int *)malloc((size_t)count * sizeof(int));
    if (open_pos == NULL) {
        stack_free(&s);
        return make_result(0, -1, "Out of memory");
    }

    result = make_result(1, -1, "Brackets are balanced");

    for (i = 0; i < count; i++) {
        if (tokens[i].type == TOKEN_LPAREN) {
            open_pos[s.top + 1] = tokens[i].position;
            stack_push(&s, tokens[i].text[0]);
        } else if (tokens[i].type == TOKEN_RPAREN) {
            if (stack_peek(&s, &top) != STACK_OK) {
                result = make_result(0, tokens[i].position,
                                     "Closing bracket has no matching opening bracket");
                break;
            }
            if (top != matching_open(tokens[i].text[0])) {
                result = make_result(0, tokens[i].position,
                                     "Mismatched bracket type");
                break;
            }
            stack_pop(&s, NULL);
        }
    }

    if (result.valid && !stack_is_empty(&s)) {
        result = make_result(0, open_pos[s.top], "Opening bracket is never closed");
    }

    free(open_pos);
    stack_free(&s);
    return result;
}
