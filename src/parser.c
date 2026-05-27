#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tokens.h"
#include "ast.h"
#include "codegen.h"


int yylex(void);
extern char* yytext;

YYSTYPE yylval;

int line = 1;
int col = 1;

static int lookahead;
static Token* looktok = NULL;
static ASTNode* program_ast = NULL;
static int syntax_errors = 0; 

static FILE* g_deriv = NULL;
#define LOG(RULE) do { if (g_deriv) fprintf(g_deriv, "%s\n", (RULE)); } while(0)

static void advance(void) {
    lookahead = yylex();
    looktok = yylval.token;
}

static void syntax_error(const char* msg) {
    char buf[256];
    snprintf(buf, sizeof(buf), "Syntax error: %s (lookahead=%d, lexeme=%s)",
             msg, lookahead, looktok && looktok->lexeme ? looktok->lexeme : "(nil)");
    add_error(looktok ? looktok->line : line, looktok ? looktok->column : col, buf);
    syntax_errors++;
    while (lookahead != 0 && lookahead != SEMICOLON) {
        advance();
    }
    if (lookahead == SEMICOLON) {
        advance();
    }
}

static void match(int expected) {
    if (lookahead == expected) {
        advance();
        return;
    }
    char buf[128];
    snprintf(buf, sizeof(buf), "expected token code %d", expected);
    syntax_error(buf);
}

static int kw_is(const char* s) {
    return (lookahead == KW && looktok && looktok->lexeme && strcmp(looktok->lexeme, s) == 0);
}

static ASTNode* program(void);
static ASTNode* stmt_list(void);
static ASTNode* statement(void);
static ASTNode* expr(void);
static ASTNode* expr_rel_tail(ASTNode* left);
static ASTNode* arith_expr(void);
static ASTNode* arith_tail(ASTNode* left);
static ASTNode* term(void);
static ASTNode* term_tail(ASTNode* left);
static ASTNode* factor(void);
static ASTNode* varDecl(void);
static int arraySize(void);
static ASTNode* relExpr(void);
static ASTNode* statBlock(void);
static ASTNode* variable(char* name, int line, int column);
static ASTNode* assignStat(char* name, int line, int column);
static ASTNode* funcCall(void);
static ASTNode* aParams(void);
static ASTNode* aParamsTail(ASTNode* param);

static ASTNode* program(void) {
    ASTNode* stmts = stmt_list();
    return create_program_node(stmts, 1, 1);
}

static ASTNode* stmt_list(void) {
    ASTNode* head = NULL, *tail = NULL;
    if (lookahead == ID || lookahead == INTEGER || lookahead == FLOAT ||
        lookahead == LPAREN || lookahead == NOT || lookahead == PLUS || lookahead == MINUS ||
        (lookahead == KW && looktok && looktok->lexeme &&
         (strcmp(looktok->lexeme, "if") == 0 ||
          strcmp(looktok->lexeme, "while") == 0 ||
          strcmp(looktok->lexeme, "integer") == 0 ||
          strcmp(looktok->lexeme, "float") == 0 ||
          strcmp(looktok->lexeme, "local") == 0 ||
          strcmp(looktok->lexeme, "attribute") == 0 ||
          strcmp(looktok->lexeme, "self") == 0 ||
          strcmp(looktok->lexeme, "read") == 0 ||
          strcmp(looktok->lexeme, "write") == 0 ||
          strcmp(looktok->lexeme, "return") == 0))) {
        ASTNode* stmt = statement();
        if (stmt) {
            head = tail = stmt;
            ASTNode* rest = stmt_list();
            if (rest) {
                tail->next = rest;
            }
        } else {
            ASTNode* rest = stmt_list();
            head = tail = rest;
        }
    } else {
    }
    return head;
}

