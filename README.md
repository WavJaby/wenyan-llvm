<img src="https://raw.githubusercontent.com/wenyan-lang/wenyan/master/screenshots/logo.png" align="right" width="128" height="128"/>

# 文言 wenyan-lang | LLVM Compiler

A compiler for [wenyan-lang](https://wy-lang.org/) that generates LLVM IR code.<br>
This project allows wenyan-lang to be compiled to modern platforms.

## Overview

Wenyan-LLVM compile wenyan-lang code into [LLVM IR](https://llvm.org/docs/LangRef.html).<br>
This allows Wenyan code to leverage LLVM’s optimization capabilities and target multiple architectures.<br>

This compiler uses Flex for lexical analysis and Bison (compatible with Yacc) for parsing, 
provides easy maintenance and contributions for developers.<br>

This project follows the grammar of wenyan-lang, 
see [wenyan-lang](https://github.com/wenyan-lang/wenyan) 
and [wenyan-book](https://github.com/wenyan-lang/book)

## Features

- **More intuitive error messages**
- **More standardized syntax**
- **Chinese Numeral System**
- **LLVM Backend**

## Example

A simple Wenyan program that prints a message twice and then counts from 1 to 10:

```wenyan
為是 二 遍。
    吾有一言。曰「「問天地好在。」」。書之。
云云。

吾有一言。曰「「開始數數」」。書之。
有數 零 。名之曰「甲」。

為是 十 遍。
    加一以「甲」。昔之「甲」者。今其是矣。

    吾有一言。曰「甲」。書之。
云云。

吾有一言。曰「「數完了」」。書之。
```

This compiles to LLVM IR with string constants and loops properly implemented.

## Building

This project requires:

- CMake
- Flex and Bison (3.0 or higher)
- LLVM development libraries (llvm clang)

```bash
# Clone the repository with submodules
git clone --recursive https://github.com/WavJaby/wenyan-llvm.git

# Install requirements
apt install flex bison

# Create build directory
mkdir build && cd build

# Configure and build
cmake ..
make
```

## Usage

```bash
# Compile a Wenyan source file to LLVM IR
./main input.wy output.ll

# Install llvm requirements
sudo apt install llvm clang

# If you want to directly execute LLVM IR code
# lli output.ll

# Compile llvm
llc -filetype=obj -relocation-model=pic output.ll
clang -fPIE output.o -o program

# Run final program
./program
```

## License

[MIT License](LICENSE)
