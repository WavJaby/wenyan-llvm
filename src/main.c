#define WJCL_STRING_IMPLEMENTATION
#define WJCL_HASH_MAP_IMPLEMENTATION
#define WJCL_LINKED_LIST_IMPLEMENTATION
#include "main.h"

#include <stdio.h>
#include <stdlib.h>
#include <utf8.c/utf8.h>
#include <string.h>

#include "compiler_util.h"
#include "lib/byte_buffer.h"

#include "WJCL/string/wjcl_string.h"
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
utf8_init(void) {
}
#endif

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

typedef struct {
    /** Map<@link SymbolData> */
    Map symbolMap;
} ScopeData;

/** LinkedList<@link ScopeData> */
LinkedList scopeList = linkedList_create();

typedef struct {
    int32_t i;
    SymbolData symbol;
} LoopInfo;

/** LinkedList<@link LoopInfo> */
LinkedList loopLabelList = linkedList_create();

const char* symbolPrefix[] = {
    [false] = "var",
    [true] = "exp",
};

int constStrCount = 0;
int loopLabelCount = 0;
int variableCacheCount = 0;

static void printUnknownError() {
    yyerrorf("遭遇不可生之謬誤。\n");
}

void freeObjectData(Object* obj) {
    if (obj == NULL) return;
    switch (obj->type) {
    case OBJECT_TYPE_STR:
        free(obj->str);
        obj->str = NULL;
        break;
    case OBJECT_TYPE_I32:
    case OBJECT_TYPE_I64:
    case OBJECT_TYPE_F64:
    case OBJECT_TYPE_NUM:
        free(obj->number);
        obj->number = NULL;
        break;
    case OBJECT_TYPE_IDENT:
        if (obj->symbol) {
            // SymbolData's name is managed by symbolMap or manually if it's an expression cache
            if (obj->symbol->expCache) {
                free(obj->symbol->name);
            }
            free(obj->symbol);
            obj->symbol = NULL;
        }
        break;
    default:
        break;
    }
}

void pushScope() {
    printf("> (scope level %d)\n", ++scopeLevel);

    const ScopeData scopeData = (ScopeData){
        .symbolMap = (Map)map_createFromInfo(symbolMapInfo)
    };
    linkedList_addp(&scopeList, true, cloneStruct(ScopeData, &scopeData));
}

void dumpScope() {
    printf("< (scope level: %d)\n", scopeLevel);

    ScopeData* scopeData = scopeList.head->prev->value;

    map_free(&scopeData->symbolMap);

    linkedList_deleteNode(&scopeList, scopeList.head->prev);
    --scopeLevel;
}

Object object_createStr(char* str) {
    printf("STRING \"%s\"\n", str);
    return (Object){OBJECT_TYPE_STR, .str = str, .number = NULL, .symbol = NULL};
}

Object object_createNumber(const ScientificNotation* number) {
    if (number->type == ERROR) {
        yyerrorf("數值莫能辨析\n");
        return (Object){OBJECT_TYPE_UNDEFINED, .str = NULL, .number = NULL, .symbol = NULL};
    }

    char* str = sciToStr(number);
    printf("NUMBER %s\n", str);
    free(str);

    return (Object){
        numberType2objectType[number->type], .str = NULL, .number = cloneStruct(ScientificNotation, number),
        .symbol = NULL
    };
}

Object object_findIdentByName(char* name) {
    linkedList_foreach(&scopeList, node) {
        ScopeData* scopeData = node->value;
        const SymbolData* symbol = map_get(&scopeData->symbolMap, name);
        if (symbol)
            return (Object){OBJECT_TYPE_IDENT, .str = NULL, .number = NULL, .symbol = cloneStruct(SymbolData, symbol)};
    }
    yyerrorf("「%s」未宣，無由識之\n", name);
    return (Object){OBJECT_TYPE_UNDEFINED, .str = NULL, .number = NULL, .symbol = NULL};
}

bool object_VariableDefineCountCheck(const ScientificNotation* count) {
    switch (count->type) {
    case I32:
        return false;

    case I64:
    case F64:
        yyerrorf("批量宣變量，指定之量必為全數\n");
        return true;

    case ERROR:
        yyerrorf("數值莫能辨析\n");
        return true;
    }
    return true;
}


bool object_VariableDefineCheck(Object* count) {
    return false;
}

