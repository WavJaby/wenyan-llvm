#ifndef COMPILER_UTIL_H
#define COMPILER_UTIL_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "lib/byte_buffer.h"

// Internal variable
extern int yylineno;
extern int yyleng;
extern int yychar;
extern FILE* yyout;
extern FILE* yyin;
extern int yyparse();
extern int yylex();
extern int yylex_destroy();

// Custom variable
extern int yycolumn;
extern int yyoffset;
extern int yycolumnUtf8;
extern int yylengUtf8;

extern char *inputFilePath, *inputFileName;
extern ByteBuffer methodBuff, constBuff, mainFunBuff;
extern bool compileError;
extern int scopeLevel;

#define ERROR_PREFIX "%s:%d:%d: 錯誤: "
#define ERROR_TEXT_BUFFER_LEN 128
#define ERROR_TOKEN_BUFFER_LEN 64
#define COLOR_RED "\033[31m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_RESET "\033[0m"
#define SCOPE_SPACE_FMT "%*s"
#define SCOPE_SPACE_VAL scopeLevel << 2, ""

#define code(format, ...) \
fprintf(yyout, SCOPE_SPACE_FMT format "\n", SCOPE_SPACE_VAL, __VA_ARGS__)
#define codeRaw(code) \
fprintf(yyout, SCOPE_SPACE_FMT code "\n", SCOPE_SPACE_VAL)

#define yyerroraf(format, ...)                                                                        \
    {                                                                                                 \
        compileError = true;                                                                          \
        fprintf(stderr, ERROR_PREFIX format, inputFileName, yylineno, yycolumnUtf8 - yylengUtf8 + 1, ##__VA_ARGS__); \
        printErrorLine();                                                                             \
        YYABORT;                                                                                      \
    }
#define yyerrorf(format, ...)                                                                         \
    {                                                                                                 \
        compileError = true;                                                                          \
        fprintf(stderr, ERROR_PREFIX format, inputFileName, yylineno, yycolumnUtf8 - yylengUtf8 + 1, ##__VA_ARGS__); \
        printErrorLine();                                                                             \
    }

void printErrorLine();

#endif
