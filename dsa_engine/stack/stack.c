/* stack.c - fixed-capacity character stack */
#include <stdio.h>
#include <stdlib.h>
#include "stack.h"

int stack_init(Stack *s, int capacity)
{
    if (s == NULL || capacity <= 0) {
        return STACK_FAIL;
    }
    s->data = (char *)malloc((size_t)capacity);
    if (s->data == NULL) {
        s->top = -1;
        s->capacity = 0;
        return STACK_FAIL;
    }
    s->top = -1;
    s->capacity = capacity;
    return STACK_OK;
}

void stack_free(Stack *s)
{
    if (s != NULL) {
        free(s->data);
        s->data = NULL;
        s->top = -1;
        s->capacity = 0;
    }
}

int stack_is_empty(const Stack *s)
{
    return (s == NULL || s->top < 0) ? 1 : 0;
}

int stack_is_full(const Stack *s)
{
    return (s != NULL && s->top >= s->capacity - 1) ? 1 : 0;
}

int stack_push(Stack *s, char value)
{
    if (s == NULL || s->data == NULL || stack_is_full(s)) {
        return STACK_FAIL;
    }
    s->data[++s->top] = value;
    return STACK_OK;
}

int stack_pop(Stack *s, char *value)
{
    if (s == NULL || s->data == NULL || stack_is_empty(s)) {
        return STACK_FAIL;
    }
    if (value != NULL) {
        *value = s->data[s->top];
    }
    s->top--;
    return STACK_OK;
}

int stack_peek(const Stack *s, char *value)
{
    if (s == NULL || s->data == NULL || stack_is_empty(s)) {
        return STACK_FAIL;
    }
    if (value != NULL) {
        *value = s->data[s->top];
    }
    return STACK_OK;
}

void stack_display(const Stack *s)
{
    int i;
    if (stack_is_empty(s)) {
        printf("  Stack: (empty)\n");
        return;
    }
    printf("  Stack (top -> bottom): ");
    for (i = s->top; i >= 0; i--) {
        printf("%c ", s->data[i]);
    }
    printf("\n");
}
