/* expression_analyzer.h - the master coordinator for Batch 5

   analyze_expression() is the ONE function a caller needs. It runs the
   raw string through every earlier batch, in order, and never re-does
   what they already do:

     tokenize()            Batch 1
     validate_brackets()   Batch 2
     validate_syntax()     Batch 2
     detect_notation()     Batch 3
     infix_to_postfix/prefix, postfix_to_infix, prefix_to_infix   Batch 3
     evaluate_postfix/prefix/infix                                Batch 4
     var_collect()                                                 Batch 4
     diagnose_tokens/diagnose_evaluation                           Batch 5 (diagnosis.c)
     generate_corrections()                                        Batch 5 (correction_generator.c) */
#ifndef EXPRESSION_ANALYZER_H
#define EXPRESSION_ANALYZER_H

#include "../tokenizer/tokenizer.h"
#include "../validation/bracket_validator.h"
#include "../validation/syntax_validator.h"
#include "../notation/notation_detector.h"
#include "../notation/conversion.h"
#include "../evaluation/evaluator.h"
#include "../evaluation/variable_manager.h"
#include "diagnosis.h"
#include "correction_generator.h"

#define ANALYZER_EXPR_LEN 256

typedef struct {
    char expression[ANALYZER_EXPR_LEN];   /* the raw input, copied (may be truncated) */

    int  tokenize_ok;                     /* 1 = tokenize() succeeded            */
    int  token_count;

    ValidationResult brackets;            /* Batch 2 - always meaningful          */
    ValidationResult syntax;              /* Batch 2 - checks the INFIX grammar
                                              only, so it can be "invalid" for a
                                              perfectly good prefix/postfix
                                              expression; `notation` below is
                                              what actually decides validity     */
    NotationType     notation;            /* Batch 3                              */

    int  valid;                           /* 1 = a usable INFIX/PREFIX/POSTFIX or
                                              AMBIGUOUS expression                */

    Diagnosis diagnosis;                  /* DIAG_NONE when there is nothing to
                                              report; otherwise explains either a
                                              structural problem (valid == 0) or
                                              an evaluation problem (valid == 1
                                              but evaluation still failed)       */

    int  correction_count;
    CorrectionCandidate corrections[MAX_CORRECTIONS];

    char postfix[EVAL_POSTFIX_LEN];       /* "" if not applicable                 */
    char prefix [EVAL_POSTFIX_LEN];       /* "" if not applicable                 */
    char infix  [EVAL_POSTFIX_LEN];       /* fully bracketed equivalent infix,
                                              "" if not applicable                */

    char variables[VAR_TABLE_SIZE];       /* distinct variable letters used       */
    int  variable_count;

    int  evaluation_attempted;            /* 1 = evaluation was tried              */
    EvaluationResult evaluation;          /* meaningful only if evaluation_attempted */
} ExpressionAnalysisResult;

/* vars       : variable values to use, or NULL if the expression has none
                (or you don't want evaluation to happen yet).
   show_steps : forwarded to the evaluator; 1 prints the stack trace. */
ExpressionAnalysisResult analyze_expression(const char *expr,
                                            const VariableTable *vars,
                                            int show_steps);

#endif
