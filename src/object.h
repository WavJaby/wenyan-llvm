//
// Created by WavJaby on 2026/3/2.
//

#ifndef WENYAN_LLVM_OBJECT_H
#define WENYAN_LLVM_OBJECT_H


#include "lib/chinese_number.h"

typedef enum {
    OBJECT_TYPE_UNDEFINED,
    
    OBJECT_TYPE_ARRAY,
    OBJECT_TYPE_BOOL,
    OBJECT_TYPE_NUM,
    OBJECT_TYPE_STR,
    
    OBJECT_TYPE_I32,
    OBJECT_TYPE_I64,
    // OBJECT_TYPE_F32,
    OBJECT_TYPE_F64,
    
    OBJECT_TYPE_IDENT,
} ObjectType;

typedef struct {
    ObjectType type;
    char* name;
    int32_t index;
    bool expCache;
} SymbolData;

typedef struct {
    ObjectType type;
    // uint32_t array;
    char* str;
    ScientificNotation* number;
    // uint8_t flag;
    SymbolData* symbol;
    // LinkedList* arraySubscript;
} Object;

extern const ObjectType numberType2objectType[];
extern const char* objectType2llvmType[];
extern const char* objectType2strFormat[];
extern const char* objectType2str[];

#endif //WENYAN_LLVM_OBJECT_H
