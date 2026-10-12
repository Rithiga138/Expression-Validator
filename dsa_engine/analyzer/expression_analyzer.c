/* expression_analyzer.c - runs the full pipeline and assembles one result

   IMPORTANT ABOUT STEP ORDER (see expression_analyzer.h for why):
   detect_notation() (Batch 3) already calls validate_brackets() and
   validate_syntax() internally to decide "is this valid INFIX", and does
   its own left-to-right / right-to-left scans to decide "is this valid
   POSTFIX / PREFIX". So `notation` - not `syntax.valid` on its own - is
   what tells us whether the expression is usable: validate_syntax() only
   understands the infix grammar, so it correctly reports "invalid" for
   good postfix/prefix input such as "25 15 + 2 *" (two operands with no
   operator between them, from an infix point of view). We still store
   brackets/syntax in the result for transparency, and diagnosis.c only
   uses them to explain a genuinely NOTATION_INVALID expression. */
#include <stdio.h>
#include <string.h>
#include "expression_analyzer.h"

/* Joins token text with single spaces - plain string formatting, not an
   algorithm, so it does not duplicate anything from conversion.c (whose
   tokens_to_string() does the same thing but is private to that file). */
static void tokens_to_text(const Token tokens[], int count, char *out, int out_size)
{
    int i;
    size_t used = 0;
    out[0] = '\0';
    for (i = 0; i < count && used + 1 < (size_t)out_size; i++) {
        size_t len = strlen(tokens[i].text);
        if (i > 0 && used + 1 < (size_t)out_size) out[used++] = ' ';
        if (used + len >= (size_t)out_size) len = (size_t)out_size - used - 1;
        memcpy(out + used, tokens[i].text, len);
        used += len;
        out[used] = '\0';
    }
}

static void set_representations(ExpressionAnalysisResult *r,
                                const Token tokens[], int count)
{
    Token tmp[MAX_TOKENS];
    int n;

    r->postfix[0] = r->prefix[0] = r->infix[0] = '\0';

    switch (r->notation) {
        case NOTATION_INFIX:
        case NOTATION_AMBIGUOUS:
            /* A single operand ("A", "25") also satisfies infix_is_valid(),
               so both conversions work unchanged for AMBIGUOUS input too. */
            infix_to_postfix(tokens, count, r->postfix, sizeof r->postfix);
            infix_to_prefix (tokens, count, r->prefix,  sizeof r->prefix);
            tokens_to_text(tokens, count, r->infix, sizeof r->infix);
            break;

        case NOTATION_POSTFIX:
            tokens_to_text(tokens, count, r->postfix, sizeof r->postfix);
            if (postfix_to_infix(tokens, count, r->infix, sizeof r->infix) == CONV_OK) {
                /* postfix -> infix -> prefix, reusing infix_to_prefix instead
                   of writing a direct postfix->prefix converter. */
                n = tokenize(r->infix, tmp, MAX_TOKENS);
                if (n > 0) infix_to_prefix(tmp, n, r->prefix, sizeof r->prefix);
            }
            break;

        case NOTATION_PREFIX:
            tokens_to_text(tokens, count, r->prefix, sizeof r->prefix);
            if (prefix_to_infix(tokens, count, r->infix, sizeof r->infix) == CONV_OK) {
                /* prefix -> infix -> postfix, same reasoning as above. */
                n = tokenize(r->infix, tmp, MAX_TOKENS);
                if (n > 0) infix_to_postfix(tmp, n, r->postfix, sizeof r->postfix);
            }
            break;

        default:
            break;   /* NOTATION_INVALID: nothing to convert */
    }
}

static void run_evaluation(ExpressionAnalysisResult *r, const Token tokens[],
                           int count, const VariableTable *vars, int show_steps)
{
    r->evaluation_attempted = 1;
    switch (r->notation) {
        case NOTATION_INFIX:
            r->evaluation = evaluate_infix(tokens, count, vars, show_steps);
            break;
        case NOTATION_PREFIX:
            r->evaluation = evaluate_prefix(tokens, count, vars, show_steps);
            break;
        case NOTATION_POSTFIX:
            r->evaluation = evaluate_postfix(tokens, count, vars, show_steps);
            break;
        default:
            /* AMBIGUOUS: per spec, do not guess a notation - skip evaluation.
               (Call evaluate_postfix/prefix/infix directly if a specific
               notation is known.) */
            r->evaluation_attempted = 0;
            memset(&r->evaluation, 0, sizeof r->evaluation);
            return;
    }
    if (!r->evaluation.success) {
        r->diagnosis = diagnose_evaluation(r->evaluation.status, r->evaluation.message);
    }
}

ExpressionAnalysisResult analyze_expression(const char *expr,
                                            const VariableTable *vars,
                                            int show_steps)
{
    ExpressionAnalysisResult r;
    Token tokens[MAX_TOKENS];
    int n;

    memset(&r, 0, sizeof r);
    snprintf(r.expression, sizeof r.expression, "%s", expr ? expr : "");

    n = tokenize(r.expression, tokens, MAX_TOKENS);
    if (n < 0) {
        r.tokenize_ok = 0;
        r.valid = 0;
        r.diagnosis.category = DIAG_INVALID_CHARACTER;
        r.diagnosis.position = -1;
        snprintf(r.diagnosis.message, sizeof r.diagnosis.message,
                 "Expression could not be tokenized (too long, or too many tokens).");
        return r;
    }
    r.tokenize_ok  = 1;
    r.token_count  = n;

    if (n == 0) {
        r.valid = 0;
        r.diagnosis = diagnose_tokens(tokens, 0,
                                      (ValidationResult){0, -1, ""},
                                      (ValidationResult){0, -1, ""});
        return r;
    }

    r.brackets = validate_brackets(tokens, n);          /* Batch 2 */
    r.syntax   = validate_syntax(tokens, n);             /* Batch 2 */
    r.notation = detect_notation(tokens, n);              /* Batch 3 */

    r.valid = (r.notation == NOTATION_INFIX  || r.notation == NOTATION_PREFIX ||
              r.notation == NOTATION_POSTFIX || r.notation == NOTATION_AMBIGUOUS);

    if (!r.valid) {
        r.diagnosis = diagnose_tokens(tokens, n, r.brackets, r.syntax);
        r.correction_count = generate_corrections(r.expression, tokens, n,
                                                   r.diagnosis, r.corrections,
                                                   MAX_CORRECTIONS);
        return r;
    }

    if (r.notation == NOTATION_AMBIGUOUS) {
        r.diagnosis = diagnose_ambiguous_notation();
    }

    r.variable_count = var_collect(tokens, n, r.variables, VAR_TABLE_SIZE);  /* Batch 4 */
    set_representations(&r, tokens, n);
    run_evaluation(&r, tokens, n, vars, show_steps);

    return r;
}