static ASTNode* statement(void) {
    if (kw_is("integer") || kw_is("float")) {
        LOG("statement -> type ID arraySize ;");
        Type type = kw_is("integer") ? TYPE_INT : TYPE_FLOAT;
        int line = looktok->line, column = looktok->column;
        advance();
        if (lookahead != ID) {
            syntax_error("expected ID after type");
            return NULL;
        }
        char* name = strdup(looktok->lexeme);
        advance();
        bool is_array = false;
        int array_size = 0;
        if (lookahead == LBRACKET) {
            array_size = arraySize();
            is_array = true;
        }
        match(SEMICOLON);
        return create_var_decl_node(name, type, is_array, array_size, line, column);
    }
    if (kw_is("local") || kw_is("attribute")) {
        LOG("statement -> <decl-kw> varDecl ;");
        advance();
        ASTNode* decl = varDecl();
        match(SEMICOLON);
        return decl;
    }
    if (kw_is("if")) {
        int line = looktok->line, column = looktok->column;
        advance();
        match(LPAREN);
        ASTNode* cond = relExpr();
        match(RPAREN);
        if (!kw_is("then")) { syntax_error("expected 'then' after if (relExpr)"); return NULL; }
        advance();
        ASTNode* then_block = statBlock();
        ASTNode* else_block = NULL;
        if (kw_is("else")) {
            advance();
            else_block = statBlock();
        }
        return create_if_node(cond, then_block, else_block, line, column);
    }
    if (kw_is("while")) {
        int line = looktok->line, column = looktok->column;
        advance();
        match(LPAREN);
        ASTNode* cond = relExpr();
        match(RPAREN);
        ASTNode* body = statBlock();
        return create_while_node(cond, body, line, column);
    }
    if (kw_is("read")) {
        LOG("statement -> read ( variable ) ;");
        int line = looktok->line, column = looktok->column;
        advance();
        match(LPAREN);
        ASTNode* var = variable(NULL, line, column); 
        match(RPAREN);
        match(SEMICOLON);
        return create_read_node(var, line, column);
    }
    if (kw_is("write")) {
        LOG("statement -> write ( expr ) ;");
        int line = looktok->line, column = looktok->column;
        advance();
        match(LPAREN);
        ASTNode* expr_node = expr();
        match(RPAREN);
        match(SEMICOLON);
        return create_write_node(expr_node, line, column);
    }
    if (kw_is("return")) {
        LOG("statement -> return ( expr ) ;");
        int line = looktok->line, column = looktok->column;
        advance();
        match(LPAREN);
        ASTNode* expr_node = expr();
        match(RPAREN);
        match(SEMICOLON);
        return create_return_node(expr_node, line, column);
    }
    if (lookahead == ID) {
        int line = looktok->line, column = looktok->column;
        char* name = strdup(looktok->lexeme);
        advance();
        if (lookahead == ASSIGN || lookahead == LBRACKET) {
            ASTNode* assign = assignStat(name, line, column);
            return assign;
        } else if (lookahead == LPAREN) {
            ASTNode* call = funcCall();
            call->data.func_call.name = name;
            call->line = line;
            call->column = column;
            return call;
        } else {
            syntax_error("expected ':=' or '(' after ID");
            free(name);
            return NULL;
        }
    }
    syntax_error("unrecognized start of statement");
    return NULL;
}

static ASTNode* varDecl(void) {
    if (lookahead != ID) {
        syntax_error("expected ID in varDecl");
        return NULL;
    }
    char* name = strdup(looktok->lexeme);
    int line = looktok->line, column = looktok->column;
    advance();
    bool is_array = false;
    int array_size = 0;
    if (lookahead == LBRACKET) {
        array_size = arraySize();
        is_array = true;
    }
    return create_var_decl_node(name, TYPE_INT, is_array, array_size, line, column); 
}

static int arraySize(void) {
    match(LBRACKET);
    if (lookahead != INTEGER) {
        syntax_error("expected INTEGER in array size");
        return 0;
    }
    int size = atoi(looktok->lexeme);
    advance();
    match(RBRACKET);
    return size;
}

static ASTNode* relExpr(void) {
    int line = looktok->line, column = looktok->column;
    ASTNode* left = arith_expr();
    if (!left) return NULL;
    int op = 0;
    if (lookahead == EQ || lookahead == NE || lookahead == LT || lookahead == LE || lookahead == GT || lookahead == GE) {
        op = lookahead;
        advance();
    } else {
        syntax_error("expected relational operator");
        return left;
    }
    ASTNode* right = arith_expr();
    if (!right) return left;
    return create_expr_node(op, left, right, line, column);
}

