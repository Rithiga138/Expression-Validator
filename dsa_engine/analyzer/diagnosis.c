/* diagnosis.c - classifies existing validation/evaluation output

   The bracket and syntax validators (Batch 2) already tell us WHAT went
   wrong as a free-text message and a character position. This file's only
   job is to recognise that text and turn it into:
     - a fixed category (useful for a UI to branch on or show an icon for)
     - a short "expected / found" pair (useful for a precise error message)
   It never re-parses the expression itself. */
#include <stdio.h>
#include <string.h>
#include "diagnosis.h"

static Diagnosis make_diag(DiagnosisCategory category, int position,
                           const char *message, const char *expected,
                           const char *found)
{
    Diagnosis d;
    d.category = category;
    d.position = position;
    snprintf(d.message,  sizeof d.message,  "%s", message);
    snprintf(d.expected, sizeof d.expected, "%s", expected  ? expected : "");
    snprintf(d.found,    sizeof d.found,    "%s", found     ? found    : "");
    return d;
}

int diagnosis_find_token(const Token tokens[], int count, int position)
{
    int i;
    if (tokens == NULL || position < 0) return -1;
    for (i = 0; i < count; i++) {
        if (tokens[i].position == position) return i;
    }
    return -1;
}

/* Fills expected/found from the token located at result.position, if any. */
static Diagnosis with_token_info(DiagnosisCategory category, int position,
                                 const char *message, const char *expected,
                                 const Token tokens[], int count)
{
    int idx = diagnosis_find_token(tokens, count, position);
    const char *found = (idx >= 0) ? tokens[idx].text : "";
    return make_diag(category, position, message, expected, found);
}

/* ---- bracket_validator.c messages (Batch 2, verbatim strings) ---- */
static Diagnosis diagnose_bracket_message(const char *msg, int position,
                                          const Token tokens[], int count)
{
    if (strcmp(msg, "Opening bracket is never closed") == 0) {
        return with_token_info(DIAG_UNMATCHED_OPENING_PARENTHESIS, position,
                               "Missing closing parenthesis.",
                               "a matching closing bracket before the end",
                               tokens, count);
    }
    if (strcmp(msg, "Closing bracket has no matching opening bracket") == 0) {
        return with_token_info(DIAG_UNMATCHED_CLOSING_PARENTHESIS, position,
                               "Closing parenthesis has no matching opening parenthesis.",
                               "no closing bracket here", tokens, count);
    }
    if (strcmp(msg, "Mismatched bracket type") == 0) {
        return with_token_info(DIAG_UNMATCHED_CLOSING_PARENTHESIS, position,
                               "Mismatched bracket type (e.g. '(' closed with ']').",
                               "the matching closing bracket type", tokens, count);
    }
    if (strcmp(msg, "No tokens supplied") == 0) {
        return make_diag(DIAG_EMPTY_EXPRESSION, -1, "The expression is empty.", "", "");
    }
    /* "Brackets are balanced" and anything unrecognised: nothing wrong here. */
    return make_diag(DIAG_NONE, -1, msg, "", "");
}

/* ---- syntax_validator.c messages (Batch 2, verbatim strings) ---- */
static Diagnosis diagnose_syntax_message(const char *msg, int position,
                                         const Token tokens[], int count)
{
    if (strcmp(msg, "Expression is empty") == 0) {
        return make_diag(DIAG_EMPTY_EXPRESSION, -1, "The expression is empty.", "", "");
    }
    if (strcmp(msg, "Invalid character or number") == 0) {
        return with_token_info(DIAG_INVALID_CHARACTER, position,
                               "The expression contains an invalid character or a malformed number.",
                               "a valid number, letter, operator or bracket",
                               tokens, count);
    }
    if (strcmp(msg, "Expression cannot start with an operator") == 0) {
        return with_token_info(DIAG_INVALID_OPERATOR_POSITION, position,
                               "The expression cannot start with an operator.",
                               "an operand or '('", tokens, count);
    }
    if (strcmp(msg, "Two operators in a row / operand missing") == 0) {
        return with_token_info(DIAG_CONSECUTIVE_OPERATORS, position,
                               "Two operators appear consecutively.",
                               "Operand or '('", tokens, count);
    }
    if (strcmp(msg, "Empty brackets") == 0) {
        return with_token_info(DIAG_EMPTY_PARENTHESES, position,
                               "Parentheses contain no expression.",
                               "an operand or a sub-expression", tokens, count);
    }
    if (strcmp(msg, "Operand missing before closing bracket") == 0) {
        return with_token_info(DIAG_MISSING_OPERAND, position,
                               "An operand is missing before the closing bracket.",
                               "an operand", tokens, count);
    }
    if (strcmp(msg, "Missing operator between operands") == 0) {
        return with_token_info(DIAG_MISSING_OPERATOR, position,
                               "An operator is missing between two operands.",
                               "an operator or ')'", tokens, count);
    }
    if (strcmp(msg, "Expression cannot end with an operator") == 0) {
        return with_token_info(DIAG_MISSING_OPERAND, position,
                               "The expression cannot end with an operator.",
                               "an operand", tokens, count);
    }
    if (strcmp(msg, "Expression is incomplete") == 0) {
        return with_token_info(DIAG_MISSING_OPERAND, position,
                               "The expression is incomplete.",
                               "an operand", tokens, count);
    }
    /* "Syntax is valid" and anything unrecognised: nothing wrong here. */
    return make_diag(DIAG_NONE, -1, msg, "", "");
}

