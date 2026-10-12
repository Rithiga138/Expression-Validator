/* conversion.c - shunting-yard conversions, tree-free postfix/prefix -> infix,
   and expression evaluation. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "conversion.h"
#include "../stack/stack.h"
#include "../validation/bracket_validator.h"
#include "../validation/syntax_validator.h"

#define EXPR_BUF 1024

static int precedence(char op)
{
    switch (op) {
        case '+': case '-':            return 1;
        case '*': case '/': case '%':  return 2;
        case '^':                      return 3;
        default:                       return 0;
    }
}

static int is_right_assoc(char op) { return op == '^'; }

static int is_operand_tok(const Token *t)
{
    return t->type == TOKEN_NUMBER || t->type == TOKEN_VARIABLE;
}

static Token make_op_token(char op)
{
    Token t;
    t.type = TOKEN_OPERATOR;
    t.text[0] = op;
    t.text[1] = '\0';
    t.position = -1;
    return t;
}

static int tokens_to_string(const Token tokens[], int count, char *out, int out_size)
{
    int i;
    size_t used = 0;
    if (out == NULL || out_size <= 0) return CONV_ERROR;
    out[0] = '\0';
    for (i = 0; i < count; i++) {
        size_t len = strlen(tokens[i].text);
        if (used + len + 2 > (size_t)out_size) return CONV_ERROR;
        if (i > 0) out[used++] = ' ';
        memcpy(out + used, tokens[i].text, len);
        used += len;
        out[used] = '\0';
    }
    return CONV_OK;
}

static int infix_is_valid(const Token tokens[], int count)
{
    return tokens != NULL && count > 0 &&
           validate_brackets(tokens, count).valid &&
           validate_syntax(tokens, count).valid;
}

/* Shunting-yard. When reversed != 0 the input is the REVERSED infix token
   list (used to build prefix): associativity rules flip. */
static int shunting_yard(const Token in[], int count, Token out[], int reversed)
{
    Stack ops;
    int i, n = 0;
    char top;

    if (stack_init(&ops, count + 1) != STACK_OK) return -1;

    for (i = 0; i < count; i++) {
        const Token *t = &in[i];
        if (is_operand_tok(t)) {
            out[n++] = *t;
        } else if (t->type == TOKEN_LPAREN) {
            stack_push(&ops, t->text[0]);
        } else if (t->type == TOKEN_RPAREN) {
            while (stack_peek(&ops, &top) == STACK_OK &&
                   top != '(' && top != '[' && top != '{') {
                stack_pop(&ops, &top);
                out[n++] = make_op_token(top);
            }
            stack_pop(&ops, NULL);   /* discard the opening bracket */
        } else if (t->type == TOKEN_OPERATOR) {
            char cur = t->text[0];
            while (stack_peek(&ops, &top) == STACK_OK && precedence(top) > 0) {
                int pt = precedence(top), pc = precedence(cur);
                int pop_it = pt > pc ||
                    (pt == pc && (reversed ? is_right_assoc(cur)
                                           : !is_right_assoc(cur)));
                if (!pop_it) break;
                stack_pop(&ops, &top);
                out[n++] = make_op_token(top);
            }
            stack_push(&ops, cur);
        }
    }
    while (stack_pop(&ops, &top) == STACK_OK) {
        out[n++] = make_op_token(top);
    }
    stack_free(&ops);
    return n;
}

static int infix_to_postfix_tokens(const Token tokens[], int count, Token out[])
{
    if (!infix_is_valid(tokens, count)) return -1;
    return shunting_yard(tokens, count, out, 0);
}

int infix_to_postfix(const Token tokens[], int count, char *out, int out_size)
{
    Token *res;
    int n, rc;
    if (!infix_is_valid(tokens, count)) return CONV_ERROR;
    res = (Token *)malloc((size_t)(count + 1) * sizeof(Token));
    if (res == NULL) return CONV_ERROR;
    n = infix_to_postfix_tokens(tokens, count, res);
    rc = (n < 0) ? CONV_ERROR : tokens_to_string(res, n, out, out_size);
    free(res);
    return rc;
}