static ASTNode* statBlock(void) {
    int line = looktok->line, column = looktok->column;
    if (lookahead == LBRACE) {
        match(LBRACE);
        ASTNode* stmts = stmt_list();
        match(RBRACE);
        return create_block_node(stmts, line, column);
    } else {
        return statement();
    }
}

static ASTNode* variable(char* name, int line, int column) {
    if (!name && lookahead != ID) {
        syntax_error("expected ID in variable");
        return NULL;
    }
    if (!name) {
        name = strdup(looktok->lexeme);
        line = looktok->line;
        column = looktok->column;
        advance();
    }

    // Base is always an identifier node
    ASTNode* node = create_identifier_node(name, line, column);
    free(name);  

    if (lookahead == LBRACKET) {
        match(LBRACKET);
        ASTNode* index = arith_expr();
        if (!index) {
            free_ast(node);
            return NULL;
        }
        match(RBRACKET);
        // Wrap it as array access
        ASTNode* arr = create_array_access_node(node->data.identifier.name, index, line, column);
        free(node->data.identifier.name);  // duplicated again in array_access
        free(node);
        return arr;
    }
    return node;
}

static ASTNode* assignStat(char* name, int line, int column) {
    LOG("assignStat -> variable := expr ;");
    ASTNode* var = variable(name, line, column);  // pass name directly, variable will free it
    if (!var) return NULL;
    match(ASSIGN);
    ASTNode* expr_node = expr();
    if (!expr_node) {
        free_ast(var);
        return NULL;
    }
    match(SEMICOLON);
    return create_assign_node(var, expr_node, line, column);
}

static ASTNode* funcCall(void) {
    LOG("funcCall -> ( aParams ) ;");
    int line = looktok ? looktok->line : 1, column = looktok ? looktok->column : 1;
    match(LPAREN);
    ASTNode* params = aParams();
    match(RPAREN);
    match(SEMICOLON);
    return create_func_call_node("", params, line, column); 
}

static ASTNode* aParams(void) {
    if (lookahead != RPAREN) {
        ASTNode* param = expr();
        if (!param) return NULL;
        return aParamsTail(param);
    } else {
        return NULL;
    }
}

static ASTNode* aParamsTail(ASTNode* param) {
    if (lookahead == COMMA) {
        match(COMMA);
        ASTNode* next_param = expr();
        if (!next_param) return param;
        next_param->next = param;
        return aParamsTail(next_param);
    } else {
        return param;
    }
}

static ASTNode* expr(void) {
    ASTNode* left = arith_expr();
    if (!left) return NULL;
    return expr_rel_tail(left);
}

static ASTNode* expr_rel_tail(ASTNode* left) {
    int line = looktok ? looktok->line : 1, column = looktok ? looktok->column : 1;
    if (lookahead == EQ || lookahead == NE || lookahead == LT ||
        lookahead == LE || lookahead == GT || lookahead == GE) {
        int op = lookahead;
        if (lookahead == EQ) LOG("expr_rel_tail -> == arith_expr");
        else if (lookahead == NE) LOG("expr_rel_tail -> <> arith_expr");
        else if (lookahead == LT) LOG("expr_rel_tail -> < arith_expr");
        else if (lookahead == LE) LOG("expr_rel_tail -> <= arith_expr");
        else if (lookahead == GT) LOG("expr_rel_tail -> > arith_expr");
        else LOG("expr_rel_tail -> >= arith_expr");
        advance();
        ASTNode* right = arith_expr();
        if (!right) return left;
        return create_expr_node(op, left, right, line, column);
    } else {
        return left;
    }
}

static ASTNode* arith_expr(void) {
    ASTNode* left = term();
    if (!left) return NULL;
    return arith_tail(left);
}

