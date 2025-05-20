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
    ScientificNotation number_var;
    char *s_var;

    Object obj_val;

    // LinkList<Object*>
    // LinkedList* array_subscript;
}
/* Token without return */
%token PRINT
%token IMMEDIATE IS CALLED NUMBER STRING IMMEDIATE_NUMBER_IS
%token SHR SHL BAN BOR BNT BXO ADD SUB MUL DIV REM NOT GTR LES GEQ LEQ EQL NEQ LAN LOR
%token VAL_ASSIGN ADD_ASSIGN SUB_ASSIGN MUL_ASSIGN DIV_ASSIGN REM_ASSIGN BAN_ASSIGN BOR_ASSIGN BXO_ASSIGN SHR_ASSIGN SHL_ASSIGN INC_ASSIGN DEC_ASSIGN
%token IF ELSE FOR WHILE RETURN BREAK CONTINUE

/* Token with return, which need to sepcify type */
%token <var_type> VARIABLE_T
%token <b_var> BOOL_LIT
%token <c_var> CHAR_LIT
%token <number_var> NUMBER_LIT
%token <s_var> STR_LIT
%token <s_var> IDENT

/* Nonterminal with return, which need to sepcify type */
%type <obj_val> ValueStmt

%right VAL_ASSIGN ADD_ASSIGN SUB_ASSIGN MUL_ASSIGN DIV_ASSIGN REM_ASSIGN BAN_ASSIGN BOR_ASSIGN BXO_ASSIGN SHR_ASSIGN SHL_ASSIGN
%left LOR
%left LAN
%left BOR
%left BXO
%left BAN
%left EQL NEQ
%left LES LEQ GTR GEQ
%left SHL SHR
%left ADD SUB
%left MUL DIV REM
%right NOT BNT
%left INC_ASSIGN DEC_ASSIGN

%nonassoc THEN
%nonassoc ELSE

/* Yacc will start at this nonterminal */
%start Program
%%
/* Grammar section */

Program
    : { pushScope(); } GlobalStmtList { dumpScope(); }
    | /* Empty file */
;

GlobalStmtList 
    : GlobalStmtList GlobalStmt
    | GlobalStmt
;

GlobalStmt
    : ValueStmt PRINT { code_stdoutPrint(&$<obj_val>1); }
    | ValueStmt CALLED IDENT { code_createVariable(&$<obj_val>1, $<s_var>3); free($<s_var>3); }
;

ValueStmt
    : IMMEDIATE STRING IS STR_LIT { $$ = (Object){OBJECT_TYPE_STR, .str = $<s_var>4}; printf("STR_LIT \"%s\"\n", $<s_var>4); }
    | IMMEDIATE NUMBER IS NUMBER_LIT { $$ = createNumberObject(&$<number_var>4); }
    | IMMEDIATE_NUMBER_IS NUMBER_LIT { $$ = createNumberObject(&$<number_var>2); }
    | IMMEDIATE STRING IS IDENT {
        $$ = findIdentByName($<s_var>4);
        if ($$.type == OBJECT_TYPE_UNDEFINED)
            yyerroraf("「%s」未定義\n", $<s_var>4);
        free($<s_var>4);
    }
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