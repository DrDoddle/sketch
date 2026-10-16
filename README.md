# Sketch

**Sketch** is a compiler for **Doodle**, an experimental statically typed language focused on readable code, grouped function overloads, and named arguments.

Written in C++, with a handwritten lexer, a recursive-descent parser, and LLVM IR generation.

## A little Doodle

```doodle
fn add {
    >(a: int, b: int): int {
        a + b
    };
}

fn main {
    >(): int {
        add(a: 2, b: 3)
    };
}
```

Function variants live together inside a named `fn` group. Arguments are named, and the final expression supplies the return value.

## Current features

- Integer literals and `int` parameters and return values (`i32` in LLVM).
- Addition and parenthesized expressions.
- Grouped function variants and named function calls.
- Name resolution, argument matching, and basic type checking.
- LLVM IR generation, verification, and `.ll` file output.

Sketch is an early prototype. The language and syntax are still evolving.

## Building

Requires CMake 3.24+, a C++20 compiler, and LLVM development headers and libraries. Ninja is used in these examples.

```sh
cmake -S . -B build -G Ninja -DLLVM_DIR="/path/to/llvm/lib/cmake/llvm"
cmake --build build
```

For the current Windows/MSYS2 UCRT64 setup, run in PowerShell:

```powershell
$env:Path = "C:\msys64\ucrt64\bin;" + $env:Path

cmake -S . -B build -G Ninja `
    -DCMAKE_C_COMPILER=C:/msys64/ucrt64/bin/clang.exe `
    -DCMAKE_CXX_COMPILER=C:/msys64/ucrt64/bin/clang++.exe `
    -DCMAKE_PREFIX_PATH=C:/msys64/ucrt64 `
    -DLLVM_DIR=C:/msys64/ucrt64/lib/cmake/llvm

cmake --build build
```

Keep the compiler and LLVM dependencies in the same toolchain environment. In CLion, select `C:\msys64\ucrt64` as the MinGW toolset rather than Bundled MinGW.

## Running the prototype

The current driver reads `test_source.doodle` and writes `output.ll` in its working directory. Run from a directory containing that source file:

```powershell
.\build\bin\sketch.exe
clang output.ll -o program.exe
.\program.exe
$LASTEXITCODE
```

The example above returns exit code `5`; it does not print `5`. Clang currently handles compiling and linking the emitted IR into a native executable.

## Formatting

The project uses `.clang-format` with Allman braces and four-space indentation. When clang-format is available at CMake configuration time:

```sh
cmake --build build --target format
cmake --build build --target format-check
```
