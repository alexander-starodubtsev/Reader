# Reader project task runner
# Build, test, format and maintain the project.

# Use Python 3.
python := "python3"
script := "tools.py"

# Default recipe.
# Run `just` to see available commands.
default:
    @just --list

# Build debug version and run tests.
build:
    {{python}} {{script}} build

# Build release version and run tests.
release:
    {{python}} {{script}} release

# Build with AddressSanitizer and UndefinedBehaviorSanitizer.
asan:
    {{python}} {{script}} asan

# Build with ThreadSanitizer.
tsan:
    {{python}} {{script}} tsan

# Run tests using the default build preset.
test:
    {{python}} {{script}} test

# Format all C++ source files.
format:
    {{python}} {{script}} format

# Check C++ formatting without modifying files.
format-check:
    {{python}} {{script}} format-check

# Remove all build directories and generated files.
clean:
    {{python}} {{script}} clean

# Run all checks: formatting, debug, ASAN/UBSAN and TSAN.
all:
    {{python}} {{script}} all