static ASTNode* arith_tail(ASTNode* left) {
    int line = looktok ? looktok->line : 1, column = looktok ? looktok->column : 1;
    if (lookahead == PLUS || lookahead == MINUS || lookahead == OR) {
        int op = lookahead;
        if (lookahead == PLUS) LOG("arith_tail -> + term arith_tail");
        else if (lookahead == MINUS) LOG("arith_tail -> - term arith_tail");
        else LOG("arith_tail -> or term arith_tail");
        advance();
        ASTNode* right = term();
        if (!right) return left;
        ASTNode* node = create_expr_node(op, left, right, line, column);
        return arith_tail(node);
    } else {
        return left;
    }
}

static ASTNode* term(void) {
    ASTNode* left = factor();
    if (!left) return NULL;
    return term_tail(left);
}

static ASTNode* term_tail(ASTNode* left) {
    int line = looktok ? looktok->line : 1, column = looktok ? looktok->column : 1;
    if (lookahead == TIMES || lookahead == DIVIDE || lookahead == AND) {
        int op = lookahead;
        if (lookahead == TIMES) LOG("term_tail -> * factor term_tail");
        else if (lookahead == DIVIDE) LOG("term_tail -> / factor term_tail");
        else LOG("term_tail -> and factor term_tail");
        advance();
        ASTNode* right = factor();
        if (!right) return left;
        ASTNode* node = create_expr_node(op, left, right, line, column);
        return term_tail(node);
    } else {
        return left;
    }
}

static ASTNode* factor(void) {
    int line = looktok ? looktok->line : 1, column = looktok ? looktok->column : 1;
    if (lookahead == ID) {
        char* name = strdup(looktok->lexeme);
        advance();
        return create_identifier_node(name, line, column);
    }
    if (lookahead == INTEGER) {
        long v = atol(looktok->lexeme);
        advance();
        return create_int_literal_node(v, line, column);
    }
    if (lookahead == FLOAT) {
        double f = atof(looktok->lexeme);
        advance();
        return create_float_literal_node(f, line, column);
    }
    if (kw_is("self")) {
        ASTNode* node = create_identifier_node(strdup("self"), line, column);
        advance();
        return node;
    }
    if (lookahead == LPAREN) {
        advance();
        ASTNode* expr_node = expr();
        if (!expr_node) return NULL;
        match(RPAREN);
        return expr_node;
    }
    if (lookahead == NOT) {
        advance();
        ASTNode* right = factor();
        if (!right) return NULL;
        return create_expr_node(NOT, right, NULL, line, column);
    }
    if (lookahead == PLUS) {
        advance();
        ASTNode* right = factor();
        if (!right) return NULL;
        return create_expr_node(PLUS, right, NULL, line, column);
    }
    if (lookahead == MINUS) {
        advance();
        ASTNode* right = factor();
        if (!right) return NULL;
        return create_expr_node(MINUS, right, NULL, line, column);
    }
    syntax_error("invalid start of factor");
    return NULL;
}

int main(int argc, char** argv) {
    g_deriv = fopen("derivation.txt", "w");
    if (!g_deriv) {
        perror("derivation.txt");
        return 1;
    }

    if (argc >= 2) {
        if (freopen(argv[1], "r", stdin) == NULL) { perror(argv[1]); return 1; }
    }

    init_symtab();
    advance();
    program_ast = program();

    if (program_ast) {
        check_declarations(program_ast);
        check_types(program_ast);
    }

    FILE* symtab_file = fopen("symtab.txt", "w");
    if (!symtab_file) {
        perror("symtab.txt");
        fclose(g_deriv);
        return 1;
    }
    write_symtab(symtab_file);
    fclose(symtab_file);

    FILE* error_file = fopen("errors.txt", "w");
    if (!error_file) {
        perror("errors.txt");
        fclose(g_deriv);
        return 1;
    }
    write_errors(error_file);
    fclose(error_file);

    fclose(g_deriv);
    init_codegen("output.tac");
    generate_code(program_ast);
    finish_codegen();
    free_ast(program_ast);
    printf("Parsing & semantic analysis completed. \n");
    if (syntax_errors > 0) {
        printf("Note: %d syntax error(s) encountered, see errors.txt\n", syntax_errors);
    }
    return syntax_errors > 0 ? 1 : 0;
}