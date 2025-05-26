cmake_minimum_required(VERSION 3.10)
project(wenyan_llvm C)

find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)

# --- Define Source Files ---
set(TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/test)
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib)
set(LEX_SRC ${SRC_DIR}/compiler.l)
set(YAC_SRC ${SRC_DIR}/compiler.y)

# --- Define Output Locations for Generated Files ---
# Good practice to put generated files in the build directory
set(GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_DIR}) # Ensure the directory exists

# Set C flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -O0 -g")

# --- Bison Target ---
# Generates parser source and header
BISON_TARGET(
        CompilerParser
        ${YAC_SRC}
        ${GENERATED_DIR}/y.tab.c
        DEFINES_FILE ${GENERATED_DIR}/y.tab.h
        COMPILE_FLAGS "-v"
)

# --- Flex Target ---
# Generates scanner source
FLEX_TARGET(
        CompilerScanner
        ${LEX_SRC}
        ${GENERATED_DIR}/lex.yy.c
)

add_flex_bison_dependency(CompilerScanner CompilerParser)

# --- Include Generated Files Directory ---
include_directories(
        ${GENERATED_DIR}
        ${SRC_DIR}
        ${LIB_DIR}
)

# --- Define Executable ---
add_executable(
        main
        ${SRC_DIR}/main.c
        ${SRC_DIR}/lib/byte_buffer.c
        ${SRC_DIR}/lib/chinese_number.c
        ${SRC_DIR}/compiler_util.c
        ${LIB_DIR}/utf8.c/utf8.c
        ${BISON_CompilerParser_OUTPUTS} # generated parser .c file
        ${FLEX_CompilerScanner_OUTPUTS} # generated scanner .c file
)
target_link_libraries(main m)

# --- Define Test Executable ---
if (EXISTS ${TEST_DIR})
    add_executable(
            test
            ${TEST_DIR}/test.c
            ${SRC_DIR}/lib/chinese_number.c
    )
endif ()
