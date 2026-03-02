//
// Created by WavJaby on 2026/3/2.
//

#ifndef WENYAN_LLVM_VALUE_DATA_H
#define WENYAN_LLVM_VALUE_DATA_H


#include "object.h"
#include "WJCL/list/wjcl_linked_list.h"

/**
 * For single or multiple variable declaration
 */
typedef struct {
    /** LinkedList<@link Object> */
    LinkedList valueList;
    ObjectType valueType;
} ValueData;


/**
 * Create init ValueData
 * @param valueType
 * @param valueType
 * @param valueData ValueData* output
 * @return false if success
 */
bool object_ValueDataListCreate(ObjectType valueType, ValueData* valueData);
/**
 * Add Value to ValueData
 * @param valueData ValueData*
 * @param obj Object* output
 * @return false if success
 */
bool object_ValueDataListAdd(ValueData* valueData, const Object* obj);

/** 
 * Pop Value in ValueData
 * @param valueData ValueData*
 * @return Object*
 */
Object* object_ValueDataListPop(ValueData* valueData);

bool object_ValueDataListFree(ValueData* valueData);

#endif //WENYAN_LLVM_VALUE_DATA_H

