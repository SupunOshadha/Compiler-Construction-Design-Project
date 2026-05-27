#ifndef TOKENS_H
#define TOKENS_H

typedef struct Token {
    char* type;
    char* lexeme;
    int   line;
    int   column;
} Token;

typedef enum {
    AST_PROGRAM,
    AST_VAR_DECL,
    AST_ASSIGN,
    AST_EXPR,
    AST_IF,
    AST_WHILE,
    AST_FUNC_CALL,
    AST_READ,
    AST_WRITE,
    AST_RETURN,
    AST_BLOCK,
    AST_INT_LITERAL,
    AST_FLOAT_LITERAL,
    AST_IDENTIFIER,
    AST_ARRAY_ACCESS
} ASTNodeType;

enum {
    ID = 256,
    KW,
    FLOAT,
    INTEGER,
    LPAREN, RPAREN, LBRACE, RBRACE, LBRACKET, RBRACKET,
    SEMICOLON, COMMA, DOT, COLON, ARROW,
    PLUS, MINUS, TIMES, DIVIDE, ASSIGN,
    LT, GT, LE, GE, EQ, NE,
    OR, AND, NOT
};

typedef union {
    Token* token;
    struct ASTNode* ast_node;
} YYSTYPE;

extern YYSTYPE yylval;

#endif
