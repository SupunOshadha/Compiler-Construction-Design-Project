#include "codegen.h"
#include "tokens.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

static FILE *out = NULL;

static int temp_counter = 0;
static int label_counter = 0;

typedef struct VarOffset {
    char *name;
    int offset;            
    int size;               
    struct VarOffset *next;
} VarOffset;

static VarOffset *var_list = NULL;
static int current_stack_bytes = 0; 

static char *strdup_s(const char *s) {
    if (!s) return NULL;
    char *p = malloc(strlen(s)+1);
    strcpy(p, s);
    return p;
}
static char *new_temp_str(void) {
    char buf[32];
    snprintf(buf, sizeof(buf), "t%d", temp_counter++);
    return strdup_s(buf);
}
static char *new_label_str(void) {
    char buf[32];
    snprintf(buf, sizeof(buf), "L%d", label_counter++);
    return strdup_s(buf);
}

static int is_number_literal(const char *s) {
    if (!s) return 0;
    int i = 0;
    int len = strlen(s);
    if (len == 0) return 0;
    if (s[0] == '+' || s[0] == '-') i = 1;
    int dots = 0;
    int digits = 0;
    for (; i < len; ++i) {
        if (s[i] == '.') {
            dots++;
            if (dots > 1) return 0;
        } else if (isdigit((unsigned char)s[i])) {
            digits = 1;
        } else return 0;
    }
    return digits;
}

static void add_var_offset(const char *name, int slots) {
    VarOffset *v = malloc(sizeof(VarOffset));
    v->name = strdup_s(name);
    v->size = slots > 0 ? slots : 1;
    v->offset = current_stack_bytes;       
    v->next = var_list;
    var_list = v;
    current_stack_bytes += v->size * 4;    
}

static int get_var_offset(const char *name) {
    for (VarOffset *v = var_list; v != NULL; v = v->next) {
        if (strcmp(v->name, name) == 0) return v->offset;
    }
    return -1;
}

static void clear_var_list(void) {
    VarOffset *v = var_list;
    while (v) {
        VarOffset *n = v->next;
        free(v->name);
        free(v);
        v = n;
    }
    var_list = NULL;
    current_stack_bytes = 0;
}

static void emitf(const char *fmt, ...) {
    if (!out) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(out, fmt, ap);
    fprintf(out, "\n");
    va_end(ap);
}

static char *gen_expr(ASTNode *node);
static void gen_stmt(ASTNode *node);

void init_codegen(const char *filename) {
    out = fopen(filename, "w");
    if (!out) { perror("init_codegen fopen"); exit(1); }
    temp_counter = 0;
    label_counter = 0;
    clear_var_list();
}

void generate_code(ASTNode *program_root) {
    if (!program_root) return;
    ASTNode *stmts = NULL;
    if (program_root->type == AST_PROGRAM) stmts = program_root->data.block.stmts;
    else if (program_root->type == AST_BLOCK) stmts = program_root->data.block.stmts;
    else stmts = program_root;
    gen_stmt(stmts);
    if (current_stack_bytes > 0) {
        emitf("; Stack frame size: %d bytes", current_stack_bytes);
        for (VarOffset *v = var_list; v; v = v->next) {
            emitf("; var %s -> [SP + %d] (slots=%d)", v->name, v->offset, v->size);
        }
    }
}

void finish_codegen(void) {
    if (out) { fclose(out); out = NULL; }
    clear_var_list();
}

static ASTNode* unwrap_lvalue(ASTNode* node) {
    if (!node) return NULL;

    while (node->type == AST_EXPR && node->data.expr.right == NULL) {
        node = node->data.expr.left;
    }

    return node;
}


