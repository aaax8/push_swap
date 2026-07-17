# push_swap

`push_swap` is a C++20 implementation for the 42 push_swap project.

Note: This repository has not been fully refactored or cleaned up. Many files are remnants of earlier experiments and are not used by the current solver. They remain in the repository for now, but they do not affect running the current implementation.

## Approach Overview

A detailed, step-by-step explanation of this algorithm is available in [the report](docs/push_swap_report.pdf). This section gives a brief overview of the approach used in this project.

This project uses an Iterated Greedy approach to push_swap. Iterated Greedy repeatedly destroys part of the current solution and then reconstructs it using a greedy method.

In this algorithm, an initial solution is first created by leaving a sequence close to circular ascending order in stack A, moving the other values to stack B as chunks by value range, and using beam search to find a procedure for moving the values in B back to A.

A solution is represented not as the final command sequence itself, but by rules that determine when and to which stack each value moves, and its relative position in the circular order of the destination stack. The algorithm improves the initial solution by repeatedly removing information about some values from the current solution, keeping the remaining solution fixed, and greedily deciding those rules again for the removed values.

This implementation is not intended to be used as-is for a 42 project submission. It is written in C++ and manages complex states and data structures using standard library containers, templates, and classes. The current implementation also has a long runtime, so it is not directly suitable for review submission.

## Limitations

This implementation has only been checked with valid inputs. It has not been confirmed whether invalid inputs are handled with appropriate error messages.

It was made to test the cases with 100 and 500 elements. Other input sizes may not work; in that case, the program reports an error.

## Requirements

- CMake 3.25 or newer
- A C++20 compiler

Confirmed environments:

- Windows with MinGW-w64 / GNU g++ 12.2.0
- macOS 13 with Homebrew CMake and Homebrew LLVM clang++ 22.1.8

## Example Runs

The following examples show one run for 100 elements and one run for 500 elements. Each example includes the input sequence, the generated command sequence, and a video showing how the stack is sorted.

### 100 Elements

https://github.com/user-attachments/assets/ea1a9124-d212-4739-9f23-cf8611b37754

- Input sequence: [input.txt](examples/n100/input.txt)
- Generated commands: [commands.txt](examples/n100/commands.txt)

### 500 Elements

https://github.com/user-attachments/assets/0a47335f-785a-4a04-bee3-c7446af67800

- Input sequence: [input.txt](examples/n500/input.txt)
- Generated commands: [commands.txt](examples/n500/commands.txt)

## Benchmark Results

The following benchmark was measured on 500 random cases for each input size. The measurements were collected with [push_swap_tester](https://github.com/nafuka11/push_swap_tester).

Environment:

- CPU: Intel Core i7-13700F
- GPU: NVIDIA GeForce RTX 3070

![Benchmark results](images/result.png)

For 100 elements, the median operation count was 363, and each case took about 1 minute 30 seconds.

For 500 elements, the median operation count was 2970, and each case took about 2 minutes 20 seconds.

Runtime depends on the machine and build environment.

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

Note: On macOS, Clang may emit stricter compiler warnings during the build. In the confirmed macOS environment, the build completed successfully and the generated executable passed basic checks, so these warnings are currently accepted rather than fixed.

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
