/* bracket_validator.h - checks that ( ) [ ] { } are balanced and matched */
#ifndef BRACKET_VALIDATOR_H
#define BRACKET_VALIDATOR_H

#include "../tokenizer/tokenizer.h"

typedef struct {
    int  valid;              /* 1 = ok, 0 = error */
    int  position;           /* character offset of the problem, -1 if none */
    char message[96];
} ValidationResult;

ValidationResult validate_brackets(const Token tokens[], int count);

#endif
