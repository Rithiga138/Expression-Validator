@echo off
REM Builds and runs every test without needing "make". Run from this folder.
if not exist build mkdir build

set E=dsa_engine
set TOK=%E%\tokenizer\tokenizer.c
set STK=%E%\stack\stack.c
set VAL=%E%\validation\bracket_validator.c %E%\validation\syntax_validator.c
set NOT=%E%\notation\notation_detector.c %E%\notation\conversion.c
set EVL=%E%\evaluation\evaluator.c %E%\evaluation\variable_manager.c
set ANL=%E%\analyzer\expression_analyzer.c %E%\analyzer\diagnosis.c %E%\analyzer\correction_generator.c
set F=-Wall -Wextra -std=c99 -g

gcc %F% -o build\test_stack.exe      %E%\stack\test_stack.c %STK%                                   || goto :err
gcc %F% -o build\test_tokenizer.exe  %E%\tokenizer\test_tokenizer.c %TOK%                           || goto :err
gcc %F% -o build\test_validation.exe %E%\validation\test_validation.c %TOK% %STK% %VAL%             || goto :err
gcc %F% -o build\test_notation.exe   %E%\notation\test_notation.c %TOK% %STK% %VAL% %NOT% -lm       || goto :err
gcc %F% -o build\test_evaluation.exe %E%\evaluation\test_evaluation.c %TOK% %STK% %VAL% %NOT% %EVL% -lm  || goto :err
gcc %F% -o build\test_analyzer.exe   %E%\analyzer\test_analyzer.c %TOK% %STK% %VAL% %NOT% %EVL% %ANL% -lm || goto :err
gcc %F% -o build\evaluator.exe       %E%\main.c %TOK% %STK% %VAL% %NOT% -lm                         || goto :err

gcc %F% -o build\analyzer_cli.exe    %E%\integration\analyzer_cli.c %TOK% %STK% %VAL% %NOT% %EVL% %ANL% -lm || goto :err

build\test_stack.exe
build\test_tokenizer.exe
build\test_validation.exe
build\test_notation.exe
build\test_evaluation.exe
build\test_analyzer.exe
echo.
echo Interactive program: build\evaluator.exe
goto :eof

:err
echo.
echo BUILD FAILED - see the error above.
exit /b 1
