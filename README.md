# push_swap

`push_swap` is a C++20 implementation for the 42 push_swap project.

## Requirements

- CMake 3.25 or newer
- A C++20 compiler

Confirmed environments:

- Windows with MinGW-w64 / GNU g++ 12.2.0
- macOS 13 with Homebrew CMake and Homebrew LLVM clang++ 22.1.8

## Build on Windows

Use MinGW Makefiles. Make sure `g++` and `cmake` are available in `PATH`.

You can check them with:

```cmd
g++ --version
cmake --version
```

Then build:

```cmd
cmake -S . -B build-make -G "MinGW Makefiles"
cmake --build build-make --target push_swap
```

To build the checker:

```cmd
cmake --build build-make --target checker
```

## Build on macOS

The default AppleClang on older macOS installations may fail to build C++20 code using `<source_location>`.
If that happens, install Homebrew LLVM and configure CMake with Homebrew clang++.

```bash
brew install cmake llvm
```

macOS builds may require an updated Command Line Tools installation.
If Homebrew reports outdated Command Line Tools, update them before installing or upgrading CMake/LLVM.

On Intel Mac, Homebrew is usually installed under `/usr/local`:

```bash
rm -rf build
cmake -S . -B build \
  -DCMAKE_C_COMPILER=/usr/local/opt/llvm/bin/clang \
  -DCMAKE_CXX_COMPILER=/usr/local/opt/llvm/bin/clang++
cmake --build build --target push_swap
```

On Apple Silicon, Homebrew is usually installed under `/opt/homebrew`:

```bash
rm -rf build
cmake -S . -B build \
  -DCMAKE_C_COMPILER=/opt/homebrew/opt/llvm/bin/clang \
  -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++
cmake --build build --target push_swap
```

To build the checker:

```bash
cmake --build build --target checker
```

## Run

Windows:

```cmd
.\push_swap 3 2 1
```

macOS:

```bash
./push_swap 3 2 1
```

## Check Output

Windows:

```cmd
.\push_swap 3 2 1 | .\checker 3 2 1
```

macOS:

```bash
./push_swap 3 2 1 | ./checker 3 2 1
```

If the result is valid, checker prints:

```text
OK
```
