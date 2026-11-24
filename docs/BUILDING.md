# Building the Project

This document describes the requirements and procedures for configuring, building, and
installing the Reader application.

## Requirements

The following tools are required to build the project:

- **CMake** 3.25 or later.
- **C++ compiler** with C++23 support.
- **Git** for downloading external dependencies.
- **Python** 3.8 or later for the build management script.

Supported compilers include:

- GCC
- Clang
- AppleClang
- MSVC

The following tools are optional:

- **clang-format** for C++ code formatting.
- **GCC or Clang** for sanitizer configurations.

## Dependencies

The project uses CMake FetchContent to automatically download and configure its external
dependencies.

| Dependency                                           | Version | Purpose      |
| ---------------------------------------------------- | ------- | ------------ |
| [spdlog](<https://github.com/gabime/spdlog>)         | 1.15.3  | Logging      |
| [GoogleTest](<https://github.com/google/googletest>) | 1.17.0  | Unit testing |

No manual installation of these dependencies is required.

An active internet connection is required during the initial configuration if the
dependencies are not already available locally.

## Build Configurations

The project provides several predefined configurations through `CMakePresets.json`.

| Preset    | Build Type | Description                                     |
| --------- | ---------- | ----------------------------------------------- |
| `default` | Debug      | Standard development build                      |
| `release` | Release    | Optimized build                                 |
| `asan`    | Debug      | AddressSanitizer and UndefinedBehaviorSanitizer |
| `tsan`    | Debug      | ThreadSanitizer                                 |

Each configuration uses a separate build directory:

text

```text
build/
build-release/
build-asan/
build-tsan/
```

All configurations enable testing and treat compiler warnings as errors by default.

## Building with CMake

### Debug Build

Configure the project:

bash

```bash
cmake --preset default
```

Build the application and tests:

bash

```bash
cmake --build --preset default --parallel
```

The resulting executable is located in:

text

```text
build/reader
```

On Windows, the executable has the `.exe` extension and may be located inside a
configuration-specific subdirectory, depending on the generator.

### Release Build

To create an optimized Release build:

bash

```bash
cmake --preset release
cmake --build --preset release --parallel
```

The resulting executable is located in:

text

```text
build-release/reader
```

### Custom Build Configuration

The project supports additional CMake configuration through cache variables.

For example, to disable tests:

bash

```bash
cmake --preset default -DREADER_BUILD_TESTS=OFF
```

To allow compiler warnings without treating them as errors:

bash

```bash
cmake --preset default -DREADER_WARNINGS_AS_ERRORS=OFF
```

For additional configuration options, refer to `CMakeLists.txt`.

## Build Management Script

The project provides a Python utility script, `tools.py`, located in the project root.

It simplifies common development tasks, including configuration, compilation, testing,
formatting, and cleaning build artifacts.

Run the script from the project root:

bash

```bash
python tools.py <command>
```

Available commands:

| Command        | Description                                                     |
| -------------- | --------------------------------------------------------------- |
| `build`        | Configure, build, and test the Debug configuration.             |
| `release`      | Configure, build, and test the Release configuration.           |
| `test`         | Configure, build, and run the Debug tests.                      |
| `asan`         | Configure, build, and test with ASan/UBSan.                     |
| `tsan`         | Configure, build, and test with TSan.                           |
| `format`       | Format all C++ source and header files.                         |
| `format-check` | Check code formatting without modifying files.                  |
| `clean`        | Remove all build directories and the compilation database.      |
| `all`          | Run formatting checks and all configured build and test checks. |

Examples:

Build and test the Debug configuration:

bash

```bash
python tools.py build
```

Build the Release configuration:

bash

```bash
python tools.py release
```

Run all validation checks:

bash

```bash
python tools.py all
```

The `all` command executes the following steps:

1. C++ formatting validation.
2. Debug build and tests.
3. ASan/UBSan build and tests.
4. TSan build and tests.

Execution stops immediately if any step fails.

The formatting commands require `clang-format` to be installed and available in the system
PATH.

## Installation

The project defines a basic CMake installation rule for the Reader executable.

After building the project, install the application using:

bash

```bash
cmake --install build --prefix install
```

The executable will be installed into:

text

```text
install/bin/
```

The installation prefix can be changed as needed:

bash

```bash
cmake --install build --prefix /usr/local
```

## Cleaning Build Artifacts

To remove generated build directories and the compilation database, run:

bash

```bash
python tools.py clean
```

The command removes the following directories:

- `build/`
- `build-release/`
- `build-asan/`
- `build-tsan/`

It also removes the root-level `compile_commands.json` file if present.

## Additional Information

- The project requires C++23 and disables compiler-specific language extensions.
- External dependencies are managed through CMake FetchContent.
- Tests are enabled by default and can be disabled using `READER_BUILD_TESTS=OFF`.
- Compiler warnings are enabled and treated as errors by default.
- Sanitizers are optional and disabled by default.
- ASan/UBSan and TSan cannot be enabled simultaneously.
- Sanitizer configurations require GCC or Clang.
- CMake generates `compile_commands.json` to support IDE integration and static analysis.

For more information about the project, refer to:

- Architecture — project structure and architectural decisions.
- Design — design principles and implementation details.
- Development — development workflow and coding conventions.
- Testing — testing strategy and test execution.
