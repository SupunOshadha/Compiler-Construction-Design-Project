#include "ast.h"
#include <string.h>
#include <stdarg.h>

static char* strdup_safe(const char* s) {
    if (!s) return NULL;
    size_t n = strlen(s)+1;
    char* p = malloc(n);
    if (!p) return NULL;
    memcpy(p, s, n);
    return p;
}

static SymbolTable symtab;
static Error* errors = NULL;

void init_symtab(void) {
    symtab.head = NULL;
    symtab.current_scope = 0;
}

static Symbol* make_symbol(const char* name, Type type, bool is_array, int array_size, int scope) {
    Symbol* s = malloc(sizeof(Symbol));
    s->name = strdup_safe(name);
    s->type = type;
    s->is_array = is_array;
    s->array_size = array_size;
    s->scope = scope;
    s->next = NULL;
    return s;
}

void insert_symbol(const char* name, Type type, bool is_array, int array_size, int line, int column) {
    Symbol* s = make_symbol(name, type, is_array, array_size, symtab.current_scope);
    s->next = symtab.head;
    symtab.head = s;
}

Symbol* lookup_symbol(const char* name) {
    for (Symbol* s = symtab.head; s; s = s->next) {
        if (strcmp(s->name, name) == 0) return s;
    }
    return NULL;
}

void enter_scope(void) { symtab.current_scope++; }
void exit_scope(void) {
    Symbol* prev = NULL;
    Symbol* cur = symtab.head;
    while (cur) {
        if (cur->scope == symtab.current_scope) {
            Symbol* rem = cur;
            if (prev) prev->next = cur->next;
            else symtab.head = cur->next;
            cur = cur->next;
            free(rem->name);
            free(rem);
        } else {
            prev = cur;
            cur = cur->next;
        }
    }
    symtab.current_scope--;
}

void write_symtab(FILE* fp) {
    fprintf(fp, "Symbol Table (scope %d downwards):\n", symtab.current_scope);
    for (Symbol* s = symtab.head; s; s = s->next) {
        fprintf(fp, "%s\t%s\tarray=%d\tscope=%d\n", s->name,
            s->type==TYPE_INT ? "integer" : (s->type==TYPE_FLOAT ? "float" : "bool"),
            s->is_array ? s->array_size : 0, s->scope);
    }
}

void add_error(int line, int column, const char* fmt, ...) {
    char buf[256];
    va_list ap; va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    Error* e = malloc(sizeof(Error));
    e->line = line; e->column = column;
    e->msg = strdup_safe(buf);
    e->next = errors;
    errors = e;
}

void write_errors(FILE* fp) {
    for (Error* e = errors; e; e = e->next) {
        fprintf(fp, "Line %d:%d: %s\n", e->line, e->column, e->msg);
    }
}

static ASTNode* alloc_node(ASTNodeType t, int line, int column) {
    ASTNode* n = malloc(sizeof(ASTNode));
    memset(n, 0, sizeof(ASTNode));
    n->type = t;
    n->line = line;
    n->column = column;
    n->expr_type = TYPE_INT;
    n->next = NULL;
    return n;
}

ASTNode* create_var_decl_node(const char* name, Type type, bool is_array, int array_size, int line, int column) {
    ASTNode* n = alloc_node(AST_VAR_DECL, line, column);
    n->data.var_decl.name = strdup_safe(name);
    n->data.var_decl.var_type = type;
    n->data.var_decl.is_array = is_array;
    n->data.var_decl.array_size = array_size;
    return n;
}

ASTNode* create_expr_node(int op, ASTNode* left, ASTNode* right, int line, int column) {
    ASTNode* n = alloc_node(AST_EXPR, line, column);
    n->data.expr.op = op;
    n->data.expr.left = left;
    n->data.expr.right = right;
    return n;
}

ASTNode* create_block_node(ASTNode* stmts, int line, int column) {
    ASTNode* n = alloc_node(AST_BLOCK, line, column);
    n->data.block.stmts = stmts;
    return n;
}

