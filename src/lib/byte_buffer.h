#ifndef BYTE_BUFFER_H
#define BYTE_BUFFER_H
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>


typedef struct ByteBuffer {
    uint8_t* buf;
    size_t bufLen;
    size_t len;
} ByteBuffer;

#define byteBufferInit() \
{ NULL, 0, 0 }
#define byteBufferNew() calloc(1, sizeof(ByteBuffer))

void byteBufferWriteI16(ByteBuffer* byteBuffer, uint16_t data);

void byteBufferWriteStrUtf8(ByteBuffer* byteBuffer, uint8_t* str);

void byteBufferWriteStr(ByteBuffer* byteBuffer, char* str);

void byteBufferWriteFormat(ByteBuffer* byteBuffer, char* fmt, ...);

void byteBufferWrite(ByteBuffer* byteBuffer, uint8_t* arr, size_t size);

void byteBufferWriteToFile(ByteBuffer* byteBuffer, FILE* file);

void byteBufferClear(ByteBuffer* byteBuffer);

void byteBufferFree(ByteBuffer* byteBuffer, bool self);


#endif //BYTE_BUFFER_H
