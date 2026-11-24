# Reader

A C++23 reader application.

## Requirements

- CMake 3.20+
- C++20 compiler
- Git

## Build

```bash
cmake --preset default
cmake --build --preset default
```

## Run

```bash
./build/reader
```

## Tests

```bash
ctest --preset default
```
```
ctest --test-dir build --output-on-failure
```

## Release build

```bash
cmake --preset release
cmake --build --preset release
ctest --preset release
```

## Project structure

- `include/` - Public headers
- `src/` - Source files
- `tests/` - Tests
- `cmake/` - CMake modules
- `docs/` - Documentation
- `.github/` - GitHub configuration

## Development

Use feature branches and Pull Requests.

Commit messages follow Conventional Commits.

Example:

`feat(main.cpp): add epub support`
