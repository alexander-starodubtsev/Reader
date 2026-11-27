# Architecture

```text
Native UI (SwiftUI / Compose / WinUI)
                  │
             platform adapter
                  │
             bridge/ (C API)
                  │
       core/ (C++20 features + SQLite)
                  │
       local files and database
```

UI code displays state and forwards user actions; reading, import, persistence,
progress, streak, conversion, and synchronization rules belong in `core/`.
The bridge translates data and errors and does not make product decisions.

| Feature | Location |
|---|---|
| Home and Library | `apps/*` screens; library operations in `core/library/` |
| Add book and audio | UI file picker; import logic in `core/library/` |
| Profile and preferences | platform UI; local identity defaults in core |
| Reading progress | `core/library/` |
| Audio/text sync | `core/sync/` |
| Reading streak | `core/stats/` |
| SQLite data | `core/storage/` |

## Where to change things

- Add a library/import rule: `core/library/`.
- Change matching or synchronization: `core/sync/`.
- Change database schema or persistence: `core/storage/`.
- Change the C boundary used by apps: `bridge/`.
- Change a screen or platform picker: its folder under `apps/`.

Platform folders currently contain existing app projects; migration to the
final one-folder-per-platform layout is incomplete. Kotlin/KMP and SQLDelight
are still required by current apps, so they are retained until those
integrations and their test coverage are replaced.
