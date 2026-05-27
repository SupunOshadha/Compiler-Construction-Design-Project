#ifndef AST_H
#define AST_H

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include "tokens.h" 

typedef enum { TYPE_INT, TYPE_FLOAT, TYPE_BOOL, TYPE_VOID } Type;

typedef struct ASTNode ASTNode;

typedef struct Symbol {
    char* name;
    Type type;
    bool is_array;
    int array_size;
    int scope; 
    struct Symbol* next;
} Symbol;

typedef struct SymbolTable {
    Symbol* head;
    int current_scope;
} SymbolTable;

typedef struct Error {
    int line, column;
    char* msg;
    struct Error* next;
} Error;

struct ASTNode {
    ASTNodeType type;   
    Type expr_type;    
    int line, column;
    ASTNode* next;      

    union {
        struct { char* name; Type var_type; bool is_array; int array_size; } var_decl;

        struct { int op; ASTNode* left; ASTNode* right; } expr;

        struct { ASTNode* cond; ASTNode* then_block; ASTNode* else_block; } if_stmt;
        struct { ASTNode* condition; ASTNode* body; } while_stmt;
        struct { char* name; ASTNode* params; } func_call;
        struct { ASTNode* var; } read;
        struct { ASTNode* expr; } write;
        struct { ASTNode* expr; } return_stmt;
        struct { ASTNode* stmts; } block;

        struct { long int_value; } int_literal;
        struct { double float_value; } float_literal;
        struct { char* name; } identifier;
        struct { char* name; ASTNode* index; } array_access;
    } data;
};

void init_symtab(void);
void insert_symbol(const char* name, Type type, bool is_array, int array_size, int line, int column);
Symbol* lookup_symbol(const char* name);
void enter_scope(void);
void exit_scope(void);
void write_symtab(FILE* fp);
void add_error(int line, int column, const char* fmt, ...);
void write_errors(FILE* fp);

ASTNode* create_var_decl_node(const char* name, Type type, bool is_array, int array_size, int line, int column);
ASTNode* create_expr_node(int op, ASTNode* left, ASTNode* right, int line, int column);
ASTNode* create_block_node(ASTNode* stmts, int line, int column);
ASTNode* create_program_node(ASTNode* stmts, int line, int column);
ASTNode* create_if_node(ASTNode* cond, ASTNode* then_block, ASTNode* else_block, int line, int column);
ASTNode* create_while_node(ASTNode* cond, ASTNode* body, int line, int column);
ASTNode* create_func_call_node(const char* name, ASTNode* params, int line, int column);
ASTNode* create_read_node(ASTNode* var, int line, int column);
ASTNode* create_write_node(ASTNode* expr, int line, int column);
ASTNode* create_return_node(ASTNode* expr, int line, int column);
ASTNode* create_assign_node(ASTNode* var, ASTNode* expr, int line, int column);

ASTNode* create_int_literal_node(long value, int line, int column);
ASTNode* create_float_literal_node(double value, int line, int column);
ASTNode* create_identifier_node(const char* name, int line, int column);
ASTNode* create_array_access_node(const char* name, ASTNode* index, int line, int column);

void check_declarations(ASTNode* node); 
void check_types(ASTNode* node);        
Type get_expr_type(ASTNode* node);

void free_ast(ASTNode* node);

#endif 
