/* variable_manager.h - stores the numeric values of single-letter variables

   A variable is one letter: A-Z or a-z. 'A' and 'a' are DIFFERENT variables,
   exactly like the tokenizer (every letter becomes its own TOKEN_VARIABLE).
   The table is a plain array of 52 slots, so every lookup is O(1). */
#ifndef VARIABLE_MANAGER_H
#define VARIABLE_MANAGER_H

#include "../tokenizer/tokenizer.h"    /* Token, TOKEN_VARIABLE (Batch 1) */

#define VAR_OK          0
#define VAR_FAIL       -1
#define VAR_TABLE_SIZE 52              /* 26 uppercase + 26 lowercase */

typedef struct {
    int    is_set[VAR_TABLE_SIZE];     /* 1 = this variable has a value */
    double value[VAR_TABLE_SIZE];
} VariableTable;

/* Marks every variable as "no value yet". */
void var_table_init(VariableTable *table);

/* Stores a value. Returns VAR_FAIL if name is not a letter, the value is
   not a finite number, or table is NULL. Setting again overwrites. */
int  var_set(VariableTable *table, char name, double value);

/* Returns 1 if the variable has a value, otherwise 0. */
int  var_exists(const VariableTable *table, char name);

/* Copies the value into *value. Returns VAR_FAIL if it has no value. */
int  var_get(const VariableTable *table, char name, double *value);

/* Writes the DISTINCT variable names used in tokens[] into names[], in the
   order of first appearance (left to right). Returns how many were written
   (at most max_names). */
int  var_collect(const Token tokens[], int count, char names[], int max_names);

/* Console helper: asks "Enter value for X: " for every variable used in
   tokens[] that has no value yet. Re-asks on bad input.
   Returns VAR_OK, or VAR_FAIL if input ended (EOF) or arguments are bad. */
int  var_prompt_missing(VariableTable *table, const Token tokens[], int count);

#endif
