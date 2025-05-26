#ifndef MAIN_H
#define MAIN_H
#include <stdbool.h>
#include <stdio.h>
#include "compiler_common.h"

void pushScope();
void dumpScope();

Object object_createStr(char* str);
Object object_createNumber(const ScientificNotation* number);
Object object_findIdentByName(char* name);

bool code_stdoutPrint(Object* obj, bool newLine);
bool code_createVariable(Object* src, char* variable);
bool code_assign(Object* dest, Object* src);
Object code_expression(char op, bool op_left, Object* a, Object* b);

bool code_forLoop(Object* obj);
bool code_forLoopEnd(Object* obj);

/*
extern ObjectType variableIdentType;

#define VAR_FLAG_IN_STACK 0b00000001

Object* findVariable(char* variableName);
bool initVariable(ObjectType variableType, LinkedList* arraySubscripts, char* variableName);
Object* createVariable(ObjectType variableType, LinkedList* arraySubscripts, char* variableName, Object* value);

void functionLocalsBegin();
void functionParmPush(ObjectType variableType, LinkedList* arraySubscripts, char* variableName);
void functionBegin(ObjectType returnType, LinkedList* arraySubscripts, char* funcName);
bool functionEnd(ObjectType returnType);

bool returnObject(Object* obj);
bool breakLoop();

void functionArgsBegin();
void functionArgPush(Object* obj);
void functionCall(char* funcName, Object* out);

// Expressions
bool objectExpression(char op, Object* a, Object* b, Object* out);
bool objectExpBinary(char op, Object* a, Object* b, Object* out);
bool objectExpBoolean(char op, Object* a, Object* b, Object* out);
bool objectNotBinaryExpression(Object* a, Object* out);
bool objectNotBooleanExpression(Object* a, Object* out);
bool objectNegExpression(Object* a, Object* out);
bool objectIncAssign(Object* a, Object* out);
bool objectDecAssign(Object* a, Object* out);
bool objectCast(ObjectType variableType, Object* a, Object* out);
bool objectExpAssign(char op, Object* dest, Object* val, Object* out);
bool objectValueAssign(Object* dest, Object* val, Object* out);

bool ifBegin(Object* a);
bool ifEnd();
bool ifOnlyEnd();
bool elseBegin();
bool elseEnd();

bool whileBegin();
bool whileBodyBegin();
bool whileEnd();

bool forBegin();
bool forInitEnd();
bool forConditionEnd(Object* result);
bool forHeaderEnd();
bool foreachHeaderEnd(Object* obj);
bool forEnd();

bool arrayCreate(Object* out);
bool objectArrayGet(Object* arr, LinkedList* arraySubscripts, Object* out);
LinkedList* arraySubscriptBegin(Object* index);
bool arraySubscriptPush(LinkedList* arraySubscripts, Object* index);
bool arraySubscriptEnd(LinkedList* arraySubscripts);
*/

#endif