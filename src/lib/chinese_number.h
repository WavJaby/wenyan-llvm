#ifndef CHINESE_NUMBER_H
#define CHINESE_NUMBER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    ERROR,
    I32,
    I64,
    F64,
} NumberType;

typedef struct {
    NumberType type;
    int64_t fraction;
    uint32_t fractionLen;
    int exp;
} ScientificNotation;

// Convert a Chinese numeral string (UTF-8) to its numeric value
bool chineseToArabic(const char* utf8, ScientificNotation* sciOut);

char* sciToStr(const ScientificNotation* sci);
double sciToDouble(const ScientificNotation* sci);

#endif //CHINESE_NUMBER_H
