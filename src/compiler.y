/* Definition section */
%{
    #include "compiler_util.h"
    #include "compiler_common.h"
    #include "main.h"

    int yydebug = 1;

    ObjectType variableIdentType;
%}

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
%token SYNTEX_ERROR
%token PRINT
%token IMMEDIATE IS CALLED NUMBER STIRNG IMMEDIATE_NUMBER_IS
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

%left ','
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
%right NOT BNT '(' ')'
%left INC_ASSIGN DEC_ASSIGN '[' ']' '{' '}'

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
    : ValueStmt PRINT {
        if (code_stdoutPrint(&$<obj_val>1)) {
            if ($<obj_val>1.type == OBJECT_TYPE_IDENT)
                yyerrorf("「%s」未定義\n", $<obj_val>1.str);
        }
    }
    | ValueStmt CALLED IDENT { code_createVariable(&$<obj_val>1, $<s_var>3); }
;

/*
DefineVariableStmt
    : VARIABLE_T IDENT VAL_ASSIGN Expression ';'
;*/

/* Scope */
ScopeStmt
    : '{' { pushScope(); } StmtList '}' { dumpScope(); }
    | '{' { pushScope(); } '}' { dumpScope(); }
    | BodyStmt
;
ManualScopeStmt
    : '{' StmtList '}'
    | '{' '}'
    | BodyStmt
;
StmtList 
    : StmtList ScopeStmt
    | ScopeStmt
;
BodyStmt
    : ';'
;

ValueStmt
    : IMMEDIATE STIRNG IS STR_LIT { $$ = (Object){OBJECT_TYPE_STR, .str = $<s_var>4}; printf("STR_LIT \"%s\"\n", $<s_var>4); }
    | IMMEDIATE NUMBER IS NUMBER_LIT { $$ = (Object){OBJECT_TYPE_NUMBER, .number = $<number_var>4}; }
    | IMMEDIATE_NUMBER_IS NUMBER_LIT { $$ = (Object){OBJECT_TYPE_NUMBER, .number = $<number_var>2}; }
    | IMMEDIATE STIRNG IS IDENT { $$ = (Object){OBJECT_TYPE_IDENT, .str = $<s_var>4}; }
;

%%
/* C code section */