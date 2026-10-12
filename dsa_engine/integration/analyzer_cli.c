/* analyzer_cli.c - thin command-line adapter around the EXISTING C engine.

   This file contains NO algorithm. It only:
     1. reads the request from stdin,
     2. calls analyze_expression() from analyzer/expression_analyzer.c,
     3. prints the ExpressionAnalysisResult as one JSON object (or, in
        --trace mode, lets the evaluator print its own stack trace).

   Request format on stdin (plain text, so no shell-quoting problems):
       line 1        : the expression, exactly as typed by the user
       further lines : one variable per line, e.g.   A=10

   Usage:
       analyzer_cli --json    < request.txt    prints the analysis as JSON
       analyzer_cli --trace   < request.txt    prints the evaluator's step trace
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "../analyzer/expression_analyzer.h"

#define LINE_MAX_LEN 1024

/* ---------- JSON helpers (formatting only) ---------- */

static void json_string(const char *s)
{
    putchar('"');
    for (; s && *s; s++) {
        unsigned char c = (unsigned char)*s;
        switch (c) {
            case '"':  fputs("\\\"", stdout); break;
            case '\\': fputs("\\\\", stdout); break;
            case '\n': fputs("\\n", stdout);  break;
            case '\r': fputs("\\r", stdout);  break;
            case '\t': fputs("\\t", stdout);  break;
            default:
                if (c < 0x20) printf("\\u%04x", c);
                else putchar(c);   /* bytes >= 0x80 are passed through untouched */
        }
    }
    putchar('"');
}

/* "" -> null, otherwise a JSON string */
static void json_string_or_null(const char *s)
{
    if (s == NULL || s[0] == '\0') fputs("null", stdout);
    else json_string(s);
}

static void json_number(double v)
{
    if (!isfinite(v)) fputs("null", stdout);
    else printf("%.15g", v);
}

/* ---------- request parsing ---------- */

static void strip_newline(char *line)
{
    line[strcspn(line, "\r\n")] = '\0';
}

/* Reads "A=10" lines into the engine's own VariableTable. */
static int read_variables(VariableTable *table)
{
    char line[LINE_MAX_LEN];
    while (fgets(line, sizeof line, stdin) != NULL) {
        char *eq, *end;
        double value;
        strip_newline(line);
        if (line[0] == '\0') continue;
        eq = strchr(line, '=');
        if (eq == NULL || eq == line || eq[1] == '\0' ||
            !isalpha((unsigned char)line[0]) || eq != line + 1) {
            fprintf(stderr, "bad variable line: %s\n", line);
            return -1;
        }
        value = strtod(eq + 1, &end);
        if (*end != '\0' || var_set(table, line[0], value) != VAR_OK) {
            fprintf(stderr, "bad variable value: %s\n", line);
            return -1;
        }
    }
    return 0;
}

/* ---------- JSON output ---------- */

static void print_json(const ExpressionAnalysisResult *r, const VariableTable *vars)
{
    int i, first;

    fputs("{", stdout);
    fputs("\"expression\":", stdout);      json_string(r->expression);
    printf(",\"valid\":%s", r->valid ? "true" : "false");
    fputs(",\"notation\":", stdout);       json_string(notation_name(r->notation));
    fputs(",\"infix\":", stdout);          json_string_or_null(r->infix);
    fputs(",\"postfix\":", stdout);        json_string_or_null(r->postfix);
    fputs(",\"prefix\":", stdout);         json_string_or_null(r->prefix);

    /* result: only when the engine's evaluation really succeeded */
    fputs(",\"result\":", stdout);
    if (r->evaluation_attempted && r->evaluation.success) json_number(r->evaluation.value);
    else fputs("null", stdout);

    fputs(",\"evaluation\":{", stdout);
    printf("\"attempted\":%s", r->evaluation_attempted ? "true" : "false");
    if (r->evaluation_attempted) {
        printf(",\"success\":%s", r->evaluation.success ? "true" : "false");
        fputs(",\"status\":", stdout);  json_string(eval_status_name(r->evaluation.status));
        fputs(",\"message\":", stdout); json_string(r->evaluation.message);
    }
    fputs("}", stdout);

    /* diagnosis */
    fputs(",\"diagnosis\":", stdout);
    if (r->diagnosis.category == DIAG_NONE) {
        fputs("null", stdout);
    } else {
        fputs("{\"category\":", stdout);
        json_string(diagnosis_category_name(r->diagnosis.category));
        fputs(",\"message\":", stdout);   json_string(r->diagnosis.message);
        if (r->diagnosis.position >= 0) printf(",\"position\":%d", r->diagnosis.position);
        else fputs(",\"position\":null", stdout);
        fputs(",\"expected\":", stdout);  json_string_or_null(r->diagnosis.expected);
        fputs(",\"found\":", stdout);     json_string_or_null(r->diagnosis.found);
        fputs("}", stdout);
    }

    /* corrections */
    fputs(",\"corrections\":[", stdout);
    for (i = 0; i < r->correction_count; i++) {
        if (i > 0) putchar(',');
        fputs("{\"expression\":", stdout); json_string(r->corrections[i].expression);
        fputs(",\"reason\":", stdout);     json_string(r->corrections[i].reason);
        fputs("}", stdout);
    }
    fputs("]", stdout);

    /* every variable the expression uses, and the ones still lacking a value */
    fputs(",\"variables_used\":[", stdout);
    for (i = 0; i < r->variable_count; i++) {
        char one[2] = { r->variables[i], '\0' };
        if (i > 0) putchar(',');
        json_string(one);
    }
    fputs("],\"variables_required\":[", stdout);
    first = 1;
    for (i = 0; i < r->variable_count; i++) {
        if (!var_exists(vars, r->variables[i])) {
            char one[2] = { r->variables[i], '\0' };
            if (!first) putchar(',');
            json_string(one);
            first = 0;
        }
    }
    fputs("]}\n", stdout);
}

int main(int argc, char **argv)
{
    char expr[LINE_MAX_LEN];
    VariableTable vars;
    int trace = (argc > 1 && strcmp(argv[1], "--trace") == 0);
    ExpressionAnalysisResult r;

    if (argc < 2 || (!trace && strcmp(argv[1], "--json") != 0)) {
        fprintf(stderr, "usage: analyzer_cli --json|--trace  (request on stdin)\n");
        return 2;
    }
    if (fgets(expr, sizeof expr, stdin) == NULL) {
        fprintf(stderr, "no expression on stdin\n");
        return 2;
    }
    strip_newline(expr);

    var_table_init(&vars);
    if (read_variables(&vars) != 0) return 2;

    /* The engine's own show_steps flag makes evaluator.c print the trace. */
    r = analyze_expression(expr, &vars, trace ? 1 : 0);

    if (!trace) print_json(&r, &vars);
    return 0;
}