Diagnosis diagnose_tokens(const Token tokens[], int count,
                          ValidationResult brackets, ValidationResult syntax)
{
    if (count == 0) {
        return make_diag(DIAG_EMPTY_EXPRESSION, -1, "The expression is empty.", "", "");
    }
    if (!brackets.valid) {
        return diagnose_bracket_message(brackets.message, brackets.position,
                                        tokens, count);
    }
    if (!syntax.valid) {
        return diagnose_syntax_message(syntax.message, syntax.position,
                                       tokens, count);
    }
    return make_diag(DIAG_NONE, -1, "The expression is valid.", "", "");
}

Diagnosis diagnose_ambiguous_notation(void)
{
    return make_diag(DIAG_AMBIGUOUS_NOTATION, -1,
        "This token sequence matches more than one notation (for example, "
        "a single operand is valid infix, prefix and postfix at once), so "
        "the notation cannot be uniquely determined.", "", "");
}

Diagnosis diagnose_evaluation(EvalStatus status, const char *eval_message)
{
    const char *msg = eval_message ? eval_message : "";
    switch (status) {
        case EVAL_OK:
            return make_diag(DIAG_NONE, -1, "Evaluation successful.", "", "");
        case EVAL_DIVISION_BY_ZERO:
            return make_diag(DIAG_DIVISION_BY_ZERO, -1, msg, "", "");
        case EVAL_MISSING_VARIABLE:
            return make_diag(DIAG_MISSING_VARIABLE, -1, msg, "", "");
        case EVAL_INVALID_EXPRESSION:
        case EVAL_ERROR:
        default:
            return make_diag(DIAG_EVALUATION_ERROR, -1, msg, "", "");
    }
}

const char *diagnosis_category_name(DiagnosisCategory category)
{
    switch (category) {
        case DIAG_NONE:                          return "NONE";
        case DIAG_EMPTY_EXPRESSION:               return "EMPTY_EXPRESSION";
        case DIAG_INVALID_CHARACTER:              return "INVALID_CHARACTER";
        case DIAG_UNMATCHED_OPENING_PARENTHESIS:  return "UNMATCHED_OPENING_PARENTHESIS";
        case DIAG_UNMATCHED_CLOSING_PARENTHESIS:  return "UNMATCHED_CLOSING_PARENTHESIS";
        case DIAG_CONSECUTIVE_OPERATORS:          return "CONSECUTIVE_OPERATORS";
        case DIAG_CONSECUTIVE_OPERANDS:           return "CONSECUTIVE_OPERANDS";
        case DIAG_INVALID_OPERATOR_POSITION:      return "INVALID_OPERATOR_POSITION";
        case DIAG_EMPTY_PARENTHESES:              return "EMPTY_PARENTHESES";
        case DIAG_INVALID_OPERAND:                return "INVALID_OPERAND";
        case DIAG_MISSING_OPERAND:                return "MISSING_OPERAND";
        case DIAG_MISSING_OPERATOR:               return "MISSING_OPERATOR";
        case DIAG_INVALID_NOTATION:               return "INVALID_NOTATION";
        case DIAG_AMBIGUOUS_NOTATION:             return "AMBIGUOUS_NOTATION";
        case DIAG_DIVISION_BY_ZERO:               return "DIVISION_BY_ZERO";
        case DIAG_MISSING_VARIABLE:               return "MISSING_VARIABLE";
        case DIAG_EVALUATION_ERROR:               return "EVALUATION_ERROR";
        default:                                  return "UNKNOWN";
    }
}