static char closing_of(char open)
{
    return open == '(' ? ')' : (open == '[' ? ']' : '}');
}

static char opening_of(char close)
{
    return close == ')' ? '(' : (close == ']' ? '[' : '{');
}

int infix_to_prefix(const Token tokens[], int count, char *out, int out_size)
{
    Token *rev, *res;
    int i, n, rc;

    if (!infix_is_valid(tokens, count)) return CONV_ERROR;
    rev = (Token *)malloc((size_t)count * sizeof(Token));
    res = (Token *)malloc((size_t)(count + 1) * sizeof(Token));
    if (rev == NULL || res == NULL) { free(rev); free(res); return CONV_ERROR; }

    /* reverse the tokens and swap the bracket directions */
    for (i = 0; i < count; i++) {
        rev[i] = tokens[count - 1 - i];
        if (rev[i].type == TOKEN_LPAREN) {
            rev[i].type = TOKEN_RPAREN;
            rev[i].text[0] = closing_of(rev[i].text[0]);
        } else if (rev[i].type == TOKEN_RPAREN) {
            rev[i].type = TOKEN_LPAREN;
            rev[i].text[0] = opening_of(rev[i].text[0]);
        }
    }
    n = shunting_yard(rev, count, res, 1);
    if (n < 0) {
        rc = CONV_ERROR;
    } else {
        /* reverse the result to get prefix order */
        for (i = 0; i < n / 2; i++) {
            Token tmp = res[i];
            res[i] = res[n - 1 - i];
            res[n - 1 - i] = tmp;
        }
        rc = tokens_to_string(res, n, out, out_size);
    }
    free(rev);
    free(res);
    return rc;
}

/* ---- postfix / prefix -> fully bracketed infix ---- */

static void strip_outer(char *s)
{
    size_t len = strlen(s);
    int depth = 0;
    size_t i;
    if (len < 2 || s[0] != '(' || s[len - 1] != ')') return;
    for (i = 0; i < len - 1; i++) {
        if (s[i] == '(') depth++;
        else if (s[i] == ')') depth--;
        if (depth == 0) return;      /* first '(' closes before the end */
    }
    memmove(s, s + 1, len - 2);
    s[len - 2] = '\0';
}

static int build_infix(const Token tokens[], int count, int from_right,
                       char *out, int out_size)
{
    char (*stk)[EXPR_BUF];
    int sp = 0, i, rc = CONV_ERROR;

    if (tokens == NULL || count <= 0) return CONV_ERROR;
    stk = malloc((size_t)count * sizeof *stk);
    if (stk == NULL) return CONV_ERROR;

    for (i = from_right ? count - 1 : 0; from_right ? i >= 0 : i < count;
         i += from_right ? -1 : 1) {
        if (is_operand_tok(&tokens[i])) {
            snprintf(stk[sp++], EXPR_BUF, "%s", tokens[i].text);
        } else if (tokens[i].type == TOKEN_OPERATOR) {
            char a[EXPR_BUF], b[EXPR_BUF];
            int written;
            if (sp < 2) goto done;
            if (from_right) {          /* prefix: first pop = left operand */
                snprintf(a, EXPR_BUF, "%s", stk[--sp]);
                snprintf(b, EXPR_BUF, "%s", stk[--sp]);
            } else {                   /* postfix: first pop = right operand */
                snprintf(b, EXPR_BUF, "%s", stk[--sp]);
                snprintf(a, EXPR_BUF, "%s", stk[--sp]);
            }
            written = snprintf(stk[sp], EXPR_BUF, "(%s %s %s)", a, tokens[i].text, b);
            if (written < 0 || written >= EXPR_BUF) goto done;
            sp++;
        } else {
            goto done;
        }
    }
    if (sp != 1) goto done;
    strip_outer(stk[0]);
    if (out != NULL && out_size > 0 && strlen(stk[0]) < (size_t)out_size) {
        strcpy(out, stk[0]);
        rc = CONV_OK;
    }
done:
    free(stk);
    return rc;
}

