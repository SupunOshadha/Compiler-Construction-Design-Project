# Compiler Construction Design Project (EEX6363)

This repository contains my final year design project for **Compiler Construction** at The Open University of Sri Lanka.  
The project implements the **code generation phase** of a compiler, completing the pipeline from tokens → AST → semantic analysis → TAC output.

## ✨ Features
- Register allocation using virtual temporaries (`t0`, `t1`, …)
- Stack-based memory management with offsets for variables and arrays
- TAC generation for:
  - Declarations and assignments
  - Arithmetic and logical expressions
  - Control flow (if, while)
  - Function calls and returns
  - Input/Output operations
- Symbol table and error reporting

## 📂 Project Structure
