/* tokenizer.h - splits an expression string into tokens */
#ifndef TOKENIZER_H
#define TOKENIZER_H

#define MAX_TOKEN_LEN 32
#define MAX_TOKENS    256

typedef enum {
    TOKEN_NUMBER,
    TOKEN_VARIABLE,
    TOKEN_OPERATOR,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_INVALID
} TokenType;

typedef struct {
    TokenType type;
    char      text[MAX_TOKEN_LEN];
    int       position;      /* character offset in the source string */
} Token;

/* Returns the number of tokens written, or -1 on bad arguments / overflow. */
int tokenize(const char *expr, Token tokens[], int max_tokens);
const char *token_type_name(TokenType type);

#endif
