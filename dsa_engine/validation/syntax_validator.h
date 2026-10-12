/* syntax_validator.h - checks the ORDER of tokens against the infix grammar */
#ifndef SYNTAX_VALIDATOR_H
#define SYNTAX_VALIDATOR_H

#include "bracket_validator.h"   /* for ValidationResult */

ValidationResult validate_syntax(const Token tokens[], int count);

#endif
