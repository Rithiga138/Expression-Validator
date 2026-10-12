# Expression Validator & Evaluator (C)

Validates infix / prefix / postfix expressions, converts between them and evaluates them.

## Run in VS Code (Windows)
1. Install MinGW-w64 / MSYS2 so `gcc --version` works in the terminal.
2. Open the project folder (the one containing `dsa_engine`) in VS Code.
3. In the terminal run: `.\run_tests.bat`  (or `make test` if you have make)
4. Interactive program: `.\build\evaluator.exe`

Do NOT compile a single test file alone (e.g. `gcc test_stack.c`): each test needs the
matching .c files too, otherwise you get "undefined reference" linker errors.

## Layout
- stack/      character stack
- tokenizer/  string -> tokens
- validation/ bracket_validator + syntax_validator
- notation/   notation_detector + conversion
- evaluation/ evaluator (postfix/prefix/infix) + variable_manager
- analyzer/   expression_analyzer (master pipeline) + diagnosis + correction_generator
- dsa_engine/main.c  interactive program

## Web app (frontend + Flask + C engine)
1. Build the engine adapter:  `build_engine.bat`  (creates `build\analyzer_cli.exe`)
2. Terminal 1:  `cd backend`  then  `pip install -r requirements.txt`  then  `python app.py`   (http://127.0.0.1:5000)
3. Terminal 2:  `cd frontend`  then  `python -m http.server 5500`   (http://127.0.0.1:5500)
4. Check http://127.0.0.1:5000/api/health, then open http://127.0.0.1:5500

Flow: browser -> Flask (`backend/`) -> `build\analyzer_cli.exe` (adapter in
`dsa_engine/integration/`) -> existing C modules -> JSON -> Flask -> browser.
