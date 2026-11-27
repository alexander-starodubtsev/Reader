# GoodReader implementation progress

## Architecture

GoodReader is being migrated to a C++20 business core with a stable C API and
native presentation layers. Existing Kotlin Multiplatform source, tests,
SQLDelight schema, and fixtures remain as the behavioral specification until
the corresponding C++ behaviors and platform integrations are complete.

## Completed

### C++ core skeleton and domain/application/alignment

- Added the CMake project, Debug/Release/LLVM coverage presets, vcpkg manifest,
  C API, and native CI jobs.
- Ported domain values, application ports/use cases, local auth and entitlement
  defaults, alignment normalization/matching/adaptive search, anchors, and
  streak calculation.
- Added deterministic ASR fixtures for repeated passages, pacing differences,
  abridged audio, noisy recognition, wrong editions, and short/long books.
- macOS native build and all 40 pre-infrastructure C++ tests passed.
- LLVM line coverage passed the 80% domain/application gate.

### SQLite persistence and filesystem import

- Implemented SQLite schema versions 1–3, preserving version-1 book data.
- Added transactional repository operations for books, paired/chapter-ordered
  audio tracks, reading/listening progress, reading sessions, and alignment
  anchors.
- Added filesystem import for PDF, EPUB, and FB2 book paths with optional MP3,
  M4A, M4B, OGG, and WAV tracks. Audio duration is read through TagLib; pairing
  is persisted atomically.
- Added integration tests for fresh and legacy migrations, repository queries,
  transaction rollback, progress/session/alignment persistence, optional audio,
  supported formats, multi-track imports, invalid files, and ID generation.
- The final native suite passed **51/51 tests**. LLVM line coverage: domain
  **96.23%**, application **94.74%**.

### Android C API bridge (in progress)

- Added an Android NDK/JNI shared library that calls the existing C API.
- Android import now stages the selected content-provider book in app cache and
  validates its local UTF-8 path through C++ before the existing import flow.
- Added an instrumented JNI validation test and CI SDK installation for the
  required CMake and NDK versions.
- The Android application still uses Kotlin application services and
  SQLDelight for import and library persistence. The current C API only exposes
  file validation, so this is not yet a full business-core rewire.

### Architecture simplification (in progress)

- Grouped native implementations under `core/library`, `core/sync`,
  `core/storage`, and `core/stats`; moved their native headers beside those
  features.
- Moved the C API from `core/` into `bridge/` and updated desktop/Android CMake
  references plus the native coverage target.
- Rewrote the root README and simplified the architecture page; added brief
  READMEs for the retained top-level and native feature folders.
- Rebuilt and ran all 51 native tests successfully after separating the bridge.
- Updated the coverage gate to match the simplified feature layout; the
  measured business-logic line coverage is **87.60%**, above the 80% minimum.
- Android debug APK still builds with the relocated JNI C API source.
- Kotlin/KMP, SQLDelight, and the combined Apple project remain because active
  app targets still depend on them. Removing those dependencies requires the
  C API and platform adapters to support library/import behavior first.

### Four-section navigation (in progress)

- Added Home, Library, Add book, and Profile navigation to iOS/macOS, Android,
  and Windows; mobile uses bottom navigation and desktop a left rail/sidebar.
- Added English/Russian tab and empty-state resources, shared light/dark color
  tokens, consistent Compose typography/spacing, and persisted tab selection.
- Preserved per-tab Compose state and Apple tab view state; added Apple
  localization tests and an Android navigation state-restoration UI test.
- Android APK/unit tests, Windows JVM tests, and Apple Swift package tests pass.
- The Android instrumentation test compiles but cannot run here because no
  emulator/device is installed. Windows JVM tests pass; desktop packaging
  succeeds on this Mac with the Compose JDK-vendor check disabled (this does
  not produce or validate a Windows installer).
- This is only the navigation shell. Home progress cards, persistent reader
  progress, library sorting and filters, full-screen import
  preview/reordering/cancellation, profile charts,
  dedicated per-tab ViewModels, and core-backed metrics still need
  implementation. The current C++ core records reading sessions but does not
  aggregate profile metrics or persist unique words read, listening time,
  finished-book counts, or last-opened book; its C API does not expose
  library/progress operations.

### Reader parsing and Android reader (in progress)

- Added a C++ reader document model and parsers for EPUB, FB2, and text-based
  PDF, with chapter/word positions independent of visual page layout. Scanned
  PDFs return a clear no-text-layer status.
- Exposed document opening, metadata, chapter text, stable positions, and
  alignment-index mapping through the C API.
- Added an Android JNI reader path and a basic reflowable Compose reader with
  chapter navigation and progress seeking. Android can pass alignment anchors
  through JNI to map cumulative audio times to text positions and back.
- Android APK assembly, JVM unit tests, and Android instrumentation-test
  compilation pass. The native reader/C API GTest suite has not been run after
  these additions; Android's NDK build does compile the C++ reader and bridge.
- This is not yet a complete reader/player flow: reading positions are not
  persisted, there is no playback engine or audio-to-text anchor generation,
  and only Android has a reader screen. Bookmarks, in-book search, reading
  settings, platform background controls, and Apple/Windows reader UIs remain
  unimplemented.

## Not yet implemented

- Extend the C API for library/import operations, then switch Compose away from
  Kotlin domain/application/SQLDelight implementations.
- Apple C API bindings and SwiftUI integration.
- WinUI 3 application and P/Invoke integration.
- Persist reader/audio positions in the C++ storage layer and expose them
  through the C API.
- Add audio playback, platform background/media controls, ASR-based alignment
  generation, and complete read/listen switching.
- PDF conversion and app-level streak UI/persistence.
- Run Android instrumentation tests on an emulator/device and verify native
  CI on Linux and Windows. Android SDK/NDK are installed locally; there is no
  configured emulator/device for instrumentation tests.

## Next steps

1. Expand the stable C API with import, library listing/search, and progress
   operations backed by the new SQLite repository.
2. Connect those APIs to Compose/JNI and add Android instrumentation tests for
   importing and searching paired books.
3. Add equivalent Apple and Windows bindings, then remove Kotlin specification
   modules only when their C++ and native-platform tests are passing.
4. Persist reader positions in the C++ storage layer and expose them through
   the C API; add playback and platform audio adapters before enabling
   read/listen controls.
