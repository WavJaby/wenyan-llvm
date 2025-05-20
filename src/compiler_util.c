#include "compiler_util.h"

#include <utf8.c/utf8.h>

void checkNewline(char* str, size_t len) {
    if (str[len - 2] == '\r') {
        str[len - 2] = '\n';
        str[len - 1] = '\0';
    }
}

int decode_utf8(const char* str, const uint8_t byte_len) {
    uint8_t c = (uint8_t)str[0];
    switch (byte_len) {
    case 1:
        return c;
    case 2:
        if ((str[1] & 0xC0) != 0x80) return -1;
        return ((c & 0x1F) << 6) | (str[1] & 0x3F);
    case 3:
        if ((str[1] & 0xC0) != 0x80 || (str[2] & 0xC0) != 0x80) return -1;
        return ((c & 0x0F) << 12) | ((str[1] & 0x3F) << 6) | (str[2] & 0x3F);
    case 4:
        if ((str[1] & 0xC0) != 0x80 || (str[2] & 0xC0) != 0x80 || (str[3] & 0xC0) != 0x80) return -1;
        return ((c & 0x07) << 18) | ((str[1] & 0x3F) << 12) | ((str[2] & 0x3F) << 6) | (str[3] & 0x3F);
    default:
        return -1; // Invalid UTF-8
    }
}

bool is_fullwidth(const int code) {
    return (code >= 0xFF00 && code <= 0xFFEF) || // Halfwidth and Fullwidth Forms
        (code >= 0x4E00 && code <= 0x9FFF) || // CJK Unified Ideographs
        (code >= 0x3400 && code <= 0x4DBF); // CJK Unified Ideographs Extension A
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

    // calculate width
    int prefixWidth = startIndex, tokenWidth = yyleng;
    utf8_string utf8Str = make_utf8_string(cache);
    if (utf8Str.str) {
        prefixWidth = 0;

        utf8_char_iter iter = make_utf8_char_iter(utf8Str);
        utf8_char c;
        while ((c = next_utf8_char(&iter)).byte_len) {
            const int code = decode_utf8(c.str, c.byte_len);
            // printf("%x\n", code);
            prefixWidth += code != -1 && is_fullwidth(code) ? 2 : 1;
        }
    }
    utf8Str = make_utf8_string(token);
    if (utf8Str.str) {
        tokenWidth = 0;

        utf8_char_iter iter = make_utf8_char_iter(utf8Str);
        utf8_char c;
        while ((c = next_utf8_char(&iter)).byte_len) {
            const int code = decode_utf8(c.str, c.byte_len);
            // printf("%x\n", code);
            tokenWidth += code != -1 && is_fullwidth(code) ? 2 : 1;
        }
    }

    // Print the error line
    printf("%6d |%s" COLOR_RED "%s" COLOR_RESET "%s", yylineno, cache, token, cache + yycolumn);
    printf("       |%*.s" COLOR_RED "^", prefixWidth, "");
    for (size_t i = 1; i < tokenWidth; i++) printf("~");
    printf(COLOR_RESET "\n");

    // Read additional context
    cache[0] = 0;
    fgets(cache, ERROR_TEXT_BUFFER_LEN, yyin);
    size_t len = strlen(cache);
    if (!len) return;
    checkNewline(cache, len);
    len = strlen(cache);
    if (cache[len - 1] == '\n' && len == 1)
        return;
    printf("%6d |%s", yylineno + 1, cache);
}