int postfix_to_infix(const Token tokens[], int count, char *out, int out_size)
{
    return build_infix(tokens, count, 0, out, out_size);
}

int prefix_to_infix(const Token tokens[], int count, char *out, int out_size)
{
    return build_infix(tokens, count, 1, out, out_size);
}

/* ---- evaluation ---- */

static void set_err(char *err, int err_size, const char *msg)
{
    if (err != NULL && err_size > 0) snprintf(err, (size_t)err_size, "%s", msg);
}

static int apply(char op, double a, double b, double *r, char *err, int err_size)
{
    switch (op) {
        case '+': *r = a + b; return CONV_OK;
        case '-': *r = a - b; return CONV_OK;
        case '*': *r = a * b; return CONV_OK;
        case '/':
            if (b == 0.0) { set_err(err, err_size, "Division by zero"); return CONV_ERROR; }
            *r = a / b; return CONV_OK;
        case '%':
            if (b == 0.0) { set_err(err, err_size, "Modulo by zero"); return CONV_ERROR; }
            *r = fmod(a, b); return CONV_OK;
        case '^': *r = pow(a, b); return CONV_OK;
        default:
            set_err(err, err_size, "Unknown operator");
            return CONV_ERROR;
    }
}

static int eval_linear(const Token tokens[], int count, int from_right,
                       double *result, char *err, int err_size)
{
    double *vals;
    int sp = 0, i, rc = CONV_ERROR;

    vals = (double *)malloc((size_t)count * sizeof(double));
    if (vals == NULL) { set_err(err, err_size, "Out of memory"); return CONV_ERROR; }

    for (i = from_right ? count - 1 : 0; from_right ? i >= 0 : i < count;
         i += from_right ? -1 : 1) {
        if (tokens[i].type == TOKEN_NUMBER) {
            vals[sp++] = atof(tokens[i].text);
        } else if (tokens[i].type == TOKEN_VARIABLE) {
            set_err(err, err_size, "Cannot evaluate: expression contains a variable");
            goto done;
        } else if (tokens[i].type == TOKEN_OPERATOR) {
            double a, b, r;
            if (sp < 2) { set_err(err, err_size, "Not enough operands"); goto done; }
            if (from_right) { a = vals[--sp]; b = vals[--sp]; }
            else            { b = vals[--sp]; a = vals[--sp]; }
            if (apply(tokens[i].text[0], a, b, &r, err, err_size) != CONV_OK) goto done;
            vals[sp++] = r;
        } else {
            set_err(err, err_size, "Unexpected token");
            goto done;
        }
    }
    if (sp != 1) { set_err(err, err_size, "Malformed expression"); goto done; }
    *result = vals[0];
    rc = CONV_OK;
done:
    free(vals);
    return rc;
}

int evaluate_expression(const Token tokens[], int count, NotationType type,
                        double *result, char *err, int err_size)
{
    Token *post;
    int n, rc;

    if (tokens == NULL || count <= 0 || result == NULL) {
        set_err(err, err_size, "Nothing to evaluate");
        return CONV_ERROR;
    }
    switch (type) {
        case NOTATION_POSTFIX:
            return eval_linear(tokens, count, 0, result, err, err_size);
        case NOTATION_PREFIX:
            return eval_linear(tokens, count, 1, result, err, err_size);
        case NOTATION_AMBIGUOUS:
            /* a single operand: its value is itself */
            return eval_linear(tokens, count, 0, result, err, err_size);
        case NOTATION_INFIX:
            post = (Token *)malloc((size_t)(count + 1) * sizeof(Token));
            if (post == NULL) { set_err(err, err_size, "Out of memory"); return CONV_ERROR; }
            n = infix_to_postfix_tokens(tokens, count, post);
            if (n < 0) {
                set_err(err, err_size, "Invalid infix expression");
                free(post);
                return CONV_ERROR;
            }
            rc = eval_linear(post, n, 0, result, err, err_size);
            free(post);
            return rc;
        default:
            set_err(err, err_size, "Invalid expression");
            return CONV_ERROR;
    }
}
