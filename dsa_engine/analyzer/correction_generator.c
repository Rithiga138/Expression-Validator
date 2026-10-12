/* correction_generator.c - deterministic correction rules

   Every rule below does the same three things:
     1. Build ONE candidate string using simple, explainable string editing
        (delete a token's characters, or insert one character).
     2. Re-run it through tokenize() + validate_brackets() + validate_syntax()
        - the exact same functions the rest of the project already uses.
     3. Only keep the candidate if step 2 says it is valid.
   Nothing here decides "this looks right" by itself; the existing
   validators (Batch 1/2) always have the final word. */
#include <stdio.h>
#include <string.h>
#include "correction_generator.h"
#include "../stack/stack.h"
#include "../validation/bracket_validator.h"
#include "../validation/syntax_validator.h"

/* ---- small string helpers (plain editing, not a DSA algorithm) ---- */

/* Copies src into out, skipping `len` characters starting at `start`. */
static void remove_span(const char *src, int start, int len,
                        char *out, int out_size)
{
    int si = 0, oi = 0;
    int n = (int)strlen(src);
    while (si < n && oi < out_size - 1) {
        if (si >= start && si < start + len) { si++; continue; }
        out[oi++] = src[si++];
    }
    out[oi] = '\0';
}

/* Copies src into out with `ins` inserted right before offset `pos`. */
static void insert_at(const char *src, int pos, const char *ins,
                      char *out, int out_size)
{
    int n = (int)strlen(src);
    int ins_len = (int)strlen(ins);
    int oi = 0, si;
    if (pos < 0) pos = 0;
    if (pos > n) pos = n;
    for (si = 0; si < pos && oi < out_size - 1; si++) out[oi++] = src[si];
    for (si = 0; si < ins_len && oi < out_size - 1; si++) out[oi++] = ins[si];
    for (si = pos; si < n && oi < out_size - 1; si++) out[oi++] = src[si];
    out[oi] = '\0';
}

/* Collapses runs of spaces into one and trims the ends. Cosmetic only -
   tokenize() already treats any run of whitespace as a single separator,
   so this never changes whether a candidate is valid, only how it reads. */
static void normalize_spaces(char *s)
{
    int r = 0, w = 0;
    int at_start = 1;
    int pending_space = 0;
    int len = (int)strlen(s);
    for (r = 0; r < len; r++) {
        if (s[r] == ' ' || s[r] == '\t') {
            if (!at_start) pending_space = 1;
        } else {
            if (pending_space) { s[w++] = ' '; pending_space = 0; }
            s[w++] = s[r];
            at_start = 0;
        }
    }
    s[w] = '\0';
}

static char closing_of(char open)
{
    return open == '(' ? ')' : (open == '[' ? ']' : '}');
}

/* Runs the candidate through the real pipeline. Returns 1 if it is a
   structurally valid expression (brackets AND infix syntax both pass). */
static int candidate_is_valid(const char *candidate)
{
    Token tmp[MAX_TOKENS];
    int n = tokenize(candidate, tmp, MAX_TOKENS);
    if (n <= 0) return 0;
    if (!validate_brackets(tmp, n).valid) return 0;
    if (!validate_syntax(tmp, n).valid)   return 0;
    return 1;
}

static int add_if_valid(CorrectionCandidate candidates[], int max_candidates,
                        int used, char *candidate, const char *reason)
{
    if (used >= max_candidates) return used;
    normalize_spaces(candidate);
    if (!candidate_is_valid(candidate)) return used;
    snprintf(candidates[used].expression, CORRECTION_EXPR_LEN, "%s", candidate);
    snprintf(candidates[used].reason, CORRECTION_REASON_LEN, "%s", reason);
    return used + 1;
}

/* ---- rules, one per Diagnosis category ---- */

/* "(25 + 15"  ->  "(25 + 15)"
   Replays the same open/close scan validate_brackets() does, but keeps the
   leftover open brackets (instead of just reporting the first one) so we
   know exactly which closing characters to append, and in which order. */
static int rule_unmatched_opening(const char *expr, const Token tokens[], int count,
                                  CorrectionCandidate candidates[], int max_candidates,
                                  int used)
{
    Stack s;
    char candidate[CORRECTION_EXPR_LEN];
    int i;
    size_t len;

    if (stack_init(&s, count + 1) != STACK_OK) return used;
    for (i = 0; i < count; i++) {
        if (tokens[i].type == TOKEN_LPAREN) {
            stack_push(&s, tokens[i].text[0]);
        } else if (tokens[i].type == TOKEN_RPAREN) {
            char top;
            if (stack_peek(&s, &top) == STACK_OK) stack_pop(&s, NULL);
        }
    }
    snprintf(candidate, sizeof candidate, "%s", expr);
    len = strlen(candidate);
    while (!stack_is_empty(&s) && len + 1 < sizeof candidate) {
        char open;
        stack_pop(&s, &open);
        candidate[len++] = closing_of(open);
        candidate[len] = '\0';
    }
    stack_free(&s);
    return add_if_valid(candidates, max_candidates, used, candidate,
                        "Added the missing closing parenthesis/parentheses.");
}