bool code_stdoutPrint(ValueData* valueData, bool newLine) {
    ScopeData* currentScope = scopeList.head->prev->value;
    Object* object = object_ValueDataListPop(valueData);

    printf("PRINT: %p\n", object);

    if (object->type == OBJECT_TYPE_IDENT) {
        // Print variable
        const SymbolData* symbol = object->symbol;
        printf("GET IDENT: %s\n", symbol->name);

        // Load variable value
        switch (symbol->type) {
        case OBJECT_TYPE_I64:
        case OBJECT_TYPE_I32:
        case OBJECT_TYPE_F64:
            const char* typeName = objectType2llvmType[symbol->type];

            buffPrintln(&mainFunBuff, "%%val.%d = load %s, %s* %%%s.%d",
                        variableCacheCount, typeName, typeName, symbolPrefix[symbol->expCache], symbol->index);
            buffPrintln(&mainFunBuff, "call i32 @printf(ptr @fmt_%s%s, %s %%val.%d)",
                        typeName, newLine? "_n": "", typeName, variableCacheCount);
            variableCacheCount++;
            freeObjectData(object);
            return false;
        default:
            break;
        }
    } else if (object->type == OBJECT_TYPE_STR) {
        // Print immediate string
        const size_t constStrLen = strlen(object->str) + newLine;
        byteBufferWriteFormat(&constBuff,
                              "@str.%d = private unnamed_addr constant [%llu x i8] c\"",
                              constStrCount, constStrLen);

        byteBufferWriteStrUtf8(&constBuff, object->str);
        if (newLine) byteBufferWriteStrUtf8(&constBuff, "\n");

        byteBufferWriteStr(&constBuff, "\"\n");

        buffPrintln(&mainFunBuff, "call i64 @fwrite(ptr @str.%d, i64 1, i64 %llu, ptr %%stdout)",
                    constStrCount, constStrLen);

        constStrCount++;
        freeObjectData(object);
        return false;
    }

    // Error
    freeObjectData(object);
    yyerrorf("無法印出數值，未支援的類型：%s\n", objectType2str[object->type]);
    return true;
}

bool code_createVariable(ValueData* valueData, char* name) {
    ScopeData* currentScope = scopeList.head->prev->value;
    Map* currentSymbolMap = &currentScope->symbolMap;
    Object* object = object_ValueDataListPop(valueData);

    printf("Create variable '%s' with type %d\n", name, object->type);

    SymbolData* symbol;
    switch (object->type) {
    case OBJECT_TYPE_I32:
    case OBJECT_TYPE_I64:
    case OBJECT_TYPE_F64:
        // Create symbol
        symbol = malloc(sizeof(SymbolData));
        *symbol = (SymbolData){.type = object->type, .name = strdup(name), .index = (int32_t)currentSymbolMap->size};
        map_putpp(currentSymbolMap, strdup(name), symbol);

        char* valueStr = sciToStr(object->number);

        const char* typeName = objectType2llvmType[object->type];
        buffPrintln(&mainFunBuff, "%var.%d = alloca %s", symbol->index, typeName);
        buffPrintln(&mainFunBuff, "store %s %s, %s* %var.%d", typeName, valueStr, typeName, symbol->index);
        free(valueStr);

        free(name);
        freeObjectData(object);
        return false;
    default:
        free(name);
        freeObjectData(object);
        yyerrorf("無法創建變數，未支援的變數類型：%s\n", objectType2str[object->type]);
        return true;
    }
}

bool code_assign(Object* dest, Object* src) {
    if (dest->type != OBJECT_TYPE_IDENT || src->type == OBJECT_TYPE_UNDEFINED) {
        printUnknownError();
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
        *out = (Object){
            .type = OBJECT_TYPE_IDENT, .str = NULL, .number = NULL, .symbol = cloneStruct(SymbolData, src->symbol)
        };
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
        *out = (Object){.type = OBJECT_TYPE_IDENT, .str = NULL, .number = NULL, .symbol = symbol};
        char* name = strdup("exp");
        *symbol = (SymbolData){.type = src->type, .name = name, .index = variableCacheCount, .expCache = true};

        char* valueStr = sciToStr(src->number);
        buffPrintln(&mainFunBuff, "%%exp.%d = alloca %s", symbol->index, typeName);
        buffPrintln(&mainFunBuff, "store %s %s, %s* %%exp.%d", typeName, valueStr, typeName, symbol->index);
        free(valueStr);

        ++variableCacheCount;
        return;
    default:
        *out = (Object){.type = OBJECT_TYPE_UNDEFINED, .str = NULL, .number = NULL, .symbol = NULL};
    }
}

Object code_expression(char op, bool op_left, Object* a, Object* b) {
    const ObjectType aType = getObjectType(a), bType = getObjectType(b);

    if (aType != bType) {
        yyerrorf("左類『%s』，與右類『%s』相左\n", objectType2str[aType], objectType2str[bType]);
        goto FAILED;
    }

    Object aCache, bCache;
    createExpressionObjectCache(a, &aCache);
    createExpressionObjectCache(b, &bCache);
    if (aCache.type == OBJECT_TYPE_UNDEFINED || bCache.type == OBJECT_TYPE_UNDEFINED)
        goto FAILED;


    Object result;

    switch (op) {
    case '+':
        if (aType == OBJECT_TYPE_I32 || aType == OBJECT_TYPE_I64 || aType == OBJECT_TYPE_F64) {
            const ObjectType type = aType;
            const char* typeName = objectType2llvmType[type];
            const SymbolData *aSymbol = op_left ? bCache.symbol : aCache.symbol,
                             *bSymbol = op_left ? aCache.symbol : bCache.symbol;

            result = (Object){.type = OBJECT_TYPE_IDENT, .symbol = malloc(sizeof(SymbolData))};
            char* name = strdup("exp");
            *result.symbol = (SymbolData){.type = type, .name = name, .index = variableCacheCount, .expCache = true};

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


            buffPrintln(&mainFunBuff, "%%exp.%d = alloca %s", result.symbol->index, typeName);
            buffPrintln(&mainFunBuff, "store %s %%exp.%d.val, %s* %%exp.%d",
                        typeName, result.symbol->index, typeName, result.symbol->index);

            freeObjectData(&aCache);
            freeObjectData(&bCache);

            ++variableCacheCount;
        } else
            goto FAILED;
        break;
    default:
        yyerrorf("Unsupported operation type for code_createVariable");
        goto FAILED;
    }

    freeObjectData(a);
    freeObjectData(b);
    return result;

FAILED:
    freeObjectData(a);
    freeObjectData(b);
    return (Object){.type = OBJECT_TYPE_UNDEFINED, .str = NULL, .number = NULL, .symbol = NULL};
}

