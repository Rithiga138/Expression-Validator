/* variable_manager.c - simple array-based variable table */
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "variable_manager.h"

/* 'A'..'Z' -> 0..25, 'a'..'z' -> 26..51, anything else -> -1 */
static int var_index(char name)
{
    if (name >= 'A' && name <= 'Z') return name - 'A';
    if (name >= 'a' && name <= 'z') return 26 + (name - 'a');
    return -1;
}

void var_table_init(VariableTable *table)
{
    int i;
    if (table == NULL) return;
    for (i = 0; i < VAR_TABLE_SIZE; i++) {
        table->is_set[i] = 0;
        table->value[i]  = 0.0;
    }
}

int var_set(VariableTable *table, char name, double value)
{
    int idx = var_index(name);
    if (table == NULL || idx < 0 || !isfinite(value)) return VAR_FAIL;
    table->is_set[idx] = 1;
    table->value[idx]  = value;
    return VAR_OK;
}

int var_exists(const VariableTable *table, char name)
{
    int idx = var_index(name);
    return (table != NULL && idx >= 0 && table->is_set[idx]) ? 1 : 0;
}

int var_get(const VariableTable *table, char name, double *value)
{
    int idx = var_index(name);
    if (table == NULL || value == NULL || idx < 0 || !table->is_set[idx]) {
        return VAR_FAIL;
    }
    *value = table->value[idx];
    return VAR_OK;
}

int var_collect(const Token tokens[], int count, char names[], int max_names)
{
    int seen[VAR_TABLE_SIZE] = {0};
    int i, n = 0;

    if (tokens == NULL || names == NULL || count <= 0 || max_names <= 0) {
        return 0;
    }
    for (i = 0; i < count && n < max_names; i++) {
        int idx;
        if (tokens[i].type != TOKEN_VARIABLE) continue;
        idx = var_index(tokens[i].text[0]);
        if (idx < 0 || seen[idx]) continue;
        seen[idx] = 1;
        names[n++] = tokens[i].text[0];
    }
    return n;
}

int var_prompt_missing(VariableTable *table, const Token tokens[], int count)
{
    char names[VAR_TABLE_SIZE];
    int  n, i;

    if (table == NULL || tokens == NULL) return VAR_FAIL;
    n = var_collect(tokens, count, names, VAR_TABLE_SIZE);

    for (i = 0; i < n; i++) {
        char line[128];
        if (var_exists(table, names[i])) continue;      /* already known */

        for (;;) {
            char  *end;
            double v;

            printf("Enter value for %c: ", names[i]);
            fflush(stdout);
            if (fgets(line, sizeof line, stdin) == NULL) return VAR_FAIL;

            v = strtod(line, &end);
            if (end != line) {
                while (isspace((unsigned char)*end)) end++;
                if (*end == '\0' && var_set(table, names[i], v) == VAR_OK) {
                    break;                               /* good value */
                }
            }
            printf("  Not a valid number, please try again.\n");
        }
    }
    return VAR_OK;
}
