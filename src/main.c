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

ByteBuffer methodByteBuff = byteBufferInit();
ByteBuffer constantByteBuff = byteBufferInit();
ByteBuffer mainFunByteBuff = byteBufferInit();
char *inputFilePath, *inputFileName;
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

int constStrCount = 0;
int loopLabelCount = 0;

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
        const SymbolData* identObj = obj->symbol;
        printf("GET IDENT: %s\n", identObj->name);

        const char* typeFormat;
        // Load variable value
        switch (identObj->type) {
        case OBJECT_TYPE_I64:
        case OBJECT_TYPE_I32:
        case OBJECT_TYPE_F64:
            typeFormat = objectType2strFormat[identObj->type];
            byteBufferWriteFormat(&constantByteBuff,
                                  "@.str.%d = private unnamed_addr constant [%d x i8] c\"%s%s\"\n",
                                  constStrCount, 2 + newLine + 1, typeFormat, newLine ? "\\0A\\00" : "\\00");

            const char* typeName = objectType2llvmType[identObj->type];

            byteBufferWriteFormat(&mainFunByteBuff, SCOPE_SPACE_FMT "%val.%d = load %s, %s* %var.%d\n",
                                  SCOPE_SPACE_VAL, identObj->index, typeName, typeName, identObj->index);
            byteBufferWriteFormat(&mainFunByteBuff, SCOPE_SPACE_FMT "call i32 @printf(ptr @.str.%d, %s %val.%d)\n",
                                  SCOPE_SPACE_VAL, constStrCount, typeName, identObj->index);
            constStrCount++;
            break;
        default:
            freeObjectData(obj);
            return true;
        }
    } else if (obj->type == OBJECT_TYPE_STR) {
        // Print immediate string
        const size_t constStrLen = strlen(obj->str);
        byteBufferWriteFormat(&constantByteBuff,
                              "@.str.%d = private unnamed_addr constant [%llu x i8] c\"",
                              constStrCount, constStrLen + newLine + 1);

        byteBufferWriteStrUtf8(&constantByteBuff, obj->str);
        if (newLine) byteBufferWriteStrUtf8(&constantByteBuff, "\n");

        byteBufferWriteStr(&constantByteBuff, "\\00\"\n");

        byteBufferWriteFormat(&mainFunByteBuff, SCOPE_SPACE_FMT "call i32 @printf(ptr @.str.%d)\n",
                              SCOPE_SPACE_VAL, constStrCount);

        constStrCount++;
        freeObjectData(obj);
        return false;
    }

    // Error
    freeObjectData(obj);
    return true;
}

bool code_createVariable(Object* obj, char* name) {
    Map* currentSymbolMap = scopeList.last->value;

    SymbolData* symbol;
    switch (obj->type) {
    case OBJECT_TYPE_I32:
    case OBJECT_TYPE_I64:
    case OBJECT_TYPE_F64:
        // Create symbol
        symbol = malloc(sizeof(SymbolData));
        *symbol = (SymbolData){.type = obj->type, .name = strdup(name), .index = (int32_t)currentSymbolMap->size};
        map_putpp(currentSymbolMap, strdup(name), symbol);

        char* valueStr = sciToStr(obj->number);

        const char* typeName = objectType2llvmType[obj->type];
        byteBufferWriteFormat(&mainFunByteBuff, SCOPE_SPACE_FMT "%var.%d = alloca %s\n",
                              SCOPE_SPACE_VAL, symbol->index, typeName);
        byteBufferWriteFormat(&mainFunByteBuff, SCOPE_SPACE_FMT "store %s %s, %s* %var.%d\n",
                              SCOPE_SPACE_VAL, typeName, valueStr, typeName, symbol->index);
        free(valueStr);

        free(name);
        freeObjectData(obj);
        return false;
    default:
        free(name);
        freeObjectData(obj);
        return true;
    }
}

