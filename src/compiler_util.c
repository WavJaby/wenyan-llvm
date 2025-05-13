#include "compiler_util.h"

#include <utf8.c/utf8.h>

void checkNewline(char* str, size_t len) {
    if (str[len - 1] != '\n') {
        str[len] = '\n';
        str[len + 1] = 0;
    }
}

void printErrorLine() {
    // Read error line
    fseek(yyin, yyoffset - yycolumn, SEEK_SET);

    char cache[ERROR_TEXT_BUFFER_LEN + 2], token[ERROR_TOKEN_BUFFER_LEN + 1];
    cache[0] = 0;

    // Read the error line from input file.
    fgets(cache, ERROR_TEXT_BUFFER_LEN, yyin);
    checkNewline(cache, strlen(cache));

    // Extract the error token from the line.
    int startIndex = yycolumn - yyleng;
    memcpy(token, cache + startIndex, yyleng);
    token[yyleng] = 0;
    // Split line
    cache[startIndex] = 0;

    // Print the error line
    printf("%6d |%s" COLOR_RED "%s" COLOR_RESET "%s", yylineno, cache, token, cache + yycolumn);
    printf("       |%*.s" COLOR_RED "^", startIndex, "");
    for (size_t i = 1; i < yyleng; i++) printf("~");
    printf(COLOR_RESET "\n");

    // Read additional context
    cache[0] = 0;
    fgets(cache, ERROR_TEXT_BUFFER_LEN, yyin);
    size_t len = strlen(cache);
    if (!len) return;
    checkNewline(cache, len);
    printf("%6d |%s", yylineno + 1, cache);
}

void yyerror(char const* msg) {
    compileError = true;

    printf(ERROR_PREFIX " %s\n", inputFilePath, yylineno, yycolumn - yyleng + 1, msg);
    printErrorLine();
}