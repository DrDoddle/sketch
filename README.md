[![License: GPL v3](https://img.shields.io/badge/License--GPLv3-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![LLVM](https://img.shields.io/badge/LLVM-Compiler-orange.svg)](https://llvm.org/)

# Sketch

**Sketch** is a compiler for **Doodle**, an experimental statically typed language focused on readable code, grouped function overloads, and named arguments.

Written in C++, with a handwritten lexer, a recursive-descent parser, semantic analysis, and LLVM IR generation.

## A little Doodle

```doodle
fn add {
    >(a: int, b: int): int
    {
        a + b
    };
}

fn main {
    >(): int
    {
        add(a: 2, b: 3)
    };
}
```

Function variants live together inside a named `fn` group. Arguments are named and can be supplied in a different order from the parameters. The final expression of a block supplies its value.

## Current features

- `int` and `bool` types, with integer and boolean literals.
- Arithmetic: `+`, `-`, `*`, `/`, unary `+` and unary `-`.
- Comparisons: `==`, `!=`, `<`, `<=`, `>`, `>=`, plus boolean negation with `!`.
- Operator precedence and parenthesized expressions.
- Local variables with explicit or inferred types, and reassignment.
- Blocks containing statements followed by a final value expression.
- Value-producing `if/else` expressions with branch-local declarations.
- Grouped function variants, named arguments, and recursive calls.
- Name resolution, overload matching, and type checking.
- LLVM IR generation, verification, and `.ll` file output.
- A VS Code syntax-highlighting extension in `editors/vscode`.

Sketch is an early prototype. The language and syntax are still evolving. Loops, strings, imports, and a standard library are future work.

## Variables and types

```doodle
a: int := 5;          // Declare with an explicit type.
b := 3;              // Infer the type from the initializer.
a = a + b;           // Assign to an existing variable.

accepted: bool := a >= 5;
rejected := !accepted;
different := a != b;
```

Variables must be initialized, and assignments must preserve their type. Reassignment is currently permitted without a mutability annotation; explicit mutability rules are planned.

Arithmetic and ordered comparisons require integers. Equality and inequality accept matching integer or boolean operands. Booleans and integers do not implicitly convert into one another.

`int` currently maps to LLVM `i32`, and `bool` maps to `i1`. Addition, subtraction, multiplication, and negation currently wrap at 32 bits. Integer division truncates toward zero and traps on division by zero or signed division overflow. These arithmetic rules may evolve.

## Conditional expressions and recursion

```doodle
fn fib {
    >(n: int): int
    {
        if n <= 1
        {
            n
        }
        else
        {
            fib(n: n - 1) + fib(n: n - 2)
        }
    };
}

fn main {
    >(): int
    {
        fib(n: 10)
    };
}
```

This program returns `55`. Conditions must be boolean, both branches must produce the same type, and only the selected branch executes. The current implementation requires an `else` branch and a final value in each branch.

Conditionals can also initialize variables:

```doodle
largest := if a > b { a } else { b };
```

## Building

Requires CMake 3.24+, a C++20 compiler, and LLVM development headers and libraries. Ninja is used in these examples. The development setup uses LLVM 23.1.3.

```sh
cmake -S . -B build -G Ninja -DLLVM_DIR="/path/to/llvm/lib/cmake/llvm"
cmake --build build
```

For Windows with MSYS2 UCRT64, run in PowerShell:

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

## Running

Sketch accepts one source-file path and writes `output.ll` in the working directory:

```powershell
.\build\bin\sketch.exe examples/test_source.doodle
```

After successful compilation, use Clang to compile and link the IR:

```powershell
clang output.ll -o program.exe
.\program.exe
$LASTEXITCODE
```

The value returned by Doodle's `main` becomes the executable's exit code; it is not printed to the console. Sketch itself returns `0` on successful compilation and a nonzero code on failure.

The compiler currently overwrites `output.ll` on success. A failed compilation may leave an older output file behind, so only compile the IR after Sketch reports success.

## VS Code highlighting

From the repository root:

```powershell
code --new-window --extensionDevelopmentPath="$PWD/editors/vscode" .
```

Open a `.doodle` file in the development window. The extension provides syntax highlighting and bracket configuration; it does not yet provide compiler diagnostics or autocomplete.

## Formatting

The project uses `.clang-format` with Allman braces and four-space indentation. When clang-format is available at CMake configuration time:

```sh
cmake --build build --target format
cmake --build build --target format-check
```

## License

Copyright (c) 2026 Joshua Maldonado.

Sketch is licensed under the GNU General Public License, version 3. See [LICENSE](LICENSE) for the full license text. LLVM and other dependencies retain their respective licenses.