ASTNode* create_program_node(ASTNode* stmts, int line, int column) {
    ASTNode* n = alloc_node(AST_PROGRAM, line, column);
    n->data.block.stmts = stmts;
    return n;
}

ASTNode* create_if_node(ASTNode* cond, ASTNode* then_block, ASTNode* else_block, int line, int column) {
    ASTNode* n = alloc_node(AST_IF, line, column);
    n->data.if_stmt.cond = cond;
    n->data.if_stmt.then_block = then_block;
    n->data.if_stmt.else_block = else_block;
    return n;
}

ASTNode* create_while_node(ASTNode* cond, ASTNode* body, int line, int column) {
    ASTNode* n = alloc_node(AST_WHILE, line, column);
    n->data.while_stmt.condition = cond;
    n->data.while_stmt.body = body;
    return n;
}

ASTNode* create_func_call_node(const char* name, ASTNode* params, int line, int column) {
    ASTNode* n = alloc_node(AST_FUNC_CALL, line, column);
    n->data.func_call.name = strdup_safe(name);
    n->data.func_call.params = params;
    return n;
}

ASTNode* create_read_node(ASTNode* var, int line, int column) {
    ASTNode* n = alloc_node(AST_READ, line, column);
    n->data.read.var = var;
    return n;
}

ASTNode* create_write_node(ASTNode* expr, int line, int column) {
    ASTNode* n = alloc_node(AST_WRITE, line, column);
    n->data.write.expr = expr;
    return n;
}

ASTNode* create_return_node(ASTNode* expr, int line, int column) {
    ASTNode* n = alloc_node(AST_RETURN, line, column);
    n->data.return_stmt.expr = expr;
    return n;
}

ASTNode* create_assign_node(ASTNode* var, ASTNode* expr, int line, int column) {
    ASTNode* n = alloc_node(AST_ASSIGN, line, column);
    n->data.expr.left = var;
    n->data.expr.right = expr;
    return n;
}

ASTNode* create_int_literal_node(long value, int line, int column) {
    ASTNode* n = alloc_node(AST_INT_LITERAL, line, column);
    n->data.int_literal.int_value = value;
    return n;
}
ASTNode* create_float_literal_node(double value, int line, int column) {
    ASTNode* n = alloc_node(AST_FLOAT_LITERAL, line, column);
    n->data.float_literal.float_value = value;
    return n;
}
ASTNode* create_identifier_node(const char* name, int line, int column) {
    ASTNode* n = alloc_node(AST_IDENTIFIER, line, column);
    n->data.identifier.name = strdup_safe(name);
    return n;
}
ASTNode* create_array_access_node(const char* name, ASTNode* index, int line, int column) {
    ASTNode* n = alloc_node(AST_ARRAY_ACCESS, line, column);
    n->data.array_access.name = strdup_safe(name);
    n->data.array_access.index = index;
    return n;
}

void check_declarations(ASTNode* node) {
    for (ASTNode* cur = node; cur; cur = cur->next) {
        switch (cur->type) {
            case AST_VAR_DECL:
                insert_symbol(cur->data.var_decl.name, cur->data.var_decl.var_type,
                              cur->data.var_decl.is_array, cur->data.var_decl.array_size,
                              cur->line, cur->column);
                break;
            case AST_BLOCK:
                enter_scope();
                check_declarations(cur->data.block.stmts);
                exit_scope();
                break;
            case AST_IF:
                check_declarations(cur->data.if_stmt.then_block);
                if (cur->data.if_stmt.else_block) check_declarations(cur->data.if_stmt.else_block);
                break;
            case AST_WHILE:
                check_declarations(cur->data.while_stmt.body);
                break;
            case AST_PROGRAM:
                check_declarations(cur->data.block.stmts);
                break;
            default: break;
        }
    }
}

