#!/usr/bin/env python3

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent

BUILD_DIRS = [
    "build",
    "build-release",
    "build-asan",
    "build-tsan",
]

SOURCE_DIRS = ["src", "include", "tests"]


def run(command):
    print(f"\n> {' '.join(map(str, command))}\n")
    subprocess.run(command, cwd=ROOT, check=True)


def cmake(*args):
    run(["cmake", *args])


def configure(preset):
    cmake("--preset", preset)


def build(preset):
    cmake("--build", "--preset", preset, "--parallel")


def test(preset):
    run(["ctest", "--preset", preset])


def build_and_test(preset):
    configure(preset)
    build(preset)
    test(preset)


def get_cpp_files():
    files = []

    for directory in SOURCE_DIRS:
        path = ROOT / directory

        if not path.exists():
            continue

        for extension in ("*.cpp", "*.hpp", "*.h"):
            files.extend(path.rglob(extension))

    return sorted(files)


def format_code(check=False):
    if shutil.which("clang-format") is None:
        sys.exit("Error: clang-format is not installed.")

    files = get_cpp_files()

    if not files:
        print("No C++ files found.")
        return

    args = ["clang-format"]

    if check:
        args.extend(["--dry-run", "--Werror"])
    else:
        args.append("-i")

    run(args + [str(file) for file in files])


def clean():
    for directory in BUILD_DIRS:
        path = ROOT / directory

        if path.exists():
            shutil.rmtree(path)
            print(f"Removed {directory}")

    compile_commands = ROOT / "compile_commands.json"

    if compile_commands.exists():
        compile_commands.unlink()


def all_checks():
    print("\n=== FORMAT CHECK ===")
    format_code(check=True)

    print("\n=== DEBUG ===")
    build_and_test("default")

    print("\n=== ASAN + UBSAN ===")
    build_and_test("asan")

    print("\n=== TSAN ===")
    build_and_test("tsan")

    print("\nAll checks passed!")


def main():
    parser = argparse.ArgumentParser(
        description="Reader project build manager"
    )

    parser.add_argument(
        "command",
        choices=[
            "build",
            "release",
            "test",
            "asan",
            "tsan",
            "format",
            "format-check",
            "clean",
            "all",
        ],
    )

    args = parser.parse_args()

    try:
        commands = {
            "build": lambda: build_and_test("default"),
            "release": lambda: build_and_test("release"),
            "test": lambda: build_and_test("default"),
            "asan": lambda: build_and_test("asan"),
            "tsan": lambda: build_and_test("tsan"),
            "format": lambda: format_code(),
            "format-check": lambda: format_code(check=True),
            "clean": clean,
            "all": all_checks,
        }

        commands[args.command]()

    except subprocess.CalledProcessError as error:
        sys.exit(error.returncode)


if __name__ == "__main__":
    main()
