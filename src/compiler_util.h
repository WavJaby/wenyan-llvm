#ifndef COMPILER_UTIL_H
#define COMPILER_UTIL_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "lib/byte_buffer.h"

extern int yylineno;
extern int yycolumn;
extern int yyoffset;
extern int yyleng;
extern FILE* yyout;
extern FILE* yyin;
extern int yyparse();
extern int yylex();
extern int yylex_destroy();

extern char* inputFilePath, *inputFileName;
extern ByteBuffer methodByteBuff, constantByteBuff, mainFunByteBuff;
extern bool compileError;
extern int scopeLevel;


#define code(format, ...) \
fprintf(yyout, "%*s" format "\n", scopeLevel << 2, "", __VA_ARGS__)
#define codeRaw(code) \
fprintf(yyout, "%*s" code "\n", scopeLevel << 2, "")

#define ERROR_PREFIX "%s:%d:%d: 錯誤: "
#define ERROR_TEXT_BUFFER_LEN 128
#define ERROR_TOKEN_BUFFER_LEN 64
#define COLOR_RED "\033[31m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_RESET "\033[0m"

#define yyerrorf(format, ...)                                                                         \
    {                                                                                                 \
        compileError = true;                                                                          \
        printf(ERROR_PREFIX format, inputFileName, yylineno, yycolumn - yyleng + 1, ##__VA_ARGS__); \
        printErrorLine();                                                                             \
        YYABORT;                                                                                      \
    }

void printErrorLine();

void yyerror(char const* msg);

#endif