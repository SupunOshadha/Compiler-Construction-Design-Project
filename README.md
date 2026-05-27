# Compiler Construction Design Project (EEX6363)

This repository contains my final year design project for **Compiler Construction** at The Open University of Sri Lanka.  
The project implements the **code generation phase** of a compiler, completing the pipeline from tokens → AST → semantic analysis → TAC output.

## ✨ Features
- Register allocation using virtual temporaries (`t0`, `t1`, …)
- Stack-based memory management with offsets for variables and arrays
- TAC generation for:
  - Declarations and assignments
  - Arithmetic and logical expressions
  - Control flow (`if`, `while`)
  - Function calls and returns
  - Input/Output operations
- Symbol table and error reporting
- Multiple output artifacts for examiner review:
  - `output.tac` → generated Three Address Code
  - `symtab.txt` → symbol table dump
  - `errors.txt` → semantic/syntactic error log
  - `derivation.txt` → parser derivation trace

## 📂 Project Structure

src/        # source code (scanner.l, parser.c, ast.c, codegen.c, headers)
docs/       # final design project report and screenshots
tests/      # sample input programs (.txt/.src)
README.md   # project overview
Code


## ⚙️ Build Instructions
1. Generate the scanner:
   ```bash
   flex scanner.l

This produces lex.yy.c.

    Compile the compiler:
    bash

    gcc -o compiler lex.yy.c parser.c ast.c codegen.c

    Run with a source program:
    bash

    ./compiler source1.txt
    ./compiler test.txt

🧪 Example Output

Source program:
pascal

integer x;
x := 5;
write(x);

Generated TAC (output.tac):
Code

; DECL x slots=1 offset=0
LOADI t0, 5
STORE t0 -> [SP + 0]
LOAD t1, [SP + 0]
WRITE t1
; Stack frame size: 4 bytes
; var x -> [SP + 0] (slots=1)

Other outputs:

    symtab.txt → contains x : integer array=0 scope=0

    errors.txt → empty (no errors)

    derivation.txt → shows grammar productions used

📜 Documentation

    Full design project report included in /docs

    Screenshots of code sections and TAC outputs

👨‍💻 Author

    Supun Oshadha

    Final-year Software Engineering Undergraduate, OUSL