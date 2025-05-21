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

ByteBuffer methodByteBuff = byteBufferInit();
ByteBuffer constantByteBuff = byteBufferInit();
ByteBuffer mainFunByteBuff = byteBufferInit();
char *inputFilePath, *inputFileName;
bool compileError;
int scopeLevel = 0;

bool mainVariableKeyEquals(void* key1, void* key2) {
    return strcmp(key1, key2) == 0;
}

uint32_t mainVariableKeyHash(void* key) {
    return strHash(key);
}

void mainVariableValueFree(void* key, void* value) {
    const SymbolData* symbol = value;
    free(symbol->name);
}

/** {@link SymbolData} */
Map mainVariable = map_create(mainVariableKeyEquals, mainVariableKeyHash, mainVariableValueFree,
                              WJCL_HASH_MAP_FREE_KEY | WJCL_HASH_MAP_FREE_VALUE);
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

void pushScope() {
    printf("> (scope level %d)\n", ++scopeLevel);
}

void dumpScope() {
    printf("> (scope level: %d)\n", scopeLevel);
}

Object createNumberObject(const ScientificNotation* number) {
    ScientificNotation* numberClone = malloc(sizeof(ScientificNotation));
    *numberClone = *number;

    char* str = sciToStr(number);
    printf("NUMBER %s\n", str);
    free(str);

    return (Object){numberType2objectType[number->type], .number = numberClone};
}

Object findIdentByName(char* name) {
    SymbolData* symbol = map_get(&mainVariable, name);
    if (!symbol)
        return (Object){OBJECT_TYPE_UNDEFINED};
    return (Object){OBJECT_TYPE_IDENT, .symbol = symbol};
}

bool code_stdoutPrint(Object* obj) {
    bool newLine = true;

    if (obj->type == OBJECT_TYPE_IDENT) {
        // Print variable
        SymbolData* identObj = obj->symbol;
        printf("GET IDENT: %s\n", identObj->name);

        // Load variable value
        switch (identObj->type) {
        case OBJECT_TYPE_I64:
        case OBJECT_TYPE_I32:
        case OBJECT_TYPE_F64:
            const char* typeFormat = objectType2strFormat[identObj->type];
            byteBufferWriteFormat(&constantByteBuff,
                                  "@.str.%d = private unnamed_addr constant [%d x i8] c\"%s%s\"\n",
                                  constStrCount, 2 + newLine + 1, typeFormat, newLine ? "\\0A\\00" : "\\00");

            const char* typeName = objectType2llvmType[identObj->type];

            byteBufferWriteFormat(&mainFunByteBuff, "  %val.%d = load %s, %s* %var.%d\n",
                                  identObj->index, typeName, typeName, identObj->index);
            byteBufferWriteFormat(&mainFunByteBuff, "  call i32 @printf(ptr @.str.%d, %s %val.%d)\n",
                                  constStrCount, typeName, identObj->index);
            constStrCount++;
            break;
        default:
            return true;
        }
    }
    else if (obj->type == OBJECT_TYPE_STR) {
        char* str = obj->str;
        // Print immediate string
        const size_t constStrLen = strlen(str);
        byteBufferWriteFormat(&constantByteBuff,
                              "@.str.%d = private unnamed_addr constant [%llu x i8] c\"",
                              constStrCount, constStrLen + newLine + 1);

        byteBufferWriteStrUtf8(&constantByteBuff, str);
        if (newLine) byteBufferWriteStrUtf8(&constantByteBuff, "\n");

        byteBufferWriteStr(&constantByteBuff, "\\00\"\n");

        byteBufferWriteFormat(&mainFunByteBuff, "  call i32 @printf(ptr @.str.%d)\n", constStrCount);

        constStrCount++;
        free(str);
    }
    return false;
}

bool code_createVariable(Object* obj, char* name) {
    switch (obj->type) {
    case OBJECT_TYPE_I32:
    case OBJECT_TYPE_I64:
    case OBJECT_TYPE_F64:
        SymbolData* symbol = malloc(sizeof(SymbolData));
        symbol->type = obj->type;
        symbol->name = strdup(name);
        symbol->index = (int32_t)mainVariable.size;
        map_putpp(&mainVariable, strdup(name), symbol);

        char* valueStr = sciToStr(obj->number);

        const char* typeName = objectType2llvmType[obj->type];
        byteBufferWriteFormat(&mainFunByteBuff, "  %var.%d = alloca %s\n  store %s %s, %s* %var.%d\n",
                              symbol->index, typeName, typeName, valueStr, typeName, symbol->index);

        free(valueStr);
        return false;
    default:
        return true;
    }
}

int main(int argc, char* argv[]) {
    utf8_init();

    char* outputFilePath = NULL;
    if (argc == 3) {
        yyin = fopen(inputFilePath = argv[1], "rb");
        yyout = fopen(outputFilePath = argv[2], "w");
    }
    else if (argc == 2) {
        yyin = fopen(inputFilePath = argv[1], "rb");
        yyout = stdout;
    }
    else {
        printf("require input file");
        exit(1);
    }
    if (!yyin) {
        printf("file `%s` doesn't exists or cannot be opened\n", inputFilePath);
        exit(1);
    }
    if (!yyout) {
        printf("file `%s` doesn't exists or cannot be opened\n", outputFilePath);
        exit(1);
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

    scopeLevel = -1;
    yylineno = 1;
    yyparse();

    byteBufferWriteToFile(&constantByteBuff, yyout);
    codeRaw("");
    codeRaw("define i32 @main() {");
    codeRaw("call void @utf8_init()");
    byteBufferWriteToFile(&mainFunByteBuff, yyout);
    codeRaw("ret i32 0");
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

    yylex_destroy();
    return 0;
}
