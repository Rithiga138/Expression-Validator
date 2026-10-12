/* notation_detector.h - decides whether tokens are infix, prefix or postfix */
#ifndef NOTATION_DETECTOR_H
#define NOTATION_DETECTOR_H

#include "../tokenizer/tokenizer.h"

typedef enum {
    NOTATION_INVALID,
    NOTATION_INFIX,
    NOTATION_PREFIX,
    NOTATION_POSTFIX,
    NOTATION_AMBIGUOUS
} NotationType;

NotationType detect_notation(const Token tokens[], int count);
const char  *notation_name(NotationType type);

#endif
