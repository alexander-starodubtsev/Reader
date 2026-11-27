# GoodReader

GoodReader is a local-first reader for user-provided PDF, EPUB, and FB2 books,
with optional audio and synchronized reading/listening progress. Business logic
is C++20; platform screens stay native and call it through `bridge/`.

## Build the core

Requirements: CMake 3.25+, Ninja, a C++20 compiler, and vcpkg.

```sh
export VCPKG_ROOT="$HOME/vcpkg"
cmake --preset default
cmake --build --preset default
ctest --preset default
```

The preset builds the native library and all C++ tests. On macOS, install the
tools with `brew install cmake ninja`; install vcpkg separately and bootstrap
it once. See [docs/BUILDING.md](docs/BUILDING.md) for platform-specific setup.

## Project map

- `core/`: book/library, reader progress, audio sync, storage, and streak rules.
- `bridge/`: stable C API used by platform code.
- `apps/`: Android, Apple, and Windows app projects.
- `tests/`: C++ unit and integration tests plus fixtures.
- `docs/`: architecture, decisions, build steps, and migration progress.

The current native C++ suite has 51 tests. Kotlin modules remain temporarily
because Android, Apple, and Windows app targets still depend on them; removing
them before replacing those app integrations would break builds and current
behavior. See [docs/PROGRESS.md](docs/PROGRESS.md).

## Navigation

The native clients now show four sections: **Home**, **Library**, **Add book**,
and **Profile**. iOS and Android use a bottom bar; macOS and Windows use a left
navigation area. Tab names follow the system language (English or Russian),
the selected section is restored, and colors follow the system light/dark
setting. Home and Profile currently show empty states because the C++ core does
not yet expose last-opened-book or profile statistics.

## Product rules

Books and audio are selected locally by the user; the app does not fetch
content. A reading streak day requires ten active reading minutes. Login,
subscriptions, and remote sync are postponed.
