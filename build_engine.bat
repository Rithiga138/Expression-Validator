@echo off
REM Compiles ONLY the C engine adapter used by the Flask backend.
REM Output: build\analyzer_cli.exe   (run from the project root)
cd /d "%~dp0"
if not exist build mkdir build
set E=dsa_engine
gcc -Wall -Wextra -std=c99 -O2 -o build\analyzer_cli.exe ^
 %E%\integration\analyzer_cli.c ^
 %E%\tokenizer\tokenizer.c %E%\stack\stack.c ^
 %E%\validation\bracket_validator.c %E%\validation\syntax_validator.c ^
 %E%\notation\notation_detector.c %E%\notation\conversion.c ^
 %E%\evaluation\evaluator.c %E%\evaluation\variable_manager.c ^
 %E%\analyzer\expression_analyzer.c %E%\analyzer\diagnosis.c %E%\analyzer\correction_generator.c ^
 -lm
if errorlevel 1 (echo BUILD FAILED & exit /b 1)
echo Built build\analyzer_cli.exe
