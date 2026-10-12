/* conversion.h - convert between notations and evaluate expressions */
#ifndef CONVERSION_H
#define CONVERSION_H

#include "../tokenizer/tokenizer.h"
#include "notation_detector.h"

#define CONV_OK    0
#define CONV_ERROR -1

/* Results are written as space-separated text into out (out_size bytes). */
int infix_to_postfix(const Token tokens[], int count, char *out, int out_size);
int infix_to_prefix (const Token tokens[], int count, char *out, int out_size);
int postfix_to_infix(const Token tokens[], int count, char *out, int out_size);
int prefix_to_infix (const Token tokens[], int count, char *out, int out_size);

/* Evaluates an expression of any notation that contains only numbers.
   On failure returns CONV_ERROR and fills err. */
int evaluate_expression(const Token tokens[], int count, NotationType type,
                        double *result, char *err, int err_size);

#endif
