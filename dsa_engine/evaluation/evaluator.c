/* evaluator.c - stack-based evaluation of postfix, prefix and infix

   WHY A PRIVATE NUMERIC STACK?
   The Batch 1 Stack stores one 'char' per element, so it cannot hold
   numbers such as 25.5 or 80. Instead of changing that module, this file has
   its own tiny stack of doubles (NumStack). It works exactly like the
   Batch 1 stack: push / pop at the top only (LIFO). It is private (static),
   so nothing else in the project depends on it.

   MODULO: C's '%' works only on integers, so 5.5 % 2 would not compile.
   fmod() from <math.h> is the floating-point remainder:
   fmod(25, 4) = 1 and fmod(5.5, 2) = 1.5, so both integers and decimals work.
   (Link with -lm on GCC.) The result has the sign of the left operand. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "evaluator.h"
#include "../validation/bracket_validator.h"
#include "../validation/syntax_validator.h"
#include "../notation/notation_detector.h"
#include "../notation/conversion.h"

/* ------------------------------------------------------------------ */
/* Private numeric stack                                              */
/* ------------------------------------------------------------------ */

typedef struct {
    double *data;
    int     top;          /* index of the top element, -1 when empty */
    int     capacity;
} NumStack;

static int nstack_init(NumStack *s, int capacity)
{
    s->data = (double *)malloc((size_t)capacity * sizeof(double));
    s->top = -1;
    s->capacity = (s->data != NULL) ? capacity : 0;
    return s->data != NULL ? 0 : -1;
}

static void nstack_free(NumStack *s)
{
    free(s->data);
    s->data = NULL;
    s->top = -1;
    s->capacity = 0;
}

static int nstack_size(const NumStack *s) { return s->top + 1; }

static int nstack_push(NumStack *s, double v)
{
    if (s->top >= s->capacity - 1) return -1;
    s->data[++s->top] = v;
    return 0;
}

static int nstack_pop(NumStack *s, double *v)
{
    if (s->top < 0) return -1;
    *v = s->data[s->top--];
    return 0;
}

/* prints  "Stack: [25, 15]"  (bottom ... top) */
static void nstack_print(const NumStack *s)
{
    int i;
    printf("     Stack: [");
    for (i = 0; i <= s->top; i++) {
        printf("%s%g", i > 0 ? ", " : "", s->data[i]);
    }
    printf("]\n");
}

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

static EvaluationResult make_success(double value)
{
    EvaluationResult r;
    memset(&r, 0, sizeof r);
    r.success = 1;
    r.status  = EVAL_OK;
    r.value   = value;
    snprintf(r.message, sizeof r.message, "Evaluation successful");
    return r;
}

static EvaluationResult make_failure(EvalStatus status, const char *msg)
{
    EvaluationResult r;
    memset(&r, 0, sizeof r);
    r.success = 0;
    r.status  = status;
    r.value   = 0.0;
    snprintf(r.message, sizeof r.message, "%s", msg);
    return r;
}

static int is_supported_operator(char op)
{
    return op == '+' || op == '-' || op == '*' || op == '/' ||
           op == '%' || op == '^';
}

/* Computes  a op b.  Returns EVAL_OK or the reason it failed (and fills msg). */
static EvalStatus apply_operator(char op, double a, double b,
                                 double *out, char *msg, size_t msg_size)
{
    double r = 0.0;

    switch (op) {
        case '+': r = a + b; break;
        case '-': r = a - b; break;
        case '*': r = a * b; break;
        case '/':
            if (b == 0.0) {
                snprintf(msg, msg_size, "Division by zero");
                return EVAL_DIVISION_BY_ZERO;
            }
            r = a / b;
            break;
        case '%':
            if (b == 0.0) {
                snprintf(msg, msg_size, "Modulo by zero");
                return EVAL_DIVISION_BY_ZERO;
            }
            r = fmod(a, b);
            break;
        case '^': r = pow(a, b); break;
        default:
            snprintf(msg, msg_size, "Invalid operator '%c'", op);
            return EVAL_INVALID_EXPRESSION;
    }

    if (!isfinite(r)) {
        snprintf(msg, msg_size, "Result is too large or not a number");
        return EVAL_ERROR;
    }
    *out = r;
    return EVAL_OK;
}