static void gen_stmt(ASTNode *node) {
    for (ASTNode *cur = node; cur; cur = cur->next) {
        if (!cur) continue;
        switch (cur->type) {
            case AST_VAR_DECL: {
                if (get_var_offset(cur->data.var_decl.name) < 0) {
                    int slots = cur->data.var_decl.is_array ? cur->data.var_decl.array_size : 1;
                    add_var_offset(cur->data.var_decl.name, slots);
                    emitf("; DECL %s  slots=%d  offset=%d", cur->data.var_decl.name, slots, get_var_offset(cur->data.var_decl.name));
                }
                break;
            }
            case AST_ASSIGN: {
			    ASTNode *lhs = unwrap_lvalue(cur->data.expr.left);
			    ASTNode *rhs = cur->data.expr.right;
			
			    char *rtemp = gen_expr(rhs);
			
			    if (lhs && lhs->type == AST_IDENTIFIER) {
			        const char *lname = lhs->data.identifier.name;
			        int off = get_var_offset(lname);
			
			        if (off < 0) {
			            add_var_offset(lname, 1);
			            off = get_var_offset(lname);
			            emitf("; (late) DECL %s offset=%d", lname, off);
			        }
			     
					const char *comment = strdup_s(lname);
			        char *full_comment = malloc(strlen(comment) + 20);
			        
			        if (rhs->type == AST_INT_LITERAL) {
			            sprintf(full_comment, "%s := %ld", lname, rhs->data.int_literal.int_value);
			        } else if (rhs->type == AST_FLOAT_LITERAL) {
			            char buf[32];
			            snprintf(buf, sizeof(buf), "%.6f", rhs->data.float_literal.float_value);
			            sprintf(full_comment, "%s := %s", lname, buf);
			        } else {
			            sprintf(full_comment, "%s := <expr result in %s>", lname, rtemp);
			        }
			
			        emitf("STORE %s -> [SP + %d] ; %s", rtemp, off, full_comment);
			
			        free(full_comment);
			        
			    }
			    else if (lhs && lhs->type == AST_ARRAY_ACCESS) {
			        char *idx = gen_expr(lhs->data.array_access.index);
			        int base = get_var_offset(lhs->data.array_access.name);
			
			        if (base < 0) {
			            add_var_offset(lhs->data.array_access.name, 1);
			            base = get_var_offset(lhs->data.array_access.name);
			            emitf("; (late) DECL %s offset=%d", lhs->data.array_access.name, base);
			        }
			
			        char *addr_temp = new_temp_str();
			        emitf("MUL %s, %s, t_const4", addr_temp, idx);
			        emitf("ADD %s, %s, %d", addr_temp, addr_temp, base);
			        emitf("STORE %s -> [%s]    ; array store", rtemp, addr_temp);
			
			        free(idx);
			        free(addr_temp);
			    }
			    else {
			        emitf("; DEBUG LHS final type = %d", lhs ? lhs->type : -1);

			    }
			
			    free(rtemp);
			    break;
			}

            case AST_WRITE: {
                char *t = gen_expr(cur->data.write.expr);
                emitf("WRITE %s", t);
                free(t);
                break;
            }
            case AST_READ: {
                const char *vname = cur->data.read.var->data.identifier.name;
                int off = get_var_offset(vname);
                if (off < 0) {
                    add_var_offset(vname, 1);
                    off = get_var_offset(vname);
                    emitf("; (late) DECL %s offset=%d", vname, off);
                }
                char *t = new_temp_str();
                emitf("READ %s", t);
                emitf("STORE %s -> [SP + %d]    ; read into %s", t, off, vname);
                free(t);
                break;
            }
            case AST_IF: {
                char *cond = gen_expr(cur->data.if_stmt.cond);
                char *lbl_else = new_label_str();
                char *lbl_end = new_label_str();
                emitf("IF_FALSE %s GOTO %s", cond, lbl_else);
                free(cond);
                gen_stmt(cur->data.if_stmt.then_block);
                emitf("GOTO %s", lbl_end);
                emitf("%s:", lbl_else);
                if (cur->data.if_stmt.else_block) gen_stmt(cur->data.if_stmt.else_block);
                emitf("%s:", lbl_end);
                free(lbl_else); free(lbl_end);
                break;
            }
            case AST_WHILE: {
                char *lbl_start = new_label_str();
                char *lbl_end = new_label_str();
                emitf("%s:", lbl_start);
                char *cond = gen_expr(cur->data.while_stmt.condition);
                emitf("IF_FALSE %s GOTO %s", cond, lbl_end);
                free(cond);
                gen_stmt(cur->data.while_stmt.body);
                emitf("GOTO %s", lbl_start);
                emitf("%s:", lbl_end);
                free(lbl_start); free(lbl_end);
                break;
            }
            case AST_FUNC_CALL: {
                int nargs = 0;
                for (ASTNode *p = cur->data.func_call.params; p; p = p->next) {
                    char *a = gen_expr(p);
                    emitf("PUSH %s", a);
                    free(a);
                    nargs++;
                }
                emitf("CALL %s, %d", cur->data.func_call.name, nargs);
                break;
            }
            case AST_RETURN: {
                char *val = gen_expr(cur->data.return_stmt.expr);
                emitf("RETURN %s", val);
                free(val);
                break;
            }
            case AST_BLOCK: {
                gen_stmt(cur->data.block.stmts);
                break;
            }
            case AST_PROGRAM: {
                gen_stmt(cur->data.block.stmts);
                break;
            }
            default: {
                if (cur->type == AST_EXPR) {
                    char *t = gen_expr(cur);
                    free(t);
                } else {
                    emitf("; unknown stmt type %d", cur->type);
                }
                break;
            }
        }
    }
}

