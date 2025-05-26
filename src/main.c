#include "main.h"

#include <stdio.h>
#include <stdlib.h>
#include <utf8.c/utf8.h>
#include <string.h>

#include "compiler_util.h"
#include "lib/byte_buffer.h"

#define WJCL_STRING_IMPLEMENTATION
#include "WJCL/string/wjcl_string.h"
#define WJCL_HASH_MAP_IMPLEMENTATION
#define WJCL_LINKED_LIST_IMPLEMENTATION
#include "WJCL/map/wjcl_hash_map.h"

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>

__attribute__((constructor))
void utf8_init(void) {
    _setmode(0, _O_BINARY);
    _setmode(1, _O_BINARY);
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
}
#else
utf8_init(void) {}
#endif

#define cloneStruct(type, ptr) memcpy(malloc(sizeof(type)), ptr, sizeof(type))
#define buffPrintln(buff, format, ...) \
    byteBufferWriteFormat(buff, SCOPE_SPACE_FMT format "\n", SCOPE_SPACE_VAL, ##__VA_ARGS__)

ByteBuffer methodBuff = byteBufferInit();
ByteBuffer constBuff = byteBufferInit();
ByteBuffer mainFunBuff = byteBufferInit();
char *inputFilePath = NULL, *inputFileName = NULL;
bool compileError;
int scopeLevel = 0;

bool symbolKeyEquals(void* key1, void* key2) {
    return strcmp(key1, key2) == 0;
}

uint32_t symbolKeyHash(void* key) {
    return strHash(key);
}

void symbolValueFree(void* key, void* value) {
    const SymbolData* symbol = value;
    free(symbol->name);
}

static const MapNodeInfo symbolMapInfo = {
    symbolKeyEquals, symbolKeyHash, symbolValueFree,
    WJCL_HASH_MAP_FREE_KEY | WJCL_HASH_MAP_FREE_VALUE
};

/** LinkedList<Map<@link SymbolData>> */
LinkedList scopeList = linkedList_create();

typedef struct {
    int32_t i;
    SymbolData symbol;
} LoopInfo;

/** LinkedList<@link LoopInfo> */
LinkedList loopLabelList = linkedList_create();

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
    [OBJECT_TYPE_I32] = "全數",
    [OBJECT_TYPE_I64] = "長數",
    [OBJECT_TYPE_F64] = "浮數",
    [OBJECT_TYPE_STR] = "字串",
    [OBJECT_TYPE_IDENT] = "識名",
    [OBJECT_TYPE_UNDEFINED] = "無定",
};
const char* symbolPrefix[] = {
    [false] = "var",
    [true] = "exp",
};

int constStrCount = 0;
int loopLabelCount = 0;
int variableCacheCount = 0;

void pushScope() {
    printf("> (scope level %d)\n", ++scopeLevel);

    Map* symbolMap = map_new(symbolMapInfo);
    linkedList_addp(&scopeList, 0, symbolMap);
}

void dumpScope() {
    printf("< (scope level: %d)\n", scopeLevel);

    Map* symbolMap = scopeList.last->value;
    map_free(symbolMap);
    free(symbolMap);

    linkedList_deleteNode(&scopeList, scopeList.last);

    --scopeLevel;
}

void freeObjectData(Object* obj) {
    free(obj->str);
    obj->str = NULL;
    free(obj->number);
    obj->number = NULL;
    free(obj->symbol);
    obj->symbol = NULL;
}

Object object_createStr(char* str) {
    printf("STRING \"%s\"\n", str);
    return (Object){OBJECT_TYPE_STR, .str = str};
}

Object object_createNumber(const ScientificNotation* number) {
    if (number->type == ERROR) {
        yyerrorf("數值莫能辨析\n");
        return (Object){OBJECT_TYPE_UNDEFINED};
    }

    char* str = sciToStr(number);
    printf("NUMBER %s\n", str);
    free(str);

    return (Object){numberType2objectType[number->type], .number = cloneStruct(ScientificNotation, number)};
}

