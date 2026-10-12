/* test_stack.c - tests for the character stack */
#include <stdio.h>
#include "stack.h"

static int failures = 0;

static void check(const char *label, int condition)
{
    if (!condition) failures++;
    printf("  [%s] %s\n", condition ? "PASS" : "FAIL", label);
}

int main(void)
{
    Stack s;
    char value;

    printf("=== STACK TESTS (capacity 3) ===\n\n");

    printf("TEST 1: Initialization\n");
    stack_init(&s, 3);
    check("new stack is empty", stack_is_empty(&s) == 1);
    check("new stack is not full", stack_is_full(&s) == 0);
    printf("\n");

    printf("TEST 2: Display empty stack\n");
    stack_display(&s);
    printf("\n");

    printf("TEST 3: Push and peek\n");
    check("push '(' succeeds", stack_push(&s, '(') == STACK_OK);
    check("peek returns '('", stack_peek(&s, &value) == STACK_OK && value == '(');
    check("stack is not empty", stack_is_empty(&s) == 0);
    printf("\n");

    printf("TEST 4: Multiple pushes\n");
    check("push '+' succeeds", stack_push(&s, '+') == STACK_OK);
    check("push '*' succeeds", stack_push(&s, '*') == STACK_OK);
    stack_display(&s);
    printf("\n");

    printf("TEST 5: isFull\n");
    check("stack is full", stack_is_full(&s) == 1);
    printf("\n");

    printf("TEST 6: Overflow\n");
    check("push ')' fails on full stack", stack_push(&s, ')') == STACK_FAIL);
    check("top is still '*'", stack_peek(&s, &value) == STACK_OK && value == '*');
    printf("\n");

    printf("TEST 7: Peek does not remove\n");
    check("first peek is '*'", stack_peek(&s, &value) == STACK_OK && value == '*');
    check("second peek is '*'", stack_peek(&s, &value) == STACK_OK && value == '*');
    check("stack is still full", stack_is_full(&s) == 1);
    printf("\n");

    printf("TEST 8: Pop (LIFO order)\n");
    check("pop gives '*'", stack_pop(&s, &value) == STACK_OK && value == '*');
    check("pop gives '+'", stack_pop(&s, &value) == STACK_OK && value == '+');
    check("pop gives '('", stack_pop(&s, &value) == STACK_OK && value == '(');
    printf("\n");

    printf("TEST 9: Underflow and peek on empty\n");
    check("stack is empty again", stack_is_empty(&s) == 1);
    check("pop on empty fails", stack_pop(&s, &value) == STACK_FAIL);
    check("peek on empty fails", stack_peek(&s, &value) == STACK_FAIL);
    stack_display(&s);
    printf("\n");

    printf("TEST 10: Stack works after underflow\n");
    check("push ')' succeeds", stack_push(&s, ')') == STACK_OK);
    stack_display(&s);

    stack_free(&s);
    printf("\n%s (%d failure(s))\n", failures ? "SOME TESTS FAILED" : "ALL TESTS PASSED", failures);
    return failures ? 1 : 0;
}