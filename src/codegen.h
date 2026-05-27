#ifndef CODEGEN_H
#define CODEGEN_H

#include <stdio.h>
#include "ast.h"

void init_codegen(const char *filename);

void generate_code(ASTNode *program_root);

void finish_codegen(void);

#endif 