Object object_findIdentByName(char* name) {
    for (LinkedListNode* node = scopeList.last; node; node = node->prev) {
        Map* symbolMap = node->value;
        const SymbolData* symbol = map_get(symbolMap, name);
        if (symbol)
            return (Object){OBJECT_TYPE_IDENT, .symbol = cloneStruct(SymbolData, symbol)};
    }
    yyerrorf("「%s」未宣，無由識之\n", name);
    return (Object){OBJECT_TYPE_UNDEFINED};
}

bool code_stdoutPrint(Object* obj, bool newLine) {
    if (obj->type == OBJECT_TYPE_IDENT) {
        // Print variable
        const SymbolData* symbol = obj->symbol;
        printf("GET IDENT: %s\n", symbol->name);

        const char* typeFormat;
        // Load variable value
        switch (symbol->type) {
        case OBJECT_TYPE_I64:
        case OBJECT_TYPE_I32:
        case OBJECT_TYPE_F64:
            typeFormat = objectType2strFormat[symbol->type];
            byteBufferWriteFormat(&constBuff,
                                  "@.str.%d = private unnamed_addr constant [%d x i8] c\"%s%s\"\n",
                                  constStrCount, 2 + newLine + 1, typeFormat, newLine ? "\\0A\\00" : "\\00");

            const char* typeName = objectType2llvmType[symbol->type];

            buffPrintln(&mainFunBuff, "%val.%d = load %s, %s* %%%s.%d",
                        symbol->index, typeName, typeName, symbolPrefix[symbol->expCache], symbol->index);
            buffPrintln(&mainFunBuff, "call i32 @printf(ptr @.str.%d, %s %val.%d)",
                        constStrCount, typeName, symbol->index);
            constStrCount++;
            break;
        default:
            freeObjectData(obj);
            return true;
        }
    } else if (obj->type == OBJECT_TYPE_STR) {
        // Print immediate string
        const size_t constStrLen = strlen(obj->str);
        byteBufferWriteFormat(&constBuff,
                              "@.str.%d = private unnamed_addr constant [%llu x i8] c\"",
                              constStrCount, constStrLen + newLine + 1);

        byteBufferWriteStrUtf8(&constBuff, obj->str);
        if (newLine) byteBufferWriteStrUtf8(&constBuff, "\n");

        byteBufferWriteStr(&constBuff, "\\00\"\n");

        buffPrintln(&mainFunBuff, "call i32 @printf(ptr @.str.%d)", constStrCount);

        constStrCount++;
        freeObjectData(obj);
        return false;
    }

    // Error
    freeObjectData(obj);
    return true;
}

bool code_createVariable(Object* src, char* variable) {
    Map* currentSymbolMap = scopeList.last->value;

    SymbolData* symbol;
    switch (src->type) {
    case OBJECT_TYPE_I32:
    case OBJECT_TYPE_I64:
    case OBJECT_TYPE_F64:
        // Create symbol
        symbol = malloc(sizeof(SymbolData));
        *symbol = (SymbolData){.type = src->type, .name = strdup(variable), .index = (int32_t)currentSymbolMap->size};
        map_putpp(currentSymbolMap, strdup(variable), symbol);

        char* valueStr = sciToStr(src->number);

        const char* typeName = objectType2llvmType[src->type];
        buffPrintln(&mainFunBuff, "%var.%d = alloca %s", symbol->index, typeName);
        buffPrintln(&mainFunBuff, "store %s %s, %s* %var.%d", typeName, valueStr, typeName, symbol->index);
        free(valueStr);

        free(variable);
        freeObjectData(src);
        return false;
    default:
        free(variable);
        freeObjectData(src);
        return true;
    }
}