/* "25 + 15)"  ->  "25 + 15" */
static int rule_unmatched_closing(const char *expr, int position,
                                  CorrectionCandidate candidates[], int max_candidates,
                                  int used)
{
    char candidate[CORRECTION_EXPR_LEN];
    if (position < 0) return used;
    remove_span(expr, position, 1, candidate, sizeof candidate);
    return add_if_valid(candidates, max_candidates, used, candidate,
                        "Removed the extra closing bracket.");
}

/* "25 + * 2" -> "25 + 2"  (remove the 2nd operator)
                 "25 * 2"  (remove the 1st operator) */
static int rule_consecutive_operators(const char *expr, const Token tokens[], int count,
                                      int position, CorrectionCandidate candidates[],
                                      int max_candidates, int used)
{
    int second = diagnosis_find_token(tokens, count, position);
    char candidate[CORRECTION_EXPR_LEN];

    if (second < 0 || second >= count || tokens[second].type != TOKEN_OPERATOR) {
        return used;
    }
    remove_span(expr, tokens[second].position, (int)strlen(tokens[second].text),
               candidate, sizeof candidate);
    used = add_if_valid(candidates, max_candidates, used, candidate,
                        "Removed the second of two consecutive operators.");

    if (second > 0 && tokens[second - 1].type == TOKEN_OPERATOR) {
        int first = second - 1;
        remove_span(expr, tokens[first].position, (int)strlen(tokens[first].text),
                   candidate, sizeof candidate);
        used = add_if_valid(candidates, max_candidates, used, candidate,
                            "Removed the first of two consecutive operators.");
    }
    return used;
}

/* "25 15" -> "25+15"  (insert '+' where the 2nd operand starts) */
static int rule_missing_operator(const char *expr, int position,
                                 CorrectionCandidate candidates[], int max_candidates,
                                 int used)
{
    char candidate[CORRECTION_EXPR_LEN];
    if (position < 0) return used;
    insert_at(expr, position, "+", candidate, sizeof candidate);
    return add_if_valid(candidates, max_candidates, used, candidate,
                        "Inserted '+' between two operands that had no operator.");
}

/* "25 + () * 2" -> "25 +  * 2" (only kept if this happens to become valid;
   removing an empty () very often just exposes a second problem, e.g.
   consecutive operators, in which case candidate_is_valid() rejects it -
   exactly per the "only show it if it actually becomes valid" rule.) */
static int rule_empty_parentheses(const char *expr, const Token tokens[], int count,
                                  int position, CorrectionCandidate candidates[],
                                  int max_candidates, int used)
{
    int close_idx = diagnosis_find_token(tokens, count, position);
    char candidate[CORRECTION_EXPR_LEN];
    int start, len;

    if (close_idx <= 0 || close_idx >= count ||
        tokens[close_idx].type != TOKEN_RPAREN ||
        tokens[close_idx - 1].type != TOKEN_LPAREN) {
        return used;
    }
    start = tokens[close_idx - 1].position;
    len   = (tokens[close_idx].position + 1) - start;
    remove_span(expr, start, len, candidate, sizeof candidate);
    return add_if_valid(candidates, max_candidates, used, candidate,
                        "Removed the empty parentheses.");
}

int generate_corrections(const char *expr, const Token tokens[], int count,
                         Diagnosis diag, CorrectionCandidate candidates[],
                         int max_candidates)
{
    int used = 0;
    if (expr == NULL || tokens == NULL || candidates == NULL || max_candidates <= 0) {
        return 0;
    }

    switch (diag.category) {
        case DIAG_UNMATCHED_OPENING_PARENTHESIS:
            used = rule_unmatched_opening(expr, tokens, count, candidates, max_candidates, used);
            break;
        case DIAG_UNMATCHED_CLOSING_PARENTHESIS:
            used = rule_unmatched_closing(expr, diag.position, candidates, max_candidates, used);
            break;
        case DIAG_CONSECUTIVE_OPERATORS:
            used = rule_consecutive_operators(expr, tokens, count, diag.position,
                                              candidates, max_candidates, used);
            break;
        case DIAG_MISSING_OPERATOR:
        case DIAG_CONSECUTIVE_OPERANDS:
            used = rule_missing_operator(expr, diag.position, candidates, max_candidates, used);
            break;
        case DIAG_EMPTY_PARENTHESES:
            used = rule_empty_parentheses(expr, tokens, count, diag.position,
                                          candidates, max_candidates, used);
            break;
        default:
            /* No safe, explainable rule for this category (e.g. an invalid
               character, or an expression that is simply empty) - leave the
               candidate list empty rather than guess. */
            break;
    }
    return used;
}
