/* evaluator.h - evaluates postfix, prefix and infix expressions (Batch 4)

   Uses the existing modules:
     tokenizer.h          Token, TokenType, MAX_TOKENS        (Batch 1)
     bracket/syntax_validator.h  infix validation             (Batch 2)
     notation_detector.h  NotationType, detect_notation       (Batch 3)
     conversion.h         infix_to_postfix                    (Batch 3)

   Every function takes an ALREADY TOKENIZED expression. Call tokenize()
   first, exactly like the earlier batches do. */
#ifndef EVALUATOR_H
#define EVALUATOR_H

#include "../tokenizer/tokenizer.h"
#include "variable_manager.h"

#define EVAL_MESSAGE_LEN 96
#define EVAL_POSTFIX_LEN 1024

/* Lets the caller tell WHY an evaluation failed. */
typedef enum {
    EVAL_OK = 0,                 /* success                                  */
    EVAL_INVALID_EXPRESSION,     /* empty, too few/many operands, bad token,
                                    bad operator, bad brackets/syntax        */
    EVAL_MISSING_VARIABLE,       /* a variable has no value                  */
    EVAL_DIVISION_BY_ZERO,       /* x / 0  or  x % 0                         */
    EVAL_ERROR                   /* anything else (overflow, out of memory,
                                    conversion failure, invalid state)       */
} EvalStatus;

typedef struct {
    int        success;                     /* 1 = value is valid, 0 = error */
    EvalStatus status;                      /* EVAL_OK when success == 1     */
    double     value;                       /* the result (0.0 on failure)   */
    char       message[EVAL_MESSAGE_LEN];   /* "Evaluation successful" or the
                                               error text, e.g.
                                               "Division by zero"            */
    char       postfix[EVAL_POSTFIX_LEN];   /* evaluate_infix: the converted
                                               postfix text. "" otherwise.   */
} EvaluationResult;

/* vars      : table of variable values. May be NULL if the expression has
               no variables.
   show_steps: 1 = print a step-by-step stack trace to the console,
               0 = silent. */

/* Scans LEFT to RIGHT.  Example tokens:  25 15 + 2 *   -> 80 */
EvaluationResult evaluate_postfix(const Token tokens[], int count,
                                  const VariableTable *vars, int show_steps);

/* Scans RIGHT to LEFT.  Example tokens:  * + 25 15 2   -> 80 */
EvaluationResult evaluate_prefix(const Token tokens[], int count,
                                 const VariableTable *vars, int show_steps);

/* Validates, converts infix -> postfix (conversion.c), then calls
   evaluate_postfix.  Example tokens:  ( 25 + 15 ) * 2   -> 80 */
EvaluationResult evaluate_infix(const Token tokens[], int count,
                                const VariableTable *vars, int show_steps);

/* Convenience: detect_notation() picks INFIX / PREFIX / POSTFIX, then the
   matching function above is called. A single operand ("A", "25") is
   AMBIGUOUS and simply evaluates to its own value. */
EvaluationResult evaluate_tokens(const Token tokens[], int count,
                                 const VariableTable *vars, int show_steps);

/* "OK", "INVALID_EXPRESSION", "MISSING_VARIABLE", ... */
const char *eval_status_name(EvalStatus status);

#endif