bool code_forLoop(Object* obj) {
    LoopInfo* loop = malloc(sizeof(LoopInfo));
    loop->i = loopLabelCount++;
    linkedList_addp(&loopLabelList, true, loop);

    // Create loop count %loop%d.i
    byteBufferWriteFormat(&mainFunByteBuff, "\n" SCOPE_SPACE_FMT "%%loop%d.i = alloca i32\n",
                          SCOPE_SPACE_VAL, loop->i);

    // Copy loop count to %loop%d.i
    const SymbolData* symbol;
    switch (obj->type) {
    case OBJECT_TYPE_I32:
    case OBJECT_TYPE_I64:
    case OBJECT_TYPE_F64:
        char* num = sciToStr(obj->number);
        byteBufferWriteFormat(&mainFunByteBuff, SCOPE_SPACE_FMT "store i32 %s, ptr %%loop%d.i\n",
                              SCOPE_SPACE_VAL, num, loop->i);
        free(num);
        break;
    case OBJECT_TYPE_IDENT:
        symbol = obj->symbol;

        byteBufferWriteFormat(&mainFunByteBuff, SCOPE_SPACE_FMT "%%loop%d.i.init = load i32, ptr %var.%d\n",
                              SCOPE_SPACE_VAL, loop->i, symbol->index);
        byteBufferWriteFormat(&mainFunByteBuff, SCOPE_SPACE_FMT "store i32 %%loop%d.i.init, ptr %%loop%d.i\n",
                              SCOPE_SPACE_VAL, loop->i, loop->i);
        break;
    default:
        return true;
    }
    // Check condition
    byteBufferWriteFormat(&mainFunByteBuff, SCOPE_SPACE_FMT "br label %%loop%d.header\n",
                          SCOPE_SPACE_VAL, loop->i);

    byteBufferWriteFormat(&mainFunByteBuff, "\n" SCOPE_SPACE_FMT "loop%d.header:\n",
                          SCOPE_SPACE_VAL, loop->i);
    byteBufferWriteFormat(&mainFunByteBuff, SCOPE_SPACE_FMT "    %%loop%d.i.val = load i32, ptr %%loop%d.i\n",
                          SCOPE_SPACE_VAL, loop->i, loop->i);
    byteBufferWriteFormat(&mainFunByteBuff, SCOPE_SPACE_FMT "    %%loop%d.cond = icmp sgt i32 %%loop%d.i.val, 0\n",
                          SCOPE_SPACE_VAL, loop->i, loop->i);
    byteBufferWriteFormat(&mainFunByteBuff,
                          SCOPE_SPACE_FMT "    br i1 %%loop%d.cond, label %%loop%d.body, label %%loop%d.exit\n\n",
                          SCOPE_SPACE_VAL, loop->i, loop->i, loop->i);

    byteBufferWriteFormat(&mainFunByteBuff, SCOPE_SPACE_FMT "loop%d.body:\n",
                          SCOPE_SPACE_VAL, loop->i);
    return false;
}


bool code_forLoopEnd(Object* obj) {
    const LoopInfo* loop = loopLabelList.last->value;

    // Decrease loop count
    byteBufferWriteFormat(&mainFunByteBuff, "\n" SCOPE_SPACE_FMT ";loop%d.update:\n",
                          SCOPE_SPACE_VAL, loop->i);
    byteBufferWriteFormat(&mainFunByteBuff, SCOPE_SPACE_FMT "    %%loop%d.i.new = sub i32 %%loop%d.i.val, 1\n",
                          SCOPE_SPACE_VAL, loop->i, loop->i);
    byteBufferWriteFormat(&mainFunByteBuff, SCOPE_SPACE_FMT "    store i32 %%loop%d.i.new, ptr %%loop%d.i\n",
                          SCOPE_SPACE_VAL, loop->i, loop->i);
    byteBufferWriteFormat(&mainFunByteBuff, SCOPE_SPACE_FMT "    br label %%loop%d.header\n",
                          SCOPE_SPACE_VAL, loop->i);


    byteBufferWriteFormat(&mainFunByteBuff, SCOPE_SPACE_FMT "loop%d.exit:\n",
                          SCOPE_SPACE_VAL, loop->i);

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

    byteBufferFree(&methodByteBuff, false);
    byteBufferFree(&constantByteBuff, false);
    byteBufferFree(&mainFunByteBuff, false);
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
    } else {
        printf("require input file");
        return 1;
    }
    if (!yyin) {
        printf("file `%s` doesn't exists or cannot be opened\n", inputFilePath);
        return 1;
    }
    if (!yyout) {
        printf("file `%s` doesn't exists or cannot be opened\n", outputFilePath);
        return 1;
    }

    // Extract file name
    inputFileName = strrchr(inputFilePath, '/');
    if (inputFileName == NULL) {
        inputFileName = strrchr(inputFilePath, '\\');
    }
    inputFileName = inputFileName == NULL ? inputFilePath : inputFileName + 1;


    // Start parsing
    // Object endl = {OBJECT_TYPE_STR, false, 0, 0, &(SymbolData){"endl", 0, -1}};
    // map_putpp(&staticVar, "endl", &endl);

    code("; ModuleID = '%s'", inputFileName);
    code("source_filename = \"%s\"", inputFileName);
    codeRaw("");

    // Start parsing
    yylineno = 1;
    yyparse();

    if (compileError) {
        fclose(yyin);
        freeAll();
        return 2;
    }

    byteBufferWriteToFile(&constantByteBuff, yyout);
    codeRaw("");
    codeRaw("define i32 @main() {");
    codeRaw("call void @utf8_init()");
    byteBufferWriteToFile(&mainFunByteBuff, yyout);
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
