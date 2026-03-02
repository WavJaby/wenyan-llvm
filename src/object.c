//
// Created by WavJaby on 2026/3/2.
//

#include "object.h"

const ObjectType numberType2objectType[] = {
    [I32] = OBJECT_TYPE_I32,
    [I64] = OBJECT_TYPE_I64,
    [F64] = OBJECT_TYPE_F64,
};
const char* objectType2llvmType[] = {
    [OBJECT_TYPE_I32] = "i32",
    [OBJECT_TYPE_I64] = "i64",
    [OBJECT_TYPE_F64] = "double",
};
const char* objectType2strFormat[] = {
    [OBJECT_TYPE_I32] = "%d",
    [OBJECT_TYPE_I64] = "%lld",
    [OBJECT_TYPE_F64] = "%.16g",
};
const char* objectType2str[] = {
    [OBJECT_TYPE_NUM] = "數值",
    [OBJECT_TYPE_I32] = "全數",
    [OBJECT_TYPE_I64] = "長數",
    [OBJECT_TYPE_F64] = "浮數",
    [OBJECT_TYPE_STR] = "字串",
    [OBJECT_TYPE_IDENT] = "識名",
    [OBJECT_TYPE_UNDEFINED] = "無定",
};