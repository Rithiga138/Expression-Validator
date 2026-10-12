/* diagnosis.h - turns validation/evaluation results into a human-readable
   explanation.

   This module does NOT re-validate anything itself. It only reads the
   ValidationResult (Batch 2) and EvalStatus (Batch 4) that the existing
   modules already produced, and classifies their message text and
   position into one of a small, fixed set of categories. */
#ifndef DIAGNOSIS_H
#define DIAGNOSIS_H

#include "../tokenizer/tokenizer.h"
#include "../validation/bracket_validator.h"   /* ValidationResult          */
#include "../evaluation/evaluator.h"           /* EvalStatus                */

typedef enum {
    DIAG_NONE = 0,                       /* expression is fine, nothing to report */
    DIAG_EMPTY_EXPRESSION,
    DIAG_INVALID_CHARACTER,
    DIAG_UNMATCHED_OPENING_PARENTHESIS,
    DIAG_UNMATCHED_CLOSING_PARENTHESIS,
    DIAG_CONSECUTIVE_OPERATORS,
    DIAG_CONSECUTIVE_OPERANDS,
    DIAG_INVALID_OPERATOR_POSITION,
    DIAG_EMPTY_PARENTHESES,
    DIAG_INVALID_OPERAND,
    DIAG_MISSING_OPERAND,
    DIAG_MISSING_OPERATOR,
    DIAG_INVALID_NOTATION,
    DIAG_AMBIGUOUS_NOTATION,
    DIAG_DIVISION_BY_ZERO,
    DIAG_MISSING_VARIABLE,
    DIAG_EVALUATION_ERROR
} DiagnosisCategory;

#define DIAG_MESSAGE_LEN  128
#define DIAG_TOKEN_LEN    MAX_TOKEN_LEN   /* reuse the tokenizer's own size */

typedef struct {
    DiagnosisCategory category;
    int    position;                 /* character offset, -1 if not applicable */
    char   message[DIAG_MESSAGE_LEN];/* one-line human explanation             */
    char   expected[48];             /* e.g. "Operand or '('", "" if n/a       */
    char   found[DIAG_TOKEN_LEN];    /* the offending token's text, "" if n/a  */
} Diagnosis;

/* Classifies a STRUCTURAL problem (empty input, unbalanced brackets, or a
   broken infix-style token order) using the results Batch 2 already
   computed. Pass the ValidationResult from validate_brackets() and
   validate_syntax() exactly as returned; this function does not call them
   itself, so nothing is validated twice. */
Diagnosis diagnose_tokens(const Token tokens[], int count,
                          ValidationResult brackets, ValidationResult syntax);

/* Explains an AMBIGUOUS notation result (Batch 3). */
Diagnosis diagnose_ambiguous_notation(void);

/* Classifies an EVALUATION problem (Batch 4): division/modulo by zero,
   a missing variable, or any other evaluator error. Pass the EvalStatus
   and message exactly as evaluate_postfix/prefix/infix returned them. */
Diagnosis diagnose_evaluation(EvalStatus status, const char *eval_message);

/* Finds the token whose .position field equals `position`.
   Returns its index, or -1 if none matches (e.g. position == -1). */
int diagnosis_find_token(const Token tokens[], int count, int position);

const char *diagnosis_category_name(DiagnosisCategory category);

#endif
