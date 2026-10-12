/* tokenizer.c - splits an expression string into tokens */
#include <ctype.h>
#include <string.h>
#include "tokenizer.h"

static int is_operator_char(char c)
{
    return c == '+' || c == '-' || c == '*' || c == '/' || c == '^' || c == '%';
}

static int is_open_bracket(char c)  { return c == '(' || c == '[' || c == '{'; }
static int is_close_bracket(char c) { return c == ')' || c == ']' || c == '}'; }

static int add_token(Token tokens[], int *count, int max_tokens,
                     TokenType type, const char *text, int len, int pos)
{
    Token *t;
    if (*count >= max_tokens) {
        return -1;
    }
    t = &tokens[*count];
    t->type = type;
    if (len >= MAX_TOKEN_LEN) {
        len = MAX_TOKEN_LEN - 1;
        t->type = TOKEN_INVALID;          /* too long to be a real number */
    }
    memcpy(t->text, text, (size_t)len);
    t->text[len] = '\0';
    t->position = pos;
    (*count)++;
    return 0;
}

int tokenize(const char *expr, Token tokens[], int max_tokens)
{
    int i = 0;
    int count = 0;

    if (expr == NULL || tokens == NULL || max_tokens <= 0) {
        return -1;
    }

    while (expr[i] != '\0') {
        unsigned char c = (unsigned char)expr[i];

        if (isspace(c)) {
            i++;
        } else if (isdigit(c) || c == '.') {
            int start = i;
            int dots = 0;
            int digits = 0;
            TokenType type;
            while (isdigit((unsigned char)expr[i]) || expr[i] == '.') {
                if (expr[i] == '.') dots++; else digits++;
                i++;
            }
            type = (dots <= 1 && digits > 0) ? TOKEN_NUMBER : TOKEN_INVALID;
            if (add_token(tokens, &count, max_tokens, type,
                          expr + start, i - start, start) < 0) return -1;
        } else if (isalpha(c)) {
            /* every letter is its own variable, so "AB+" is A, B, + */
            if (add_token(tokens, &count, max_tokens, TOKEN_VARIABLE,
                          expr + i, 1, i) < 0) return -1;
            i++;
        } else if (is_operator_char((char)c)) {
            if (add_token(tokens, &count, max_tokens, TOKEN_OPERATOR,
                          expr + i, 1, i) < 0) return -1;
            i++;
        } else if (is_open_bracket((char)c)) {
            if (add_token(tokens, &count, max_tokens, TOKEN_LPAREN,
                          expr + i, 1, i) < 0) return -1;
            i++;
        } else if (is_close_bracket((char)c)) {
            if (add_token(tokens, &count, max_tokens, TOKEN_RPAREN,
                          expr + i, 1, i) < 0) return -1;
            i++;
        } else {
            if (add_token(tokens, &count, max_tokens, TOKEN_INVALID,
                          expr + i, 1, i) < 0) return -1;
            i++;
        }
    }
    return count;
}

const char *token_type_name(TokenType type)
{
    switch (type) {
        case TOKEN_NUMBER:   return "NUMBER";
        case TOKEN_VARIABLE: return "VARIABLE";
        case TOKEN_OPERATOR: return "OPERATOR";
        case TOKEN_LPAREN:   return "LPAREN";
        case TOKEN_RPAREN:   return "RPAREN";
        default:             return "INVALID";
    }
}