Type get_expr_type(ASTNode* node) {
    if (!node) return TYPE_INT;

    switch (node->type) {
        case AST_INT_LITERAL:
            return TYPE_INT;
        case AST_FLOAT_LITERAL:
            return TYPE_FLOAT;
        case AST_IDENTIFIER: {
            Symbol* s = lookup_symbol(node->data.identifier.name);
            if (s) return s->type;
            return TYPE_INT;  
        }
        case AST_ARRAY_ACCESS: {
            Symbol* s = lookup_symbol(node->data.array_access.name);
            if (s) return s->type;
            return TYPE_INT;
        }
        case AST_FUNC_CALL:
            return TYPE_INT;  
        case AST_EXPR: {
            int op = node->data.expr.op;
            if (op == EQ || op == NE || op == LT || op == LE || op == GT || op == GE) {
                return TYPE_BOOL;
            }
            if (op == OR || op == AND || op == NOT) {
                return TYPE_BOOL;
            }
            Type l = get_expr_type(node->data.expr.left);
            Type r = node->data.expr.right ? get_expr_type(node->data.expr.right) : TYPE_INT;
            if (l == TYPE_FLOAT || r == TYPE_FLOAT) return TYPE_FLOAT;
            return TYPE_INT;
        }
        default:
            return TYPE_INT;
    }
}


void check_types(ASTNode* node) {
    for (ASTNode* cur = node; cur; cur = cur->next) {
        switch (cur->type) {
            case AST_ASSIGN: {
                Type lt = get_expr_type(cur->data.expr.left);
                Type rt = get_expr_type(cur->data.expr.right);
                if (lt != rt && !(lt == TYPE_FLOAT && rt == TYPE_INT)) {
                    add_error(cur->line, cur->column, "Type mismatch in assignment (left: %s, right: %s)",
                              lt==TYPE_INT?"int":(lt==TYPE_FLOAT?"float":"bool"),
                              rt==TYPE_INT?"int":(rt==TYPE_FLOAT?"float":"bool"));
                }
                break;
            }
            case AST_IF: {
                Type ct = get_expr_type(cur->data.if_stmt.cond);
                if (ct!=TYPE_BOOL) add_error(cur->line, cur->column, "If condition not boolean");
                check_types(cur->data.if_stmt.then_block);
                if (cur->data.if_stmt.else_block) check_types(cur->data.if_stmt.else_block);
                break;
            }
            case AST_WHILE: {
                Type ct = get_expr_type(cur->data.while_stmt.condition);
                if (ct!=TYPE_BOOL) add_error(cur->line, cur->column, "While condition not boolean");
                check_types(cur->data.while_stmt.body);
                break;
            }
            case AST_BLOCK:
                check_types(cur->data.block.stmts);
                break;
            case AST_PROGRAM:
                check_types(cur->data.block.stmts);
                break;
            default: break;
        }
    }
}

void free_ast(ASTNode* node) {
    while (node) {
        ASTNode* next = node->next;
        switch (node->type) {
            case AST_VAR_DECL:
                free(node->data.var_decl.name);
                break;
            case AST_EXPR:
                if (node->data.expr.left) free_ast(node->data.expr.left);
                if (node->data.expr.right) free_ast(node->data.expr.right);
                break;
            case AST_FUNC_CALL:
                free(node->data.func_call.name);
                free_ast(node->data.func_call.params);
                break;
            case AST_READ:
                free_ast(node->data.read.var);
                break;
            case AST_WRITE:
                free_ast(node->data.write.expr);
                break;
            case AST_RETURN:
                free_ast(node->data.return_stmt.expr);
                break;
            case AST_BLOCK:
            case AST_PROGRAM:
                free_ast(node->data.block.stmts);
                break;
            case AST_IF:
                free_ast(node->data.if_stmt.cond);
                free_ast(node->data.if_stmt.then_block);
                if (node->data.if_stmt.else_block) free_ast(node->data.if_stmt.else_block);
                break;
            case AST_WHILE:
                free_ast(node->data.while_stmt.condition);
                free_ast(node->data.while_stmt.body);
                break;
            case AST_INT_LITERAL:
                break;
            case AST_FLOAT_LITERAL:
                break;
            case AST_IDENTIFIER:
                free(node->data.identifier.name);
                break;
            case AST_ARRAY_ACCESS:
                free(node->data.array_access.name);
                if (node->data.array_access.index) free_ast(node->data.array_access.index);
                break;
            default: break;
        }
        free(node);
        node = next;
    }
}
