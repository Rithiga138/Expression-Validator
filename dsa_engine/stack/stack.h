/* stack.h - fixed-capacity character stack */
#ifndef STACK_H
#define STACK_H

#define STACK_OK    0
#define STACK_FAIL -1

typedef struct {
    char *data;
    int   top;        /* index of the top element, -1 when empty */
    int   capacity;
} Stack;

int  stack_init(Stack *s, int capacity);
void stack_free(Stack *s);
int  stack_is_empty(const Stack *s);
int  stack_is_full(const Stack *s);
int  stack_push(Stack *s, char value);
int  stack_pop(Stack *s, char *value);
int  stack_peek(const Stack *s, char *value);
void stack_display(const Stack *s);

#endif
