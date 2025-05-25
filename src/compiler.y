/* Definition section */
%{
    #include "compiler_util.h"
    #include "compiler_common.h"
    #include "main.h"

    void yyerror(char const* msg);
%}

%define parse.error custom

/* Variable or self-defined structure */
%union {
    ObjectType var_type;

    bool b_var;
    char c_var;
    ScientificNotation n_var;
    char *s_var;

    Object obj_val;

    // LinkList<Object*>
    // LinkedList* array_subscript;
}
/* Token without return */
%token PRINT
%token IMMEDIATE IS AS NUMBER STRING IMMEDIATE_NUMBER_IS
%token FOR TIMES END_BRACKET
%token PAST VARIABLE ASSIGN TO_IT

/* Token with return, which need to sepcify type */
%token <var_type> VARIABLE_T
%token <b_var> BOOL_LIT
%token <c_var> CHAR_LIT
%token <n_var> NUMBER_LIT
%token <s_var> STR_LIT
%token <s_var> IDENT

/* Nonterminal with return, which need to sepcify type */
%type <obj_val> DefineStmt
%type <obj_val> ExpressionStmt
%type <obj_val> ValueStmt

%nonassoc THEN
%nonassoc ELSE
%nonassoc END_BRACKET

/* Yacc will start at this nonterminal */
%start Program
%%
/* Grammar section */

/* Scope */
Program
    : GlobalScopeStmt
    | /* Empty file */
;

GlobalScopeStmt
    : { pushScope(); } BodyListStmt { dumpScope(); }
;

ScopeStmt
    : { pushScope(); } BodyListStmt { dumpScope(); } END_BRACKET
;

/* Scope Body */
BodyListStmt
    : BodyListStmt BodyStmt
    | BodyStmt
;

BodyStmt
    : OperationStmt
    | ConditionStmt
;

/* Condition and Operation */
ConditionStmt
    : FOR ExpressionStmt TIMES { code_forLoop(&$<obj_val>2); } ScopeStmt { code_forLoopEnd(&$<obj_val>2); }
;

OperationStmt
    : DefineStmt PRINT { code_stdoutPrint(&$<obj_val>1, true); }
    | DefineStmt AS IDENT { code_createVariable(&$<obj_val>1, $<s_var>3); }
;

DefineStmt
    : IMMEDIATE STRING IS ExpressionStmt { $$ = $<obj_val>4; }
    | IMMEDIATE NUMBER IS ExpressionStmt { $$ = $<obj_val>4; }
    | IMMEDIATE_NUMBER_IS ExpressionStmt { $$ = $<obj_val>2; }
;

ExpressionStmt
    : ValueStmt { $$ = $<obj_val>1; }
;


/* Value */
ValueStmt
    : STR_LIT { $$ = object_createStr($<s_var>1); }
    | NUMBER_LIT { $$ = object_createNumber(&$<n_var>1); }
    | IDENT { if (($$ = object_findIdentByName($<s_var>1)).type == OBJECT_TYPE_UNDEFINED) YYABORT; }
;

%%

void yyerror(char const* msg) {
    compileError = true;
    fprintf(stderr, ERROR_PREFIX " %s\n", inputFilePath, yylineno, yycolumnUtf8 - yylengUtf8 + 1, msg);
    printErrorLine();
}

static const char* yysymbolNameCh(yysymbol_kind_t symbol) {
    switch (symbol) {
        case YYSYMBOL_NUMBER_LIT: return "數值";
        case YYSYMBOL_STR_LIT: return "字串";
        case YYSYMBOL_NUMBER: return "『一數』";
        case YYSYMBOL_STRING: return "『一言』";
        default: return "未知";
    }
}

static int yyreport_syntax_error(const yypcontext_t *ctx) {
    compileError = true;
    fprintf(stderr, ERROR_PREFIX, inputFilePath, yylineno, yycolumnUtf8 - yylengUtf8 + 1);
    
    // expecting token
    yysymbol_kind_t lookahead = yypcontext_token(ctx);
    if (lookahead != YYSYMBOL_YYEMPTY) {
        fprintf(stderr, "忽逢%s，殊非所期", yysymbolNameCh(lookahead));
    }
    
    enum { TOKENMAX = 10 };
    yysymbol_kind_t expected[TOKENMAX];
    int n = yypcontext_expected_tokens(ctx, expected, TOKENMAX);
    if (n > 0) {
        fprintf(stderr, "，當得");
        for (int i = 0; i < n; ++i) {
            if (i > 0) fprintf(stderr, "或");
            fprintf(stderr, "%s", yysymbolNameCh(expected[i]));
        }
    }
    fprintf(stderr, "\n");
    
    printErrorLine();
    return 0;
}