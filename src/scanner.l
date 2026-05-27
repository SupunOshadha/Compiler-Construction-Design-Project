%{
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tokens.h" 

Token* create_token(const char* type, const char* lexeme, int line, int col) {
    Token* t = (Token*)malloc(sizeof(Token));
    t->type = strdup(type);
    t->lexeme = strdup(lexeme);
    t->line = line;
    t->column = col;
    return t;
}

extern int line;
extern int col;

void update_position(const char* text) {
    for (const char* p = text; *p; p++) {
        if (*p == '\n') {
            line++;
            col = 1;
        } else {
            col++;
        }
    }
}
%}

%option noyywrap

DIGIT      [0-9]
INTEGER    {DIGIT}+
FLOAT      {DIGIT}+"."{DIGIT}*
ID         [a-zA-Z_][a-zA-Z0-9_]*
KW         (else|float|func|if|implement|class|attribute|integer|isa|private|public|read|return|self|construct|then|local|void|while|write)

%%

[ \t\r]+      { update_position(yytext); }
\n            { update_position(yytext); }
"//".*        { update_position(yytext); }

"/*"([^*]*|\*+[^*/])*"\*/" {
    update_position(yytext);
}

":="          { yylval.token = create_token("ASSIGN", yytext, line, col); update_position(yytext); return ASSIGN; }
"=>"          { yylval.token = create_token("ARROW", yytext, line, col); update_position(yytext); return ARROW; }
":"           { yylval.token = create_token("COLON", yytext, line, col); update_position(yytext); return COLON; }

"<="          { yylval.token = create_token("LE", yytext, line, col); update_position(yytext); return LE; }
">="          { yylval.token = create_token("GE", yytext, line, col); update_position(yytext); return GE; }
"=="          { yylval.token = create_token("EQ", yytext, line, col); update_position(yytext); return EQ; }
"<>"          { yylval.token = create_token("NE", yytext, line, col); update_position(yytext); return NE; }
"<"           { yylval.token = create_token("LT", yytext, line, col); update_position(yytext); return LT; }
">"           { yylval.token = create_token("GT", yytext, line, col); update_position(yytext); return GT; }

"+"           { yylval.token = create_token("PLUS", yytext, line, col); update_position(yytext); return PLUS; }
"-"           { yylval.token = create_token("MINUS", yytext, line, col); update_position(yytext); return MINUS; }
"*"           { yylval.token = create_token("TIMES", yytext, line, col); update_position(yytext); return TIMES; }
"/"           { yylval.token = create_token("DIVIDE", yytext, line, col); update_position(yytext); return DIVIDE; }

"("           { yylval.token = create_token("LPAREN", yytext, line, col); update_position(yytext); return LPAREN; }
")"           { yylval.token = create_token("RPAREN", yytext, line, col); update_position(yytext); return RPAREN; }
"{"           { yylval.token = create_token("LBRACE", yytext, line, col); update_position(yytext); return LBRACE; }
"}"           { yylval.token = create_token("RBRACE", yytext, line, col); update_position(yytext); return RBRACE; }
"["           { yylval.token = create_token("LBRACKET", yytext, line, col); update_position(yytext); return LBRACKET; }
"]"           { yylval.token = create_token("RBRACKET", yytext, line, col); update_position(yytext); return RBRACKET; }
";"           { yylval.token = create_token("SEMICOLON", yytext, line, col); update_position(yytext); return SEMICOLON; }
","           { yylval.token = create_token("COMMA", yytext, line, col); update_position(yytext); return COMMA; }
"."           { yylval.token = create_token("DOT", yytext, line, col); update_position(yytext); return DOT; }

"or"          { yylval.token = create_token("OR", yytext, line, col); update_position(yytext); return OR; }
"and"         { yylval.token = create_token("AND", yytext, line, col); update_position(yytext); return AND; }
"not"         { yylval.token = create_token("NOT", yytext, line, col); update_position(yytext); return NOT; }

{FLOAT}       { yylval.token = create_token("FLOAT", yytext, line, col); update_position(yytext); return FLOAT; }
{INTEGER}     { yylval.token = create_token("INTEGER", yytext, line, col); update_position(yytext); return INTEGER; }
{KW}          { yylval.token = create_token("KW", yytext, line, col); update_position(yytext); return KW; }
{ID}          { yylval.token = create_token("ID", yytext, line, col); update_position(yytext); return ID; }

.             { update_position(yytext); }

%%