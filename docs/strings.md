# Initial string support

`string` is an immutable sequence of valid UTF-8 bytes. Literal storage lasts
for the entire program. A variable may be reassigned, but the bytes themselves
cannot be modified. Assignment and parameter passing copy a pointer and length.

```doodle
greeting: string := "Hello, world!";
copy := greeting;
copy = "Another string";
```

The compiler represents a string as `%doodle.string = type { ptr, i64 }`:
a pointer and an unsigned 64-bit **byte count**, independent of pointer width.
This is an internal representation, not a published C ABI. No null terminator
is appended or required. Empty strings and embedded nulls work.

Supported escapes: `\"`, `\\`, `\n`, `\r`, `\t`, `\0`. Raw UTF-8 text is accepted;
raw line breaks and unknown escapes are errors. Hex and Unicode escapes are
not introduced yet. The decoded bytes are validated, rejecting malformed,
overlong, surrogate, truncated, and out-of-range UTF-8 encodings.

`==` and `!=` compare the full encoded byte sequences, including bytes after
embedded nulls. They do not compare addresses, normalize Unicode, or perform
locale-dependent matching. Arithmetic and ordered comparisons reject strings.

Locals, parameters, returns, assignments, overload matching, and conditional
expressions support strings. `main` still returns `int`.

Dynamic string allocation, indexing, length-access syntax, Unicode iteration,
and console output are subsequent features. The byte count is stored now;
no ambiguous character-counting operation is added.

## Applying the patch

From the Sketch repository root, with `strings.patch` downloaded there:

```sh
git apply --check strings.patch
git apply strings.patch
cmake --build build
```

The patch targets commit `59ca1c552450fcb2bb9c7bfcd2db0c4b047c6c7e` and does not
replace whole files. Keep any newer local loop work. If the check reports a
conflict, resolve the relevant hunks rather than replacing your local files.

Compile `examples/strings.doodle` normally; the executable should return `8`.

```powershell
python tests/strings.py build/bin/sketch.exe --clang clang
```

Frontend-only checks can also be built independently of LLVM:

```sh
g++ -std=c++20 -I include src/lexer.cpp src/string_literal.cpp src/parser.cpp src/semanticanalyzer.cpp tests/strings_frontend.cpp -o strings_frontend
./strings_frontend
```

The development environment passed 68 frontend checks, including an ASan/UBSan
run with leak detection disabled because this sandbox cannot inspect `/proc`. LLVM development libraries were unavailable there, so the full LLVM build
and the 20 native end-to-end cases still need to be run in the existing UCRT64
environment.
