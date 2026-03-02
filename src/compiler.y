/* Definition section */
%{
    #include "compiler_util.h"
    #include "compiler_common.h"
    #include "main.h"
    #include "object.h"
    #include "value_data.h"

    void yyerror(char const* msg);
%}

%define parse.error custom

/* Variable or self-defined structure */
%union {
    ObjectType var_type;

    bool b_var;
    ScientificNotation n_var;
    char *s_var;

    Object obj_val;
    ValueData val_data;
    LinkedList ident_list;
    
    bool exp_left;
    char exp_op;
}
/* Token */
%token PRINT
%token HERE_ARE HERE_IS_A SAID NAME_IT
%token FOR TIMES END_BRACKET IF
%token PAST VARIABLE ASSIGN THAT TO_IT
%token PREPOSITION_LEFT PREPOSITION_RIGHT
%token NEWLINE STR_BEGIN

%token <exp_op> EXP_OPERATION
%token <exp_left> EXP_PREPOSITION

%token <var_type> VAR_TYPE
%token <b_var> BOOL_LIT
%token <n_var> NUMBER_LIT
%token <s_var> STR_LIT
%token <s_var> IDENT

/* Nonterminal with return, which need to specify type */
%type <obj_val> ExpressionStmt
%type <obj_val> ExpressionOrValueStmt
%type <obj_val> ValueLiteralStmt
%type <obj_val> VariableStmt
%type <val_data> CreateValueDataListStmt

%nonassoc THEN
%nonassoc ELSE

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
    | IF IDENT THEN BodyStmt
    | IF IDENT THEN BodyStmt ELSE BodyStmt
;

/* Condition and Operation */
ConditionStmt
    : FOR ExpressionOrValueStmt TIMES { code_forLoop(&$<obj_val>2); } ScopeStmt { code_forLoopEnd(&$<obj_val>2); }
;

OperationStmt
    : CreateValueDataListStmt PrintStmt { object_ValueDataListFree(&$<val_data>1); }
    | CreateValueDataListStmt NAME_IT VariableDefineStmt { object_ValueDataListFree(&$<val_data>1); }
    | PAST VariableStmt VARIABLE ASSIGN ExpressionOrValueStmt { if (code_assign(&$<obj_val>2, &$<obj_val>5)) YYABORT; } TO_IT
    | ExpressionStmt PAST VariableStmt { if (code_assign(&$<obj_val>3, &$<obj_val>1)) YYABORT; } VARIABLE ASSIGN THAT TO_IT
;

PrintStmt
    : PRINT { code_stdoutPrint(&$<val_data>0, true); }
    | PrintStmt PRINT { code_stdoutPrint(&$<val_data>0, true); }
;

VariableDefineStmt
    : VariableDefineStmt SAID IDENT { code_createVariable(&$<val_data>-1, $<s_var>3); } 
    | IDENT { code_createVariable(&$<val_data>-1, $<s_var>1); } 
;

CreateValueDataListStmt:
    // 有數( 一 |「甲」)
    HERE_IS_A VAR_TYPE { object_ValueDataListCreate($<var_type>2, &$<val_data>$); printf("%p\n", $<val_data>$.valueList.head); }
        ExpressionOrValueStmt  { if (object_ValueDataListAdd(&$<val_data>3, &$<obj_val>4)) YYABORT; $$ = $<val_data>3; }
    
    // (吾有|今有)三數。曰一。曰三。曰五
    | HERE_ARE NUMBER_LIT VAR_TYPE { object_ValueDataListCreate($<var_type>3, &$<val_data>$); }
        CreateValueDataList_AddValueDataStmt { $$ = $<val_data>4; }
    
    // 加一於二
    | ExpressionStmt 
        { object_ValueDataListCreate($<obj_val>1.type, &$<val_data>$); if (object_ValueDataListAdd(&$<val_data>$, &$<obj_val>1)) YYABORT; }
;

CreateValueDataList_AddValueDataStmt
    : CreateValueDataList_AddValueDataStmt SAID ExpressionOrValueStmt { if (object_ValueDataListAdd(&$<val_data>0, &$<obj_val>3)) YYABORT; }
    | SAID ExpressionOrValueStmt { if (object_ValueDataListAdd(&$<val_data>0, &$<obj_val>2)) YYABORT; }
;

ExpressionOrValueStmt
    : ExpressionStmt
    | ValueLiteralStmt
;

ExpressionStmt
    : EXP_OPERATION ExpressionOrValueStmt EXP_PREPOSITION ExpressionOrValueStmt 
        { $$ = code_expression($<exp_op>1, $<exp_left>3, &$<obj_val>2, &$<obj_val>4); }
;

/* Value */
ValueLiteralStmt
    : STR_BEGIN STR_LIT { $$ = object_createStr($<s_var>2); }
    | NUMBER_LIT { if (($$ = object_createNumber(&$<n_var>1)).type == OBJECT_TYPE_UNDEFINED) YYABORT; }
    | VariableStmt
;

VariableStmt
    : IDENT { if (($$ = object_findIdentByName($<s_var>1)).type == OBJECT_TYPE_UNDEFINED) YYABORT; }
;

%%

void yyerror(char const* msg) {
    compileError = true;
    fprintf(stderr, ERROR_PREFIX " %s\n", inputFilePath, yylineno, yycolumnUtf8 - yylengUtf8 + 1, msg);
    printErrorLine();
}

static const char* yysymbolNameCh(yysymbol_kind_t symbol) {
    switch (symbol) {
        case YYSYMBOL_NUMBER_LIT: return " 數值 ";
        case YYSYMBOL_STR_LIT: return " 字串 ";
        case YYSYMBOL_VAR_TYPE: return " 類 ";
        default: return yysymbol_name (symbol);
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