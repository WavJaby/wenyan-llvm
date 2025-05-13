#include "byte_buffer.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "WJCL/string/wjcl_string.h"

#define BUFFER_INIT_SIZE 1024
#define BUFFER_GROW_FACTOR 2

void byteBufferAddLen(ByteBuffer* byteBuffer, size_t len) {
    byteBuffer->len += len;
    if (byteBuffer->len > byteBuffer->bufLen) {
        if (byteBuffer->bufLen)
            byteBuffer->bufLen *= BUFFER_GROW_FACTOR;
        else
            byteBuffer->bufLen = BUFFER_INIT_SIZE;
        void* buf = (uint8_t*)realloc(byteBuffer->buf, byteBuffer->bufLen);

        if (buf == NULL) {
            fprintf(stderr, "ByteBuffer: realloc failed\n");
            exit(1);
        }

        byteBuffer->buf = buf;
    }
}

char to_hex_char(uint8_t digit) {
    if (digit < 10)
        return digit + '0';
    return digit - 10 + 'A';
}

void byteBufferWriteStrUtf8(ByteBuffer* byteBuffer, uint8_t* str) {
    while (*str) {
        const size_t offset = byteBuffer->len;
        if (*str >= 0x7F || *str < ' ') {
            byteBufferAddLen(byteBuffer, 3);
            *(byteBuffer->buf + offset) = '\\';
            *(byteBuffer->buf + offset + 1) = to_hex_char((*str >> 4) & 0xF);
            *(byteBuffer->buf + offset + 2) = to_hex_char((*str) & 0xF);
        } else {
            byteBufferAddLen(byteBuffer, 1);
            *(byteBuffer->buf + offset) = *str;
        }
        str++;
    }
}

void byteBufferWriteFormat(ByteBuffer* byteBuffer, char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    string constStr = strFromFormatV(fmt, ap);
    va_end(ap);
    byteBufferWriteStr(byteBuffer, constStr);
    strFree(constStr);
}

void byteBufferWriteStr(ByteBuffer* byteBuffer, char* str) {
    byteBufferWrite(byteBuffer, (uint8_t*)str, strlen(str));
}

void byteBufferWriteI16(ByteBuffer* byteBuffer, uint16_t data) {
    const size_t offset = byteBuffer->len;
    byteBufferAddLen(byteBuffer, 2);
    *(byteBuffer->buf + offset) = (data >> 8) & 0xFF;
    *(byteBuffer->buf + offset + 1) = data & 0xFF;
}

void byteBufferWrite(ByteBuffer* byteBuffer, uint8_t* arr, size_t size) {
    const size_t offset = byteBuffer->len;
    byteBufferAddLen(byteBuffer, size);
    memcpy(byteBuffer->buf + offset, arr, size);
}

void byteBufferWriteToFile(ByteBuffer* byteBuffer, FILE* file) {
    fwrite(byteBuffer->buf, 1, byteBuffer->len, file);
}

void byteBufferClear(ByteBuffer* byteBuffer) {
    byteBuffer->bufLen = BUFFER_INIT_SIZE;
    byteBuffer->buf = (uint8_t*)realloc(byteBuffer->buf, byteBuffer->bufLen);
    byteBuffer->len = 0;
}

void byteBufferFree(ByteBuffer* byteBuffer, bool self) {
    free(byteBuffer->buf);
    if (self) free(byteBuffer);
}
