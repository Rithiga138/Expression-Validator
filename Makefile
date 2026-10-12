CC      = gcc
CFLAGS  = -Wall -Wextra -std=c99 -g
E       = dsa_engine
STACK   = $(E)/stack/stack.c
TOK     = $(E)/tokenizer/tokenizer.c
VAL     = $(E)/validation/bracket_validator.c $(E)/validation/syntax_validator.c
NOT     = $(E)/notation/notation_detector.c $(E)/notation/conversion.c
EVAL    = $(E)/evaluation/evaluator.c $(E)/evaluation/variable_manager.c
ANALYZE = $(E)/analyzer/expression_analyzer.c $(E)/analyzer/diagnosis.c $(E)/analyzer/correction_generator.c
BIN     = build

all: $(BIN)/test_stack $(BIN)/test_tokenizer $(BIN)/test_validation $(BIN)/test_notation $(BIN)/test_evaluation $(BIN)/test_analyzer $(BIN)/evaluator $(BIN)/analyzer_cli

$(BIN):
	mkdir -p $(BIN)

$(BIN)/test_stack: $(E)/stack/test_stack.c $(STACK) | $(BIN)
	$(CC) $(CFLAGS) -o $@ $^

$(BIN)/test_tokenizer: $(E)/tokenizer/test_tokenizer.c $(TOK) | $(BIN)
	$(CC) $(CFLAGS) -o $@ $^

$(BIN)/test_validation: $(E)/validation/test_validation.c $(TOK) $(STACK) $(VAL) | $(BIN)
	$(CC) $(CFLAGS) -o $@ $^

$(BIN)/test_notation: $(E)/notation/test_notation.c $(TOK) $(STACK) $(VAL) $(NOT) | $(BIN)
	$(CC) $(CFLAGS) -o $@ $^ -lm

$(BIN)/test_evaluation: $(E)/evaluation/test_evaluation.c $(TOK) $(STACK) $(VAL) $(NOT) $(EVAL) | $(BIN)
	$(CC) $(CFLAGS) -o $@ $^ -lm

$(BIN)/test_analyzer: $(E)/analyzer/test_analyzer.c $(TOK) $(STACK) $(VAL) $(NOT) $(EVAL) $(ANALYZE) | $(BIN)
	$(CC) $(CFLAGS) -o $@ $^ -lm

$(BIN)/evaluator: $(E)/main.c $(TOK) $(STACK) $(VAL) $(NOT) | $(BIN)
	$(CC) $(CFLAGS) -o $@ $^ -lm

$(BIN)/analyzer_cli: $(E)/integration/analyzer_cli.c $(TOK) $(STACK) $(VAL) $(NOT) $(EVAL) $(ANALYZE) | $(BIN)
	$(CC) $(CFLAGS) -o $@ $^ -lm

engine: $(BIN)/analyzer_cli

test: all
	./$(BIN)/test_stack && ./$(BIN)/test_tokenizer && ./$(BIN)/test_validation && ./$(BIN)/test_notation && ./$(BIN)/test_evaluation && ./$(BIN)/test_analyzer

clean:
	rm -rf $(BIN)

.PHONY: all test clean engine
