# Architecture decisions

## 2026-11-27: Move the business core to C++20

The user explicitly changed the required architecture from Kotlin Multiplatform
to a C++20 core. All business rules, parsers, alignment, persistence, conversion,
streak calculation, and library operations belong in C++; platform projects
must remain presentation and platform-adapter layers. The Kotlin domain,
application, alignment, database, tests, and fixtures are retained temporarily
as the behavioral specification and must not be removed until equivalent C++
tests pass.

## 2026-11-27: Use CMake, presets, and vcpkg manifest mode

CMake is the cross-platform build system, `CMakePresets.json` provides repeatable
developer configurations, and vcpkg manifest mode will pin native dependencies.
This keeps dependency selection and test builds consistent across Linux,
Windows, macOS, and Android. The project targets C++20 rather than the previous
starter's C++23 setting to match the explicit product requirement.

## 2026-11-27: Keep a stable C ABI between the core and platform code

The public core boundary uses `extern "C"`, opaque handles, stable status and
format enums, UTF-8 paths, and callback-based asynchronous operations. Native
exceptions are contained inside the implementation. Operation handles support
cancellation and can be released without blocking the caller. Platform code
must not depend on internal C++ types.

## 2026-11-27: Use WinUI 3 for the Windows presentation layer

Windows will use WinUI 3 with C# ViewModels calling the C ABI through P/Invoke.
This gives Windows users the native accessibility, windowing, and control
behavior expected from a Windows app while keeping all business logic in the
shared C++ core. Qt/QML would add a second cross-platform presentation stack and
its own deployment/licensing considerations without reducing core duplication.

## 2026-11-27: Keep SQLite and defer auth and entitlements

SQLite remains the local persistence format, implemented behind C++ repository
ports. `AuthService` and `EntitlementService` remain application interfaces
with local-only and all-unlocked defaults. No account, payment, or remote-sync
implementation is part of the MVP.

## 2026-11-27: Keep persistence behind C++ repository ports

The SQLite implementation uses explicit versioned migrations and transactions
for importing a book with its complete audio pairing. This keeps storage
replaceable for later synchronization and prevents partial imports from
appearing in the library. TagLib is used only to inspect selected audio
metadata and duration; audio playback remains a separate platform adapter.

The filesystem importer accepts local paths and local `file://` URIs only.
Platform document-provider URLs must be copied or otherwise materialized as
local files by the platform adapter before passing them to the core. Android
currently stages a selected book in its app cache for C API validation, but
still uses the Kotlin importer and SQLDelight repository for persistence until
the C API exposes library operations.

## 2026-11-27: Define a reading day as ten active reading minutes

A reading day requires ten minutes of active reading; listening alone does not
qualify. Persisted intervals are divided at local calendar-day boundaries using
time-zone rules, rather than assuming every day is 24 hours, so midnight and DST
transitions are handled correctly.

## 2026-11-27: Keep media and ASR behind replaceable ports

Audio playback and speech recognition are application ports. Native media
backends implement playback without adding platform dependencies to domain or
alignment rules; whisper.cpp can later implement the ASR port.

## 2026-11-27: Cache the end of each recognized audio fragment

When matching a transcript from the short fragment before playback stops, cache
the final matched word at the stop timestamp, rather than pairing the fragment's
first word with the stop time. This makes the first read position correspond to
where playback actually stopped. Later switches interpolate between saved
anchors without invoking ASR; a new match replaces only anchors that would
break the text/audio ordering.

The initial search remains a configurable, left-biased 20% text window and
expands only when the score or margin is too low. If ASR cannot find a confident
match, return the audio-ratio estimate with an explicit correction warning
instead of silently treating it as exact. The
synchronization use case is covered with deterministic fake-ASR tests and
round-trip cases using the matching `test-data/sample.txt` audiobook, including
the missing-intro recording. Native playback is wired through AVQueuePlayer on
Apple, MediaPlayer on Android, and the JVM audio providers on Windows. The
platform reader currently uses a ratio estimate and reports that it may be
inaccurate; it is not yet wired to the ASR/cache use case or whisper.cpp.

## 2026-11-27: Group native code by feature and keep the C API in bridge/

Native C++ implementations now live in `core/library`, `core/sync`,
`core/storage`, and `core/stats`; the public C API and its implementation live
in `bridge/`. This removes the native build's separate domain/application/
infrastructure library targets while keeping the existing C++ logic and tests.

Kotlin Multiplatform and SQLDelight have not been removed yet. Android, Apple,
and Windows projects still compile against those modules, and the C API
currently exposes file validation only. Deleting those modules now would break
the app builds and library/import behavior; migrate their callers to C++ first,
then remove the compatibility modules in a separately verified step.

## 2026-11-27: Use four persistent native navigation sections

Mobile uses the platform tab bar; desktop uses a left navigation area. Section
names are resource-backed for English and Russian. Selection and per-section
view state use each platform's native persistence mechanism. Compose shares a
semantic palette and spacing scale; SwiftUI uses dynamic colors and
system-scaled type. Home and Profile show empty states rather than fabricated
progress because the current core/API does not track or expose last-opened
books or profile statistics yet.
