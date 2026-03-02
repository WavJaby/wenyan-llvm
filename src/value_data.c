//
// Created by WavJaby on 2026/3/2.
//

#include "value_data.h"

#include "compiler_util.h"

bool object_ValueDataListCreate(const ObjectType valueType, ValueData* valueData) {
    linkedList_init(&valueData->valueList);
    valueData->valueType = valueType;
    return false;
}

bool object_ValueDataListAdd(ValueData* valueData, const Object* obj) {
    const ObjectType objType = obj->type == OBJECT_TYPE_IDENT ? obj->symbol->type : obj->type;
    
    if ((valueData->valueType == OBJECT_TYPE_NUM &&
            objType != OBJECT_TYPE_I32 && objType != OBJECT_TYPE_I64 && objType != OBJECT_TYPE_F64) ||
        (valueData->valueType == OBJECT_TYPE_STR &&
            objType != OBJECT_TYPE_STR)
    ) {
        yyerrorf("當前數值之類屬『%s』，與批量宣變之類屬『%s』相左\n",
                 objectType2str[objType], objectType2str[valueData->valueType]);
        return true;
    }

    linkedList_addp(&valueData->valueList, false, cloneStruct(Object, obj));
    return false;
}

Object* object_ValueDataListPop(ValueData* valueData) {
    if (valueData->valueList.length == 0)
        return NULL;
    
    LinkedListNode* firstNode = valueData->valueList.head->next;
    Object* obj = firstNode->value;
    linkedList_deleteNode(&valueData->valueList, firstNode);
    return obj;
}

bool object_ValueDataListFree(ValueData* valueData) {
    linkedList_freeA(&valueData->valueList, free);
    return false;
}
