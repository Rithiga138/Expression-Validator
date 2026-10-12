/* correction_generator.h - small, deterministic, rule-based correction
   candidates for structurally invalid expressions.

   This is NOT an AI/ML system. There is no learning, no external calls,
   and no attempt to find "all possible" corrections. A fixed, small set
   of hand-written rules fires depending on the Diagnosis category, and
   EVERY candidate it produces is re-run through the real tokenizer and
   validators (Batch 1/2) before it is accepted. A rule that would need to
   guess (e.g. "insert some operator, we don't know which") is simply not
   implemented, so the candidate list may legitimately be empty. */
#ifndef CORRECTION_GENERATOR_H
#define CORRECTION_GENERATOR_H

#include "../tokenizer/tokenizer.h"
#include "diagnosis.h"

#define MAX_CORRECTIONS      5
#define CORRECTION_EXPR_LEN  256
#define CORRECTION_REASON_LEN 96

typedef struct {
    char expression[CORRECTION_EXPR_LEN];   /* the corrected text            */
    char reason[CORRECTION_REASON_LEN];     /* why this change was suggested */
} CorrectionCandidate;

/* Generates up to max_candidates possible corrections for `expr`, based on
   the Diagnosis already computed for it (see diagnosis.h). tokens/count
   must be the ORIGINAL expression's tokens (the ones diag.position refers
   to). Returns how many candidates were written into candidates[] -
   0 is a valid result meaning "no safe rule applied".

   Every returned candidate has ALREADY been tokenized and passed both
   validate_brackets() and validate_syntax() successfully. */
int generate_corrections(const char *expr, const Token tokens[], int count,
                         Diagnosis diag, CorrectionCandidate candidates[],
                         int max_candidates);

#endif