bool code_assign(Object* dest, Object* src) {
    if (dest->type != OBJECT_TYPE_IDENT || src->type == OBJECT_TYPE_UNDEFINED) {
        yyerrorf("遭遇不可生之謬誤。\n");
        freeObjectData(dest);
        freeObjectData(src);
        return true;
    }

    const char* llvmType;
    bool failed = false;
    switch (src->type) {
    case OBJECT_TYPE_I32:
    case OBJECT_TYPE_I64:
    case OBJECT_TYPE_F64:
        if (dest->symbol->type != src->type) {
            yyerrorf("的之類屬，與源『%s』之類相左\n", dest->symbol->name);
            failed = true;
            break;
        }

        llvmType = objectType2llvmType[src->type];

        char* num = sciToStr(src->number);
        buffPrintln(&mainFunBuff, "store %s %s, ptr %%%d.%d", llvmType, num,
                    symbolPrefix[src->symbol->expCache], dest->symbol->index);
        free(num);
        ++variableCacheCount;
        break;
    case OBJECT_TYPE_IDENT:
        if (dest->symbol->type != src->symbol->type) {
            yyerrorf("源『%s』之類屬，與的『%s』之類相左\n", dest->symbol->name, src->symbol->name);
            failed = true;
            break;
        }

        llvmType = objectType2llvmType[dest->symbol->type];
        buffPrintln(&mainFunBuff, "%%cache.%d = load %s, ptr %%%s.%d",
                    variableCacheCount, llvmType, symbolPrefix[src->symbol->expCache], src->symbol->index);
        buffPrintln(&mainFunBuff, "store %s %%cache.%d, ptr %%%s.%d",
                    llvmType, variableCacheCount, symbolPrefix[dest->symbol->expCache], dest->symbol->index);
        ++variableCacheCount;
        break;
    default:
        failed = true;
        break;
    }

    freeObjectData(dest);
    freeObjectData(src);
    return failed;
}

ObjectType getObjectType(const Object* obj) {
    if (obj->type == OBJECT_TYPE_IDENT)
        return obj->symbol->type;
    return obj->type;
}

void createExpressionObjectCache(Object* src, Object* out) {
    if (src->type == OBJECT_TYPE_IDENT) {
        *out = (Object){.type = OBJECT_TYPE_IDENT, .symbol = cloneStruct(SymbolData, src->symbol)};
        return;
    }

    const char* typeName;
    SymbolData* symbol;
    switch (src->type) {
    case OBJECT_TYPE_I32:
    case OBJECT_TYPE_I64:
    case OBJECT_TYPE_F64:
        typeName = objectType2llvmType[src->type];
        symbol = malloc(sizeof(SymbolData));
        *out = (Object){.type = OBJECT_TYPE_IDENT, .symbol = symbol};
        char* name = strdup("exp");
        *symbol = (SymbolData){.type = src->type, .name = name, .index = variableCacheCount, .expCache = true};

        char* valueStr = sciToStr(src->number);
        buffPrintln(&mainFunBuff, "%%exp.%d = alloca %s", symbol->index, typeName);
        buffPrintln(&mainFunBuff, "store %s %s, %s* %%exp.%d", typeName, valueStr, typeName, symbol->index);
        free(valueStr);

        ++variableCacheCount;
        return;
    default:
        *out = (Object){.type = OBJECT_TYPE_UNDEFINED};
    }
}

Object code_expression(char op, bool op_left, Object* a, Object* b) {
    const ObjectType aType = getObjectType(a), bType = getObjectType(b);

    bool failed = false;
    if (aType != bType) {
        yyerrorf("左類『%s』，與右類『%s』相左\n", objectType2str[aType], objectType2str[bType]);
        failed = true;
    }

    Object aCache, bCache;
    createExpressionObjectCache(a, &aCache);
    createExpressionObjectCache(b, &bCache);
    if (aCache.type == OBJECT_TYPE_UNDEFINED || bCache.type == OBJECT_TYPE_UNDEFINED)
        failed = true;


    Object result;

    switch (op) {
    case '+':
        if (aType == OBJECT_TYPE_I32 || aType == OBJECT_TYPE_I64 || aType == OBJECT_TYPE_F64) {
            const ObjectType type = aType;
            const char* typeName = objectType2llvmType[type];
            const SymbolData *aSymbol = op_left ? bCache.symbol : aCache.symbol,
                             *bSymbol = op_left ? aCache.symbol : bCache.symbol;

            SymbolData* symbol = malloc(sizeof(SymbolData));
            result = (Object){.type = OBJECT_TYPE_IDENT, .symbol = symbol};
            char* name = strdup("exp");
            *symbol = (SymbolData){.type = type, .name = name, .index = variableCacheCount, .expCache = true};

            buffPrintln(&mainFunBuff, "%%%s.%d.val = load %s, ptr %%%s.%d",
                        symbolPrefix[aSymbol->expCache], aSymbol->index, typeName,
                        symbolPrefix[aSymbol->expCache], aSymbol->index);
            buffPrintln(&mainFunBuff, "%%%s.%d.val = load %s, ptr %%%s.%d",
                        symbolPrefix[bSymbol->expCache], bSymbol->index, typeName,
                        symbolPrefix[bSymbol->expCache], bSymbol->index);
            buffPrintln(&mainFunBuff, "%%exp.%d.val = add nsw %s %%%s.%d.val, %%%s.%d.val",
                        variableCacheCount, typeName,
                        symbolPrefix[aSymbol->expCache], aSymbol->index,
                        symbolPrefix[bSymbol->expCache], bSymbol->index);

            
            buffPrintln(&mainFunBuff, "%%exp.%d = alloca %s", symbol->index, typeName);
            buffPrintln(&mainFunBuff, "store %s %%exp.%d.val, %s* %%exp.%d", typeName, symbol->index, typeName, symbol->index);
            
            freeObjectData(&aCache);
            freeObjectData(&bCache);

            ++variableCacheCount;
        } else
            failed = true;
        break;
    default:
        failed = true;
        break;
    }

    freeObjectData(a);
    freeObjectData(b);
    if (failed)
        return (Object){.type = OBJECT_TYPE_UNDEFINED};
    return result;
}