/* First variable (left to right) that has no value, or 0 if none. */
static char find_missing_variable(const Token tokens[], int count,
                                  const VariableTable *vars)
{
    int i;
    for (i = 0; i < count; i++) {
        if (tokens[i].type == TOKEN_VARIABLE &&
            !var_exists(vars, tokens[i].text[0])) {
            return tokens[i].text[0];
        }
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Core algorithm shared by postfix and prefix                        */
/* ------------------------------------------------------------------ */

/* from_right == 0 : POSTFIX, scan left -> right
   from_right == 1 : PREFIX,  scan right -> left

   Both use the same rule:
     operand  -> push its value
     operator -> pop two values, calculate, push the answer

   Only ONE thing differs: which popped value is the LEFT operand.
     postfix: first pop  = RIGHT operand   (25 15 -  ->  25 - 15)
     prefix : first pop  = LEFT  operand   (- 25 15  ->  25 - 15)         */
static EvaluationResult evaluate_linear(const Token tokens[], int count,
                                        const VariableTable *vars,
                                        int from_right, int show_steps)
{
    NumStack stack;
    EvaluationResult result;
    char msg[EVAL_MESSAGE_LEN];
    char missing;
    int  i, step = 0;
    double answer;

    if (tokens == NULL || count <= 0) {
        return make_failure(EVAL_INVALID_EXPRESSION, "Empty expression");
    }

    missing = find_missing_variable(tokens, count, vars);
    if (missing != 0) {
        snprintf(msg, sizeof msg, "Missing value for %c", missing);
        return make_failure(EVAL_MISSING_VARIABLE, msg);
    }

    if (nstack_init(&stack, count) != 0) {
        return make_failure(EVAL_ERROR, "Out of memory");
    }

    for (i = from_right ? count - 1 : 0;
         from_right ? i >= 0 : i < count;
         i += from_right ? -1 : 1) {
        const Token *t = &tokens[i];

        if (t->type == TOKEN_NUMBER || t->type == TOKEN_VARIABLE) {
            double v;
            if (t->type == TOKEN_NUMBER) {
                v = strtod(t->text, NULL);
            } else if (var_get(vars, t->text[0], &v) != VAR_OK) {
                result = make_failure(EVAL_MISSING_VARIABLE, "Missing variable value");
                goto done;
            }
            if (nstack_push(&stack, v) != 0) {
                result = make_failure(EVAL_ERROR, "Invalid evaluation state (stack overflow)");
                goto done;
            }
            if (show_steps) {
                step++;
                if (t->type == TOKEN_VARIABLE) {
                    printf("  %d. Push %s (= %g)\n", step, t->text, v);
                } else {
                    printf("  %d. Push %g\n", step, v);
                }
                nstack_print(&stack);
            }
        } else if (t->type == TOKEN_OPERATOR) {
            double first, second, a, b, r;
            char   op = t->text[0];
            EvalStatus st;

            if (t->text[1] != '\0' || !is_supported_operator(op)) {
                snprintf(msg, sizeof msg, "Invalid operator '%s'", t->text);
                result = make_failure(EVAL_INVALID_EXPRESSION, msg);
                goto done;
            }
            if (nstack_size(&stack) < 2) {
                snprintf(msg, sizeof msg,
                         "Too few operands for operator '%c'", op);
                result = make_failure(EVAL_INVALID_EXPRESSION, msg);
                goto done;
            }
            nstack_pop(&stack, &first);
            nstack_pop(&stack, &second);
            if (from_right) { a = first;  b = second; }   /* prefix  */
            else            { a = second; b = first;  }   /* postfix */

            st = apply_operator(op, a, b, &r, msg, sizeof msg);
            if (st != EVAL_OK) {
                if (show_steps) {
                    step++;
                    printf("  %d. %c  Pop %g and %g\n     Cannot calculate %g %c %g\n",
                           step, op, first, second, a, op, b);
                }
                result = make_failure(st, msg);
                goto done;
            }
            nstack_push(&stack, r);   /* cannot overflow: we just popped two */

            if (show_steps) {
                step++;
                printf("  %d. %c  Pop %g and %g\n", step, op, first, second);
                printf("     Calculate %g %c %g = %g, Push %g\n", a, op, b, r, r);
                nstack_print(&stack);
            }
        } else {
            snprintf(msg, sizeof msg, "Invalid token '%s' in %s expression",
                     t->text, from_right ? "prefix" : "postfix");
            result = make_failure(EVAL_INVALID_EXPRESSION, msg);
            goto done;
        }
    }

    if (nstack_size(&stack) > 1) {
        result = make_failure(EVAL_INVALID_EXPRESSION,
                              "Too many operands (an operator is missing)");
    } else if (nstack_size(&stack) < 1) {
        result = make_failure(EVAL_ERROR, "Invalid evaluation state");
    } else {
        nstack_pop(&stack, &answer);
        if (show_steps) {
            printf("  Final result = %g\n", answer);
        }
        result = make_success(answer);
    }

done:
    nstack_free(&stack);
    return result;
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

EvaluationResult evaluate_postfix(const Token tokens[], int count,
                                  const VariableTable *vars, int show_steps)
{
    return evaluate_linear(tokens, count, vars, 0, show_steps);
}

EvaluationResult evaluate_prefix(const Token tokens[], int count,
                                 const VariableTable *vars, int show_steps)
{
    return evaluate_linear(tokens, count, vars, 1, show_steps);
}

/* INFIX -> POSTFIX (Batch 3, conversion.c) -> POSTFIX evaluation.
   infix_to_postfix() gives us TEXT, so we tokenize that text again and hand
   the tokens to evaluate_postfix(). */
EvaluationResult evaluate_infix(const Token tokens[], int count,
                                const VariableTable *vars, int show_steps)
{
    ValidationResult v;
    Token  post[MAX_TOKENS];
    char   text[EVAL_POSTFIX_LEN];
    EvaluationResult result;
    int    n;

    if (tokens == NULL || count <= 0) {
        return make_failure(EVAL_INVALID_EXPRESSION, "Empty expression");
    }

    v = validate_brackets(tokens, count);          /* Batch 2 */
    if (!v.valid) {
        return make_failure(EVAL_INVALID_EXPRESSION, v.message);
    }
    v = validate_syntax(tokens, count);            /* Batch 2 */
    if (!v.valid) {
        return make_failure(EVAL_INVALID_EXPRESSION, v.message);
    }

    if (infix_to_postfix(tokens, count, text, sizeof text) != CONV_OK) {
        return make_failure(EVAL_ERROR, "Could not convert infix to postfix");
    }
    n = tokenize(text, post, MAX_TOKENS);
    if (n <= 0) {
        return make_failure(EVAL_ERROR, "Could not read the converted postfix");
    }

    if (show_steps) {
        printf("  Converted postfix: %s\n", text);
    }
    result = evaluate_postfix(post, n, vars, show_steps);
    snprintf(result.postfix, sizeof result.postfix, "%s", text);
    return result;
}

EvaluationResult evaluate_tokens(const Token tokens[], int count,
                                 const VariableTable *vars, int show_steps)
{
    NotationType type;

    if (tokens == NULL || count <= 0) {
        return make_failure(EVAL_INVALID_EXPRESSION, "Empty expression");
    }
    type = detect_notation(tokens, count);         /* Batch 3 */
    switch (type) {
        case NOTATION_INFIX:
            return evaluate_infix(tokens, count, vars, show_steps);
        case NOTATION_PREFIX:
            return evaluate_prefix(tokens, count, vars, show_steps);
        case NOTATION_POSTFIX:
        case NOTATION_AMBIGUOUS:      /* a single operand: its own value */
            return evaluate_postfix(tokens, count, vars, show_steps);
        default:
            return make_failure(EVAL_INVALID_EXPRESSION,
                                "Not a valid infix, prefix or postfix expression");
    }
}

const char *eval_status_name(EvalStatus status)
{
    switch (status) {
        case EVAL_OK:                 return "OK";
        case EVAL_INVALID_EXPRESSION: return "INVALID_EXPRESSION";
        case EVAL_MISSING_VARIABLE:   return "MISSING_VARIABLE";
        case EVAL_DIVISION_BY_ZERO:   return "DIVISION_BY_ZERO";
        default:                      return "ERROR";
    }
}
