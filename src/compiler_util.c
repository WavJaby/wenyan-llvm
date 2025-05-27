#include "compiler_util.h"

#include <utf8.c/utf8.h>

void checkNewline(char* str, size_t len) {
    if (str[len - 2] == '\r') {
        str[len - 2] = '\n';
        str[len - 1] = '\0';
    }
}

bool isFullWidth(const uint32_t c) {
    return
        c == 0x2329u || c == 0x232Au || c == 0x23F0u || c == 0x23F3u || c == 0x267Fu || c == 0x2693u || c == 0x26A1u ||
        c == 0x26CEu || c == 0x26D4u || c == 0x26EAu || c == 0x26F5u || c == 0x26FAu || c == 0x26FDu || c == 0x2705u ||
        c == 0x2728u || c == 0x274Cu || c == 0x274Eu || c == 0x2757u || c == 0x27B0u || c == 0x27BFu || c == 0x2B50u ||
        c == 0x2B55u || c == 0x3000u || c == 0x3004u || c == 0x3005u || c == 0x3006u || c == 0x3007u || c == 0x3008u ||
        c == 0x3009u || c == 0x300Au || c == 0x300Bu || c == 0x300Cu || c == 0x300Du || c == 0x300Eu || c == 0x300Fu ||
        c == 0x3010u || c == 0x3011u || c == 0x3014u || c == 0x3015u || c == 0x3016u || c == 0x3017u || c == 0x3018u ||
        c == 0x3019u || c == 0x301Au || c == 0x301Bu || c == 0x301Cu || c == 0x301Du || c == 0x3020u || c == 0x3030u ||
        c == 0x303Bu || c == 0x303Cu || c == 0x303Du || c == 0x303Eu || c == 0x309Fu || c == 0x30A0u || c == 0x30FBu ||
        c == 0x30FFu || c == 0x3250u || c == 0xA015u || c == 0xFE17u || c == 0xFE18u || c == 0xFE19u || c == 0xFE30u ||
        c == 0xFE35u || c == 0xFE36u || c == 0xFE37u || c == 0xFE38u || c == 0xFE39u || c == 0xFE3Au || c == 0xFE3Bu ||
        c == 0xFE3Cu || c == 0xFE3Du || c == 0xFE3Eu || c == 0xFE3Fu || c == 0xFE40u || c == 0xFE41u || c == 0xFE42u ||
        c == 0xFE43u || c == 0xFE44u || c == 0xFE47u || c == 0xFE48u || c == 0xFE58u || c == 0xFE59u || c == 0xFE5Au ||
        c == 0xFE5Bu || c == 0xFE5Cu || c == 0xFE5Du || c == 0xFE5Eu || c == 0xFE62u || c == 0xFE63u || c == 0xFE68u ||
        c == 0xFE69u || c == 0xFF04u || c == 0xFF08u || c == 0xFF09u || c == 0xFF0Au || c == 0xFF0Bu || c == 0xFF0Cu ||
        c == 0xFF0Du || c == 0xFF3Bu || c == 0xFF3Cu || c == 0xFF3Du || c == 0xFF3Eu || c == 0xFF3Fu || c == 0xFF40u ||
        c == 0xFF5Bu || c == 0xFF5Cu || c == 0xFF5Du || c == 0xFF5Eu || c == 0xFF5Fu || c == 0xFF60u || c == 0xFFE2u ||
        c == 0xFFE3u || c == 0xFFE4u ||
        (0x1100u <= c && c <= 0x115Fu) || (0x231Au <= c && c <= 0x231Bu) || (0x23E9u <= c && c <= 0x23ECu) ||
        (0x25FDu <= c && c <= 0x25FEu) || (0x2614u <= c && c <= 0x2615u) || (0x2648u <= c && c <= 0x2653u) ||
        (0x26AAu <= c && c <= 0x26ABu) || (0x26BDu <= c && c <= 0x26BEu) || (0x26C4u <= c && c <= 0x26C5u) ||
        (0x26F2u <= c && c <= 0x26F3u) || (0x270Au <= c && c <= 0x270Bu) || (0x2753u <= c && c <= 0x2755u) ||
        (0x2795u <= c && c <= 0x2797u) || (0x2B1Bu <= c && c <= 0x2B1Cu) || (0x2E80u <= c && c <= 0x2E99u) ||
        (0x2E9Bu <= c && c <= 0x2EF3u) || (0x2F00u <= c && c <= 0x2FD5u) || (0x2FF0u <= c && c <= 0x2FFBu) ||
        (0x3001u <= c && c <= 0x3003u) || (0x3012u <= c && c <= 0x3013u) || (0x301Eu <= c && c <= 0x301Fu) ||
        (0x3021u <= c && c <= 0x3029u) || (0x302Au <= c && c <= 0x302Du) || (0x302Eu <= c && c <= 0x302Fu) ||
        (0x3031u <= c && c <= 0x3035u) || (0x3036u <= c && c <= 0x3037u) || (0x3038u <= c && c <= 0x303Au) ||
        (0x3041u <= c && c <= 0x3096u) || (0x3099u <= c && c <= 0x309Au) || (0x309Bu <= c && c <= 0x309Cu) ||
        (0x309Du <= c && c <= 0x309Eu) || (0x30A1u <= c && c <= 0x30FAu) || (0x30FCu <= c && c <= 0x30FEu) ||
        (0x3105u <= c && c <= 0x312Fu) || (0x3131u <= c && c <= 0x318Eu) || (0x3190u <= c && c <= 0x3191u) ||
        (0x3192u <= c && c <= 0x3195u) || (0x3196u <= c && c <= 0x319Fu) || (0x31A0u <= c && c <= 0x31BFu) ||
        (0x31C0u <= c && c <= 0x31E3u) || (0x31F0u <= c && c <= 0x31FFu) || (0x3200u <= c && c <= 0x321Eu) ||
        (0x3220u <= c && c <= 0x3229u) || (0x322Au <= c && c <= 0x3247u) || (0x3251u <= c && c <= 0x325Fu) ||
        (0x3260u <= c && c <= 0x327Fu) || (0x3280u <= c && c <= 0x3289u) || (0x328Au <= c && c <= 0x32B0u) ||
        (0x32B1u <= c && c <= 0x32BFu) || (0x32C0u <= c && c <= 0x32FFu) || (0x3300u <= c && c <= 0x33FFu) ||
        (0x3400u <= c && c <= 0x4DBFu) || (0x4E00u <= c && c <= 0x9FFCu) || (0x9FFDu <= c && c <= 0x9FFFu) ||
        (0xA000u <= c && c <= 0xA014u) || (0xA016u <= c && c <= 0xA48Cu) || (0xA490u <= c && c <= 0xA4C6u) ||
        (0xA960u <= c && c <= 0xA97Cu) || (0xAC00u <= c && c <= 0xD7A3u) || (0xF900u <= c && c <= 0xFA6Du) ||
        (0xFA6Eu <= c && c <= 0xFA6Fu) || (0xFA70u <= c && c <= 0xFAD9u) || (0xFADAu <= c && c <= 0xFAFFu) ||
        (0xFE10u <= c && c <= 0xFE16u) || (0xFE31u <= c && c <= 0xFE32u) || (0xFE33u <= c && c <= 0xFE34u) ||
        (0xFE45u <= c && c <= 0xFE46u) || (0xFE49u <= c && c <= 0xFE4Cu) || (0xFE4Du <= c && c <= 0xFE4Fu) ||
        (0xFE50u <= c && c <= 0xFE52u) || (0xFE54u <= c && c <= 0xFE57u) || (0xFE5Fu <= c && c <= 0xFE61u) ||
        (0xFE64u <= c && c <= 0xFE66u) || (0xFE6Au <= c && c <= 0xFE6Bu) || (0xFF01u <= c && c <= 0xFF03u) ||
        (0xFF05u <= c && c <= 0xFF07u) || (0xFF0Eu <= c && c <= 0xFF0Fu) || (0xFF10u <= c && c <= 0xFF19u) ||
        (0xFF1Au <= c && c <= 0xFF1Bu) || (0xFF1Cu <= c && c <= 0xFF1Eu) || (0xFF1Fu <= c && c <= 0xFF20u) ||
        (0xFF21u <= c && c <= 0xFF3Au) || (0xFF41u <= c && c <= 0xFF5Au) || (0xFFE0u <= c && c <= 0xFFE1u) ||
        (0xFFE5u <= c && c <= 0xFFE6u);
}

uint32_t decode_utf8(const char* str, const uint8_t byte_len) {
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
        return 0; // Invalid UTF-8
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

    // calculate width
    int prefixWidth = startIndex, tokenWidth = yyleng;
    utf8_string utf8Str = make_utf8_string(cache);
    if (utf8Str.str) {
        prefixWidth = 0;

        utf8_char_iter iter = make_utf8_char_iter(utf8Str);
        utf8_char c;
        while ((c = next_utf8_char(&iter)).byte_len) {
            const uint32_t code = decode_utf8(c.str, c.byte_len);
            // printf("%x\n", code);
            prefixWidth += code != 0 && isFullWidth(code) ? 2 : 1;
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
            tokenWidth += code != -1 && isFullWidth(code) ? 2 : 1;
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