bool code_forLoop(Object* obj) {
    LoopInfo* loop = malloc(sizeof(LoopInfo));
    loop->i = loopLabelCount++;
    linkedList_addp(&loopLabelList, true, loop);


    // Create loop
    buffPrintln(&mainFunBuff, "");
    buffPrintln(&mainFunBuff, "br label %%loop%d.entry", loop->i);
    buffPrintln(&mainFunBuff, "loop%d.entry:", loop->i);


    // Copy loop count to %loop%d.i
    const char* llvmType;
    switch (obj->type) {
    case OBJECT_TYPE_I32:
    case OBJECT_TYPE_I64:
    case OBJECT_TYPE_F64:
        loop->symbol = (SymbolData){.type = obj->type};
        llvmType = objectType2llvmType[loop->symbol.type];

        buffPrintln(&mainFunBuff, "    br label %%loop%d.header", loop->i);
        buffPrintln(&mainFunBuff, "loop%d.header:", loop->i);
        buffPrintln(&mainFunBuff, "    %%loop%d.i = phi %s [0, %%loop%d.entry], [%%loop%d.i.next, %%loop%d.update]",
                    loop->i, llvmType, loop->i, loop->i, loop->i);

        char* num = sciToStr(obj->number);
        buffPrintln(&mainFunBuff, "    %%loop%d.cond = icmp slt %s %%loop%d.i, %s", loop->i, llvmType, loop->i, num);
        free(num);
        break;
    case OBJECT_TYPE_IDENT:
        loop->symbol = *obj->symbol;
        llvmType = objectType2llvmType[loop->symbol.type];

        buffPrintln(&mainFunBuff, "    %%loop%d.i.end = load %s, ptr %var.%d\n",
                    loop->i, llvmType, loop->symbol.index);

        buffPrintln(&mainFunBuff, "    br label %%loop%d.header", loop->i);
        buffPrintln(&mainFunBuff, "loop%d.header:", loop->i);
        buffPrintln(&mainFunBuff,
                    "    %%loop%d.i = phi %s [0, %%loop%d.entry], [%%loop%d.i.next, %%loop%d.update]",
                    loop->i, llvmType, loop->i, loop->i, loop->i);
        buffPrintln(&mainFunBuff, "    %%loop%d.cond = icmp slt %s %%loop%d.i, %%loop%d.i.end",
                    loop->i, llvmType, loop->i, loop->i);

        break;
    default:
        return true;
    }

    buffPrintln(&mainFunBuff, "    br i1 %%loop%d.cond, label %%loop%d.body, label %%loop%d.exit",
                loop->i, loop->i, loop->i);

    buffPrintln(&mainFunBuff, "loop%d.body:", loop->i);
    return false;
}


