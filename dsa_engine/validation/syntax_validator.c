/* syntax_validator.c - infix grammar check
   Two states:
     expecting an OPERAND : allowed = number, variable, '('
     expecting an OPERATOR: allowed = operator, ')', or end of input        */
#include <stdio.h>
#include "syntax_validator.h"

static ValidationResult make_result(int valid, int position, const char *msg)
{
    ValidationResult r;
    r.valid = valid;
    r.position = position;
    snprintf(r.message, sizeof r.message, "%s", msg);
    return r;
}

ValidationResult validate_syntax(const Token tokens[], int count)
{
    int expect_operand = 1;
    int i;

    if (tokens == NULL || count <= 0) {
        return make_result(0, -1, "Expression is empty");
    }

    for (i = 0; i < count; i++) {
        TokenType t = tokens[i].type;

        if (t == TOKEN_INVALID) {
            return make_result(0, tokens[i].position, "Invalid character or number");
        }

        if (expect_operand) {
            if (t == TOKEN_NUMBER || t == TOKEN_VARIABLE) {
                expect_operand = 0;
            } else if (t == TOKEN_LPAREN) {
                /* stay in "expect operand" */
            } else if (t == TOKEN_OPERATOR) {
                return make_result(0, tokens[i].position,
                                   i == 0 ? "Expression cannot start with an operator"
                                          : "Two operators in a row / operand missing");
            } else {
                return make_result(0, tokens[i].position,
                                   i > 0 && tokens[i - 1].type == TOKEN_LPAREN
                                       ? "Empty brackets"
                                       : "Operand missing before closing bracket");
            }
        } else {
            if (t == TOKEN_OPERATOR) {
                expect_operand = 1;
            } else if (t == TOKEN_RPAREN) {
                /* stay in "expect operator" */
            } else {
                return make_result(0, tokens[i].position,
                                   "Missing operator between operands");
            }
        }
    }

    if (expect_operand) {
        return make_result(0, tokens[count - 1].position,
                           tokens[count - 1].type == TOKEN_OPERATOR
                               ? "Expression cannot end with an operator"
                               : "Expression is incomplete");
    }
    return make_result(1, -1, "Syntax is valid");
}