static char *gen_expr(ASTNode *node) {
    if (!node) return strdup_s("t_err");
    switch (node->type) {
        case AST_INT_LITERAL: {
            char tmpbuf[64];
            snprintf(tmpbuf, sizeof(tmpbuf), "%ld", node->data.int_literal.int_value);
            char *temp = new_temp_str();
            emitf("LOADI %s, %s", temp, tmpbuf);
            return temp;
        }
        case AST_FLOAT_LITERAL: {
            char tmpbuf[64];
            snprintf(tmpbuf, sizeof(tmpbuf), "%f", node->data.float_literal.float_value);
            char *temp = new_temp_str();
            emitf("LOADF %s, %s", temp, tmpbuf);
            return temp;
        }
        case AST_IDENTIFIER: {
            const char *name = node->data.identifier.name;
            int off = get_var_offset(name);
            if (off < 0) {
                add_var_offset(name, 1);
                off = get_var_offset(name);
                emitf("; (late) DECL %s offset=%d", name, off);
            }
            char *tmp = new_temp_str();
            emitf("LOAD %s, [SP + %d]    ; load %s", tmp, off, name);
            return tmp;
        }
        case AST_ARRAY_ACCESS: {
            char *idx = gen_expr(node->data.array_access.index);
            int base = get_var_offset(node->data.array_access.name);
            if (base < 0) {
                add_var_offset(node->data.array_access.name, 1);
                base = get_var_offset(node->data.array_access.name);
                emitf("; (late) DECL %s offset=%d", node->data.array_access.name, base);
            }
            char *addr = new_temp_str();
            emitf("MUL %s, %s, %s", addr, idx, "t_const4"); 
            emitf("ADD %s, %s, %d", addr, addr, base);
            char *res = new_temp_str();
            emitf("LOAD %s, [%s]", res, addr);
            free(idx); free(addr);
            return res;
        }
        case AST_EXPR: {
            ASTNode *L = node->data.expr.left;
            ASTNode *R = node->data.expr.right;
            if (!R) {
                char *val = gen_expr(L);
                char *res = new_temp_str();
                if (node->data.expr.op == NOT) emitf("NOT %s, %s", res, val);
                else if (node->data.expr.op == PLUS) emitf("UADD %s, %s", res, val);
                else if (node->data.expr.op == MINUS) emitf("USUB %s, %s", res, val);
                else emitf("UNKN %s, %s", res, val);
                free(val);
                return res;
            } else {
                char *lt = gen_expr(L);
                char *rt = gen_expr(R);
                char *res = new_temp_str();
                int op = node->data.expr.op;
                switch (op) {
                    case PLUS: emitf("ADD %s, %s, %s", res, lt, rt); break;
                    case MINUS: emitf("SUB %s, %s, %s", res, lt, rt); break;
                    case TIMES: emitf("MUL %s, %s, %s", res, lt, rt); break;
                    case DIVIDE: emitf("DIV %s, %s, %s", res, lt, rt); break;
                    case EQ: emitf("EQ %s, %s, %s", res, lt, rt); break;
                    case NE: emitf("NE %s, %s, %s", res, lt, rt); break;
                    case LT: emitf("LT %s, %s, %s", res, lt, rt); break;
                    case LE: emitf("LE %s, %s, %s", res, lt, rt); break;
                    case GT: emitf("GT %s, %s, %s", res, lt, rt); break;
                    case GE: emitf("GE %s, %s, %s", res, lt, rt); break;
                    case OR: emitf("OR %s, %s, %s", res, lt, rt); break;
                    case AND: emitf("AND %s, %s, %s", res, lt, rt); break;
                    default: emitf("OPUNK %s, %s, %s", res, lt, rt); break;
                }
                free(lt); free(rt);
                return res;
            }
        }
        case AST_FUNC_CALL: {
            int nargs = 0;
            for (ASTNode *p = node->data.func_call.params; p; p = p->next) {
                char *a = gen_expr(p);
                emitf("PUSH %s", a);
                free(a);
                nargs++;
            }
            emitf("CALL %s, %d", node->data.func_call.name, nargs);
            char *ret = new_temp_str();
            emitf("%s = RETVAL", ret);
            return ret;
        }
        default: {
            char *tmp = new_temp_str();
            emitf("; unknown expr type %d -> %s", node->type, tmp);
            return tmp;
        }
    }
}