bool code_forLoopEnd(Object* obj) {
    const LoopInfo* loop = loopLabelList.last->value;
    const char* llvmType = objectType2llvmType[loop->symbol.type];

    buffPrintln(&mainFunBuff, "    br label %%loop%d.update", loop->i);

    buffPrintln(&mainFunBuff, "loop%d.update:", loop->i);
    buffPrintln(&mainFunBuff, "    %%loop%d.i.next = add nsw %s %%loop%d.i, 1", loop->i, llvmType, loop->i);
    buffPrintln(&mainFunBuff, "    br label %%loop%d.header", loop->i);

    buffPrintln(&mainFunBuff, "loop%d.exit:", loop->i);
    buffPrintln(&mainFunBuff, "");

    linkedList_deleteNode(&loopLabelList, loopLabelList.last);
    freeObjectData(obj);
    return false;
}

void freeAll() {
    // Free all scope
    linkedList_foreach(&scopeList, node) {
        Map* symbolMap = linkedList_nodeVal(Map*, node);
        map_free(symbolMap);
        free(symbolMap);
    }
    linkedList_free(&scopeList);

    byteBufferFree(&methodBuff, false);
    byteBufferFree(&constBuff, false);
    byteBufferFree(&mainFunBuff, false);
    yylex_destroy();
}

int main(int argc, char* argv[]) {
    utf8_init();

    char* outputFilePath = NULL;
    if (argc == 3) {
        yyin = fopen(inputFilePath = argv[1], "rb");
        yyout = fopen(outputFilePath = argv[2], "w");
    } else if (argc == 2) {
        yyin = fopen(inputFilePath = argv[1], "rb");
        yyout = stdout;
    } else if (argc == 1) {
        yyin = stdin;
        yyout = stdout;
        printf("===== Use stdin for parsing =====");
    } else {
        fprintf(stderr, "Usage: %s [input file] [output file]\n", argv[0]);
    }
    if (!yyin) {
        fprintf(stderr, "file `%s` doesn't exists or cannot be opened\n", inputFilePath);
        return 1;
    }
    if (!yyout) {
        fprintf(stderr, "file `%s` doesn't exists or cannot be opened\n", outputFilePath);
        return 1;
    }

    // Extract file name
    if (inputFilePath) {
        inputFileName = strrchr(inputFilePath, '/');
        if (inputFileName == NULL) {
            inputFileName = strrchr(inputFilePath, '\\');
        }
        inputFileName = inputFileName == NULL ? inputFilePath : inputFileName + 1;

        code("; ModuleID = '%s'", inputFileName);
        code("source_filename = \"%s\"", inputFileName);
    }

    // Start parsing
    // Object endl = {OBJECT_TYPE_STR, false, 0, 0, &(SymbolData){"endl", 0, -1}};
    // map_putpp(&staticVar, "endl", &endl);
    codeRaw("");

    // Start parsing
    yylineno = 1;
    yyparse();

    if (compileError) {
        fclose(yyin);
        freeAll();
        return 2;
    }

    byteBufferWriteToFile(&constBuff, yyout);
    codeRaw("");
    codeRaw("define i32 @main() {");
    codeRaw("call void @utf8_init()");
    byteBufferWriteToFile(&mainFunBuff, yyout);
    codeRaw("    ret i32 0");
    codeRaw("}");

    codeRaw("");
    codeRaw("define dso_local void @utf8_init() {\n"
        "%%1 = call i32 @_setmode(i32 0, i32 32768)\n"
        "%%2 = call i32 @_setmode(i32 1, i32 32768)\n"
        "%%3 = call i32 @SetConsoleCP(i32 65001)\n"
        "%%4 = call i32 @SetConsoleOutputCP(i32 65001)\n"
        "ret void\n"
        "}");

    codeRaw("");
    codeRaw("declare i32 @printf(i8*, ...)");
    codeRaw("declare dllimport i32 @_setmode(i32, i32)");
    codeRaw("declare dllimport i32 @SetConsoleCP(i32)");
    codeRaw("declare dllimport i32 @SetConsoleOutputCP(i32)");


    printf("\nTotal lines: %d\n", yylineno);
    fclose(yyin);

    freeAll();
    return 0;
}