bool code_forLoop(Object* obj) {
    printf("> (for loop)\n");

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
    const LoopInfo* loop = loopLabelList.head->prev->value;
    const char* llvmType = objectType2llvmType[loop->symbol.type];

    buffPrintln(&mainFunBuff, "    br label %%loop%d.update", loop->i);

    buffPrintln(&mainFunBuff, "loop%d.update:", loop->i);
    buffPrintln(&mainFunBuff, "    %%loop%d.i.next = add nsw %s %%loop%d.i, 1", loop->i, llvmType, loop->i);
    buffPrintln(&mainFunBuff, "    br label %%loop%d.header", loop->i);

    buffPrintln(&mainFunBuff, "loop%d.exit:", loop->i);
    buffPrintln(&mainFunBuff, "");

    linkedList_deleteNode(&loopLabelList, loopLabelList.head->prev);
    freeObjectData(obj);
    printf("< (for loop end)\n");
    return false;
}

void freeAll() {
    // Free all scope
    linkedList_foreach(&scopeList, node) {
        const ScopeData* scopeData = node->value;
        map_free(&scopeData->symbolMap);
    }
    linkedList_free(&scopeList);


    byteBufferFree(&methodBuff, false);
    byteBufferFree(&constBuff, false);
    byteBufferFree(&mainFunBuff, false);
    yylex_destroy();
}

int main(int argc, char* argv[]) {
    utf8_init();
    linkedList_init(&scopeList);
    linkedList_init(&loopLabelList);

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

    codeRaw("");
#ifdef WIN32
    // Enable windows cmd utf8 output
    codeRaw("declare dllimport i32 @_setmode(i32, i32)");
    codeRaw("declare dllimport i32 @SetConsoleCP(i32)");
    codeRaw("declare dllimport i32 @SetConsoleOutputCP(i32)");
    codeRaw("define dso_local void @utf8_init() {\n"
        "%%1 = call i32 @_setmode(i32 0, i32 32768)\n"
        "%%2 = call i32 @_setmode(i32 1, i32 32768)\n"
        "%%3 = call i32 @SetConsoleCP(i32 65001)\n"
        "%%4 = call i32 @SetConsoleOutputCP(i32 65001)\n"
        "ret void\n"
        "}");

    // Get stdout
    codeRaw("@stdout = global ptr null");
    codeRaw("declare ptr @__acrt_iob_func(i32)");
#else
    codeRaw("@stdout = external global ptr");
#endif

    codeRaw("");
    codeRaw("declare i32 @printf(i8*, ...)");
    codeRaw("declare i32 @_write(i32, ptr, i32)");
    codeRaw("declare i64 @fwrite(ptr, i64, i64, ptr)");
    codeRaw("");
    codeRaw("@fmt_i32_n = private unnamed_addr constant [4 x i8] c\"%%d\\0A\\00\"");
    codeRaw("@fmt_i32 = private unnamed_addr constant [3 x i8] c\"%%d\\00\"");
    codeRaw("@fmt_i64_n = private unnamed_addr constant [6 x i8] c\"%%lld\\0A\\00\"");
    codeRaw("@fmt_i64 = private unnamed_addr constant [5 x i8] c\"%%lld\\00\"");
    // codeRaw("@fmt_float_n = private unnamed_addr constant [4 x i8] c\"%%f\\0A\\00\"");
    // codeRaw("@fmt_float = private unnamed_addr constant [3 x i8] c\"%%f\\00\"");
    codeRaw("@fmt_double_n = private unnamed_addr constant [6 x i8] c\"%%llf\\0A\\00\"");
    codeRaw("@fmt_double = private unnamed_addr constant [5 x i8] c\"%%llf\\00\"");

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
#ifdef WIN32
    // Enable windows cmd utf8 output
    codeRaw("call void @utf8_init()");
    // Get stdout
    codeRaw("%%_stdout = call ptr @__acrt_iob_func(i32 1)");
    codeRaw("store ptr %%_stdout, ptr @stdout");
#endif
    codeRaw("%%stdout = load ptr, ptr @stdout");

    byteBufferWriteToFile(&mainFunBuff, yyout);
    codeRaw("    ret i32 0");
    codeRaw("}");

    printf("\nTotal lines: %d\n", yylineno);
    fclose(yyin);

    freeAll();
    return 0;
}
