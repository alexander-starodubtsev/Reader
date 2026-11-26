# AudioReader — Software Architecture & Design

**Document Type:** Software Design Document (SDD)\
**Project:** AudioReader\
**Status:** Proposed Architecture\
**Version:** 1.0\
**Primary Language:** C++20\
**Target Platforms:** Windows, Linux, macOS, Android, iOS

---

## 1\. Project Overview

### 1.1. Purpose

AudioReader is a cross-platform application designed to provide seamless synchronization
between digital book text and audiobook playback.

The primary objective is to allow users to switch between reading and listening while
preserving their current position in the book.

The application must support two-way navigation:

- **Audio-to-Text:** When a user pauses audiobook playback, the application identifies the
corresponding position in the digital book and navigates to it.
- **Text-to-Audio:** When a user stops reading and switches to audio playback, the
application identifies the corresponding position in the audiobook and resumes playback
from that location.

The application is designed with an offline-first approach. All core functionality,
including speech recognition, text processing, synchronization, and progress tracking,
must operate locally without requiring an internet connection.

### 1.2. Core Requirements

The system must provide:

1. EPUB book import and parsing.
2. Local audiobook playback.
3. Offline automatic speech recognition (ASR).
4. Bidirectional text and audio synchronization.
5. Persistent reading and listening progress.
6. Background processing for computationally intensive tasks.
7. Cross-platform support through a shared C++ core.
8. A modular architecture that allows future functionality to be added without major
architectural changes.

### 1.3. Initial Scope

The initial MVP will support:

- EPUB as the primary book format.
- MP3 as the primary audiobook format.
- Local speech recognition using Whisper.
- Desktop application as the initial development target.
- Android and iOS as subsequent deployment targets.

Additional book and audio formats may be introduced through independent adapters.

---

## 2\. Architectural Principles

The project will follow a **Modular Monolith architecture with Clean Architecture
principles**.

The primary architectural objective is to maintain strict separation between business
logic, application orchestration, infrastructure, and presentation.

### 2.1. Core Principles

**Separation of Concerns**

Each module must have a clearly defined responsibility.

**Dependency Inversion**

Core business logic must not depend on specific frameworks, databases, or operating
systems.

**Platform Independence**

The same C++ core must be reusable across all supported platforms.

**Offline-First Design**

The application must not require network connectivity for its primary functionality.

**Modularity**

Components must expose well-defined interfaces and minimize dependencies on implementation
details.

**Testability**

The alignment engine and navigation engine must be independently testable without
requiring the UI or actual audio playback.

**Performance Awareness**

Computationally expensive operations must execute outside the main UI thread.

### 2.2. Architectural Constraints

- The Domain layer must not depend on Qt.
- The Alignment Engine must not depend on Qt, SQLite, or FFmpeg.
- UI components must not directly access database implementations.
- Audio playback and audio transcription must remain separate subsystems.
- All long-running operations must support progress reporting and cancellation where
practical.

---

## 3\. Technology Stack

| Component             | Technology                             | Purpose                               |
| --------------------- | -------------------------------------- | ------------------------------------- |
| Programming Language  | C++20                                  | Main application language             |
| UI Framework          | Qt 6 / QML                             | Cross-platform interface              |
| Build System          | CMake                                  | Project configuration and compilation |
| Dependency Management | Conan 2 or vcpkg                       | External dependencies                 |
| Database              | SQLite                                 | Local persistent storage              |
| EPUB Processing       | libzip + libxml2                       | EPUB extraction and XML parsing       |
| Audio Processing      | FFmpeg                                 | Audio decoding and preprocessing      |
| Speech Recognition    | whisper.cpp                            | Offline ASR                           |
| JSON Processing       | nlohmann/json                          | Data serialization                    |
| Logging               | spdlog                                 | Application logging                   |
| Formatting            | clang-format                           | Code formatting                       |
| Static Analysis       | clang-tidy                             | Code quality                          |
| Unit Testing          | GoogleTest                             | Automated testing                     |
| Concurrency           | std::jthread + Qt threading facilities | Background processing                 |

### 3.1. Technology Selection Rationale

**C++20**

Provides modern language features, strong performance, and broad compiler support.

**Qt 6**

Provides a shared UI framework for desktop and mobile platforms, including multimedia and
platform integration facilities.

**SQLite**

Provides lightweight, embedded, transactional storage without requiring an external
database server.

**whisper.cpp**

Provides local speech recognition through a native C/C++ implementation.

**FFmpeg**

Provides audio decoding and preprocessing capabilities independently of the playback
subsystem.

**CMake**

Provides a consistent build configuration across desktop and mobile toolchains.

### 3.2. Licensing Considerations

Before distributing the application, the project must review the licenses of all
third-party dependencies and bundled models.

Particular attention must be given to:

- Qt licensing and deployment requirements.
- FFmpeg build configuration and LGPL/GPL implications.
- Whisper model licensing.
- Third-party library redistribution requirements.

A dependency license inventory must be maintained throughout development.

---

## 4\. High-Level System Architecture

The application consists of five architectural layers.

### 4.1. Presentation Layer

**Technology:** Qt 6 / QML

Responsible for all user-facing functionality.

Components:

- Library UI.
- EPUB Reader.
- Audio Player Controls.
- Reading Progress Display.
- Synchronization Indicators.
- Application Settings.

The Presentation Layer communicates exclusively with the Application Layer.

It must not directly access database implementations, audio decoders, or alignment
algorithms.

### 4.2. Application Layer

Responsible for coordinating application operations and implementing user-facing
workflows.

Main services:

- BookService.
- BookImportService.
- ReaderService.
- PlaybackService.
- NavigationService.
- ProgressService.
- ProcessingManager.

This layer coordinates interactions between domain logic and infrastructure
implementations.

### 4.3. Domain Layer

Contains the fundamental business models and rules.

Main components:

- Book.
- Chapter.
- TextSegment.
- AudioTrack.
- Transcript.
- AlignmentMap.
- ReadingPosition.

The Domain Layer must remain independent of external frameworks.

### 4.4. Infrastructure Layer

Responsible for implementing external integrations.

Components:

- EPUB Parser.
- Audio Decoder.
- Whisper Transcriber.
- SQLite Repository.
- File Storage.
- Audio Playback Adapter.

Infrastructure components implement interfaces defined by the application or domain-facing
abstractions.

### 4.5. Platform Layer

Responsible for platform-specific functionality.

Supported platforms:

- Windows.
- Linux.
- macOS.
- Android.
- iOS.

Responsibilities:

- File access.
- Audio sessions.
- Background audio.
- Permissions.
- Application lifecycle.
- Platform-specific resource management.

---

## 5\. Repository Structure

The project will use a monorepository structure.

text

```text
audioreader/
│
├── CMakeLists.txt
├── CMakePresets.json
├── conanfile.py
│
├── apps/
│   ├── desktop/
│   │   ├── main.cpp
│   │   └── CMakeLists.txt
│   │
│   └── mobile/
│       ├── main.cpp
│       └── CMakeLists.txt
│
├── src/
│   │
│   ├── domain/
│   │   ├── book/
│   │   ├── audio/
│   │   ├── alignment/
│   │   └── progress/
│   │
│   ├── application/
│   │   ├── services/
│   │   ├── commands/
│   │   └── interfaces/
│   │
│   ├── infrastructure/
│   │   ├── epub/
│   │   ├── audio/
│   │   ├── asr/
│   │   ├── database/
│   │   └── filesystem/
│   │
│   ├── alignment/
│   │   ├── normalization/
│   │   ├── matching/
│   │   ├── indexing/
│   │   └── navigation/
│   │
│   └── platform/
│       ├── desktop/
│       ├── android/
│       └── ios/
│
├── ui/
│   ├── qml/
│   │   ├── screens/
│   │   ├── components/
│   │   └── styles/
│   │
│   └── viewmodels/
│
├── tests/
│   ├── unit/
│   ├── integration/
│   ├── alignment/
│   └── fixtures/
│
├── resources/
│   ├── models/
│   └── icons/
│
├── docs/
│   ├── ARCHITECTURE.md
│   ├── DOMAIN_MODEL.md
│   ├── ALIGNMENT.md
│   ├── THREADING.md
│   ├── BUILDING.md
│   └── DECISIONS/
│
└── third_party/
```

The directory structure should reflect architectural boundaries rather than merely
grouping files by technical type.

---

## 6\. CMake Target Architecture

The project must be divided into independently buildable libraries.

| Target               | Type       | Responsibility              |
| -------------------- | ---------- | --------------------------- |
| ar\_domain           | STATIC     | Domain models and types     |
| ar\_alignment        | STATIC     | Text and audio alignment    |
| ar\_navigation       | STATIC     | Bidirectional navigation    |
| ar\_book             | STATIC     | Book parsing                |
| ar\_audio            | STATIC     | Audio playback abstractions |
| ar\_asr              | STATIC     | ASR integration             |
| ar\_storage          | STATIC     | Database and file storage   |
| ar\_application      | STATIC     | Application services        |
| ar\_platform         | STATIC     | Platform-specific adapters  |
| ar\_ui               | STATIC     | Qt/QML presentation         |
| audioreader\_desktop | EXECUTABLE | Desktop application         |
| audioreader\_mobile  | EXECUTABLE | Mobile application          |

### 6.1. Dependency Rules

The following dependency rules must be maintained:

1. `ar_domain` must not depend on any other project module.
2. `ar_alignment` may depend on `ar_domain`.
3. `ar_navigation` may depend on `ar_alignment` and `ar_domain`.
4. Infrastructure modules may depend on domain interfaces and external libraries.
5. `ar_application` coordinates domain and infrastructure abstractions.
6. UI modules depend on application interfaces.
7. Platform-specific modules must not introduce dependencies into the domain layer.

Circular dependencies are prohibited.

### 6.2. Build Configuration

The project uses:

- C++20.
- CMake 3.25 or later.
- CMAKE\_CXX\_STANDARD\_REQUIRED.
- CMAKE\_CXX\_EXTENSIONS OFF.
- Separate build configurations for Debug and Release.
- Platform-specific toolchain files where required.

The project must support independent builds of its core libraries and test targets.

---

## 7\. Domain Models

All core domain models must be independent of external frameworks.

### 7.1. Position Types

Text positions and audio timestamps must use separate types.

cpp

```cpp
struct TextPosition {
    uint64_t tokenIndex;
};

struct AudioTimestamp {
    double seconds;
};

struct BookId {
    std::string value;
};

struct ChapterId {
    std::string value;
};
```

This prevents accidental mixing of text offsets and audio timestamps.

### 7.2. Book Model

cpp

```cpp
struct TextSegment {
    uint64_t id;
    uint32_t chapterIndex;
    uint32_t segmentIndex;

    std::string text;

    TextPosition start;
    TextPosition end;
};

struct Chapter {
    ChapterId id;
    std::string title;

    std::vector<TextSegment> segments;
};

struct Book {
    BookId id;
    std::string title;

    std::vector<Chapter> chapters;

    uint64_t totalTokens;
};
```

The implementation should additionally preserve stable source locations within the
original EPUB document.

### 7.3. Audio Model

cpp

```cpp
struct TranscriptWord {
    std::string text;

    AudioTimestamp start;
    AudioTimestamp end;

    float confidence;
};

struct TranscriptSegment {
    std::vector<TranscriptWord> words;

    AudioTimestamp start;
    AudioTimestamp end;
};

struct AudioTrack {
    std::string id;
    std::string filePath;

    AudioTimestamp duration;
};
```

Word-level timestamps should be retained even if the initial UI only highlights entire
sentences.

### 7.4. Alignment Model

cpp

```cpp
struct AlignmentAnchor {
    TextPosition textPosition;
    AudioTimestamp audioTimestamp;

    float confidence;
};

struct AlignmentMap {
    std::vector<AlignmentAnchor> anchors;

    uint32_t algorithmVersion;
};
```

The alignment map represents the relationship between positions in the book and positions
in the audiobook.

Alignment points must be monotonic.

The system must support uncertain or unmatched regions rather than forcing invalid
correspondences.

### 7.5. Reading Position

cpp

```cpp
struct ReadingPosition {
    TextPosition text;
    AudioTimestamp audio;

    float confidence;
    bool isSynchronized;
};
```

The application should maintain a unified logical reading position while preserving the
last active reading mode.

---

## 8\. Book Engine

The Book Engine is responsible for importing and parsing EPUB files.

### 8.1. Responsibilities

- Open EPUB archives.
- Parse package metadata.
- Read the EPUB reading order.
- Extract XHTML content.
- Remove irrelevant markup.
- Preserve paragraph and chapter boundaries.
- Normalize extracted text.
- Generate stable text segments.
- Generate token positions.

### 8.2. Processing Pipeline

text

```text
EPUB File
| --- |
    v
ZIP Extraction
| --- |
    v
OPF Metadata Parsing
| --- |
    v
Reading Order Resolution
| --- |
    v
XHTML Parsing
| --- |
    v
Text Extraction
| --- |
    v
Normalization
| --- |
    v
Tokenization
| --- |
    v
Book Model
```

### 8.3. Important Requirements

The parser must preserve:

- Chapter ordering.
- Paragraph boundaries.
- Unicode characters.
- Original text locations.
- Language metadata where available.

The parser should also detect EPUB Media Overlays (SMIL), when present. Such metadata may
provide pre-existing text-to-audio synchronization and should be preferred over ASR-based
reconstruction when reliable.

---

## 9\. Audio Engine

The Audio Engine is responsible for audiobook playback.

### 9.1. Responsibilities

- Load audio tracks.
- Play, pause, and stop playback.
- Seek to timestamps.
- Report current playback position.
- Report audio duration.
- Handle playback errors.
- Support background playback through platform adapters.

### 9.2. Interface

cpp

```cpp
class IAudioPlayer {
public:
    virtual ~IAudioPlayer() = default;

    virtual void load(const AudioTrack& track) = 0;

    virtual void play() = 0;
    virtual void pause() = 0;
    virtual void stop() = 0;

    virtual void seek(AudioTimestamp position) = 0;

    virtual AudioTimestamp position() const = 0;
    virtual AudioTimestamp duration() const = 0;

    virtual bool isPlaying() const = 0;
};
```

### 9.3. Implementation

The initial implementation should use Qt Multimedia.

FFmpeg should be used separately for audio decoding and preprocessing required by the ASR
subsystem.

Playback and transcription must not share real-time execution responsibilities.

---

## 10\. Speech Recognition Engine

The Speech Recognition subsystem is responsible for converting audiobook audio into a
timestamped transcript.

### 10.1. Technology

The initial implementation will use whisper.cpp.

All speech recognition must operate locally.

### 10.2. Responsibilities

- Load local Whisper models.
- Prepare audio for inference.
- Execute speech recognition.
- Produce timestamped transcription segments.
- Report processing progress.
- Support cancellation.
- Store transcription results.

### 10.3. Interface

cpp

```cpp
class ITranscriber {
public:
    virtual ~ITranscriber() = default;

    virtual TranscriptionResult transcribe(
        const AudioInput& audio,
        const TranscriptionOptions& options,
        ProgressCallback progress,
        std::stop_token stopToken
    ) = 0;
};
```

### 10.4. Processing Modes

**Initial mode:**

Segment-level transcription with approximate timestamps.

**Future mode:**

Word-level alignment and improved timestamp accuracy.

The architecture must not assume that standard ASR timestamps provide exact word
boundaries.

---

## 11\. Alignment Engine

The Alignment Engine is the most important computational component of AudioReader.

Its purpose is to establish a correspondence between the original book text and the
recognized audiobook transcript.

### 11.1. Input

- Structured book text.
- Timestamped audio transcript.
- Language information.
- Normalization configuration.

### 11.2. Output

- Alignment Map.
- Confidence values.
- Matched text and audio intervals.
- Unmatched regions.
- Processing diagnostics.

### 11.3. Processing Pipeline

text

```text
Book Text
| --- |
    v
Text Normalization
| --- |
    v
Book Tokenization
| --- |
    +----------------------+
| --- |
                           v
                    Candidate Matching
| --- |
                           v
                    Sequence Alignment
| --- |
                           v
                    Confidence Evaluation
| --- |
                           v
                    Alignment Map
                           ^
| --- |
Transcript -> Normalization -> Tokenization
```

### 11.4. Algorithmic Components

The initial implementation should investigate:

1. Unicode and language-aware text normalization.
2. Tokenization.
3. N-gram indexing.
4. Exact and fuzzy sequence matching.
5. Candidate matching.
6. Monotonic sequence alignment.
7. Confidence estimation.
8. Local alignment refinement.

Potential algorithms include:

- Levenshtein distance.
- Jaro-Winkler similarity.
- N-gram matching.
- Smith-Waterman local alignment.
- Dynamic Time Warping (DTW), where appropriate.

The algorithms should be selected and benchmarked against real audiobook and EPUB pairs.

### 11.5. Alignment Interface

cpp

```cpp
class IAlignmentEngine {
public:
    virtual ~IAlignmentEngine() = default;

    virtual AlignmentMap build(
        const Book& book,
        const AudioTranscript& transcript
    ) = 0;

    virtual AlignmentResult findTextPosition(
        AudioTimestamp timestamp
    ) const = 0;

    virtual AlignmentResult findAudioPosition(
        TextPosition position
    ) const = 0;
};
```

### 11.6. Algorithm Versioning

Every generated Alignment Map must include an algorithm version.

This allows the application to detect outdated synchronization data and rebuild it when
necessary.

---

## 12\. Navigation Engine

The Navigation Engine uses an existing Alignment Map to translate positions between text
and audio.

It must not perform full speech recognition during ordinary navigation.

### 12.1. Interface

cpp

```cpp
class NavigationEngine {
public:
    explicit NavigationEngine(
        std::shared_ptr<const AlignmentMap> map
    );

    NavigationResult textToAudio(
        TextPosition position
    ) const;

    NavigationResult audioToText(
        AudioTimestamp timestamp
    ) const;
};
```

### 12.2. Navigation Requirements

The engine must:

- Use indexed alignment points.
- Support interpolation between anchors where appropriate.
- Search locally around uncertain matches.
- Return confidence information.
- Identify unmatched regions.
- Avoid non-monotonic navigation.
- Support chapter boundaries.

Binary search should be used to locate the nearest alignment anchors. Local matching or
interpolation should then refine the destination.

---

## 13\. Application Services

The Application Layer coordinates the subsystems.

### 13.1. ReaderService

Responsible for reader state and text navigation.

cpp

```cpp
class ReaderService {
public:
    void openBook(BookId id);

    void seekToText(TextPosition position);

    void seekToAudio(AudioTimestamp timestamp);

    void saveProgress();

    ReaderState state() const;
};
```

### 13.2. PlaybackService

Responsible for audio playback orchestration.

cpp

```cpp
class PlaybackService {
public:
    void play();
    void pause();

    void seek(AudioTimestamp timestamp);

    AudioTimestamp currentPosition() const;
};
```

### 13.3. BookImportService

Responsible for book import and processing initialization.

cpp

```cpp
class BookImportService {
public:
    JobId importBook(
        std::filesystem::path epubPath,
        std::filesystem::path audioPath
    );
};
```

### 13.4. ProcessingManager

Responsible for background processing tasks.

Responsibilities:

- Task scheduling.
- Task cancellation.
- Processing progress.
- Error reporting.
- Resource management.
- Processing state persistence.

---

## 14\. Bidirectional Synchronization Workflows

### 14.1. Audio-to-Text

When the user pauses audiobook playback:

1. PlaybackService retrieves the current audio timestamp.
2. NavigationEngine receives the timestamp.
3. The Alignment Map is searched for the corresponding text position.
4. The destination is validated using confidence information.
5. ReaderService navigates to the corresponding text segment.
6. The UI highlights the current reading position.
7. Reading progress is updated.

text

```text
Audio Pause
| --- |
    v
Current Audio Timestamp
| --- |
    v
Navigation Engine
| --- |
    v
Alignment Map Lookup
| --- |
    v
Text Position
| --- |
    v
Reader Service
| --- |
    v
Display and Highlight Text
```

### 14.2. Text-to-Audio

When the user selects a text position and switches to audio:

1. ReaderService retrieves the current text position.
2. NavigationEngine searches the Alignment Map.
3. The corresponding audio timestamp is calculated.
4. PlaybackService seeks to the destination.
5. Playback resumes.
6. Reading progress is updated.

text

```text
Text Selection
| --- |
    v
Current Text Position
| --- |
    v
Navigation Engine
| --- |
    v
Alignment Map Lookup
| --- |
    v
Audio Timestamp
| --- |
    v
Playback Service
| --- |
    v
Resume Playback
```

Both workflows must be covered by integration tests.

---

## 15\. Database Architecture

SQLite is the primary persistent storage mechanism.

### 15.1. Stored Data

The database must store:

- Book metadata.
- Chapter metadata.
- Text segments.
- Audio metadata.
- Transcription results.
- Alignment points.
- Reading progress.
- Processing jobs.
- Algorithm versions.

### 15.2. Initial Database Schema

sql

```sql
CREATE TABLE books (
    id TEXT PRIMARY KEY,
    title TEXT NOT NULL,
    language TEXT,
    total_tokens INTEGER NOT NULL DEFAULT 0,
    created_at INTEGER NOT NULL
);

CREATE TABLE chapters (
    id TEXT PRIMARY KEY,
    book_id TEXT NOT NULL,
    chapter_index INTEGER NOT NULL,
    title TEXT,
    FOREIGN KEY(book_id) REFERENCES books(id)
);

CREATE TABLE text_segments (
    id INTEGER PRIMARY KEY,
    chapter_id TEXT NOT NULL,
    segment_index INTEGER NOT NULL,
    text TEXT NOT NULL,
    token_start INTEGER NOT NULL,
    token_end INTEGER NOT NULL,
    FOREIGN KEY(chapter_id) REFERENCES chapters(id)
);

CREATE TABLE audio_tracks (
    id TEXT PRIMARY KEY,
    book_id TEXT NOT NULL,
    file_path TEXT NOT NULL,
    duration REAL NOT NULL,
    FOREIGN KEY(book_id) REFERENCES books(id)
);

CREATE TABLE transcript_words (
    id INTEGER PRIMARY KEY,
    audio_track_id TEXT NOT NULL,
    word_index INTEGER NOT NULL,
    text TEXT NOT NULL,
    start_time REAL NOT NULL,
    end_time REAL NOT NULL,
    confidence REAL,
    FOREIGN KEY(audio_track_id) REFERENCES audio_tracks(id)
);

CREATE TABLE alignment_points (
    id INTEGER PRIMARY KEY,
    book_id TEXT NOT NULL,
    text_position INTEGER NOT NULL,
    audio_timestamp REAL NOT NULL,
    confidence REAL NOT NULL,
    algorithm_version INTEGER NOT NULL,
    FOREIGN KEY(book_id) REFERENCES books(id)
);

CREATE TABLE reading_progress (
    user_id TEXT NOT NULL,
    book_id TEXT NOT NULL,
    text_position INTEGER,
    audio_timestamp REAL,
    updated_at INTEGER NOT NULL,
    PRIMARY KEY(user_id, book_id)
);

CREATE INDEX idx_alignment_text
ON alignment_points(book_id, text_position);

CREATE INDEX idx_alignment_audio
ON alignment_points(book_id, audio_timestamp);

CREATE INDEX idx_transcript_time
ON transcript_words(audio_track_id, start_time);
```

This schema represents the initial logical model. Production implementation must add
database migrations, transaction management, processing-job persistence, alignment-run
metadata, and appropriate indexes.

### 15.3. File Storage

Large media files must not be stored directly in SQLite.

The application should use local filesystem storage.

text

```text
app_data/
│
├── database/
│   └── audioreader.db
│
├── books/
│   └── book_001/
│       ├── original.epub
│       ├── audio/
│       ├── parsed/
│       └── cache/
│
└── models/
    └── whisper/
```

The storage implementation must use platform-appropriate application data directories.

---

## 16\. Multithreading and Concurrency

Long-running operations must never block the UI thread.

### 16.1. Logical Execution Contexts

| Execution Context | Responsibility                  |
| ----------------- | ------------------------------- |
| UI Thread         | Rendering and user interaction  |
| Audio Playback    | Audio output and playback state |
| ASR Worker        | Whisper inference               |
| Alignment Worker  | Text and transcript matching    |
| Import Worker     | EPUB parsing and extraction     |
| Storage Executor  | Database operations             |

These are logical execution contexts, not a requirement to create one dedicated
operating-system thread per subsystem.

### 16.2. Concurrency Requirements

- Use `std::jthread` and `std::stop_token` where appropriate.
- Use Qt signals and slots for cross-thread UI communication.
- Never update QML objects directly from worker threads.
- Avoid concurrent heavy ASR operations.
- Keep audio callbacks free from blocking operations.
- Use bounded task queues.
- Persist task states where recovery is necessary.
- Support cancellation of long-running processing tasks.

### 16.3. Processing Manager

The ProcessingManager should provide a unified task interface.

cpp

```cpp
class ProcessingManager {
public:
    JobId submit(ProcessingTask task);

    void cancel(JobId id);

    ProcessingStatus status(JobId id) const;
};
```

The actual implementation should include task scheduling, worker ownership, cancellation
handling, and resource limits.

### 16.4. Progress Persistence

The current reading position should be maintained in memory and persisted periodically.

Database writes should occur:

- When the user pauses playback.
- When the user switches reading modes.
- At periodic intervals.
- When changing books.
- When the application enters the background.
- During controlled application shutdown.

Audio playback callbacks must never perform database writes.

---

## 17\. Platform Strategy

The application will use a shared C++ core with platform-specific adapters.

### 17.1. Shared Components

The following components must remain platform-independent:

- Domain Models.
- Book Engine.
- Alignment Engine.
- Navigation Engine.
- ASR Integration.
- Storage Layer.
- Application Services.

### 17.2. Platform-Specific Components

The following components require platform-specific implementations or configuration:

- Audio Sessions.
- Background Playback.
- File Access.
- Permissions.
- Application Lifecycle.
- Model Storage.
- System Resource Management.

### 17.3. Development Order

1. Windows / Linux / macOS desktop development.
2. Android integration.
3. iOS integration.

The Desktop application will serve as the initial development and debugging environment.

Mobile deployment must be validated separately because of differences in memory
constraints, CPU capabilities, background execution policies, and application lifecycle
management.

### 17.4. Offline Requirements

The application must support its core features without network access.

This includes:

- Book import.
- Audio playback.
- Speech recognition.
- Alignment generation.
- Navigation.
- Progress persistence.

Whisper models must be available locally. Model selection should account for available
storage and memory.

---

## 18\. Performance and Resource Management

Performance optimization must be guided by profiling and real-world measurements.

### 18.1. Primary Performance Considerations

**Speech Recognition**

ASR is expected to be one of the most computationally expensive operations.

The implementation should support configurable models and hardware acceleration where
available.

**Alignment**

The Alignment Engine must use indexed matching rather than repeatedly scanning the entire
book and transcript.

**Database**

Frequently accessed positions should be indexed.

**Audio Playback**

Playback must remain responsive during simultaneous background processing.

**Memory**

Large transcripts and book contents should not require unnecessary copies.

### 18.2. Optimization Strategy

The initial implementation should prioritize:

1. Correctness.
2. Predictable memory usage.
3. Responsive playback.
4. Efficient indexing.
5. Measurable performance improvements.

Premature optimization must be avoided.

---

## 19\. Testing Strategy

Testing is a mandatory part of development.

### 19.1. Unit Tests

Each independent module must have unit tests.

Examples:

- EPUB parsing.
- Unicode normalization.
- Tokenization.
- Fuzzy matching.
- Alignment point validation.
- Binary search.
- Position interpolation.
- Progress serialization.

### 19.2. Integration Tests

Integration tests must verify:

- EPUB import and parsing.
- Audio transcription.
- Alignment generation.
- Audio-to-text navigation.
- Text-to-audio navigation.
- Database persistence.
- Recovery after application restart.

### 19.3. Alignment Evaluation

The synchronization system must be evaluated against manually verified reference
positions.

Important metrics:

- Percentage of correctly matched reference points.
- Audio navigation error in seconds.
- Text navigation error in words.
- False match rate.
- Alignment confidence calibration.
- Alignment processing time.

Testing should include:

- Different speakers.
- Different narration speeds.
- Chapter transitions.
- Repeated phrases.
- Punctuation differences.
- Missing or additional text.
- ASR transcription errors.
- Long pauses and silence.

No fixed accuracy threshold should be assumed before collecting benchmark data.

### 19.4. Cross-Platform Testing

The shared core must be tested on all supported compiler and platform configurations.

Mobile testing must additionally cover:

- Low-memory devices.
- Background audio.
- Application suspension.
- Audio interruption.
- File access restrictions.
- Cancellation of processing tasks.

---

## 20\. Development Roadmap

The development process is divided into six phases.

### Phase 1 — Project Foundation

**Estimated duration: 1 week**

Tasks:

- Create the monorepository.
- Configure CMake.
- Integrate Qt 6.
- Configure dependency management.
- Integrate GoogleTest.
- Establish coding conventions.
- Implement initial domain models.
- Configure CI.

Deliverable:

A successfully building C++ project with automated tests.

### Phase 2 — Book and Audio Engines

**Estimated duration: 2 weeks**

Tasks:

- Implement EPUB parsing.
- Extract structured book text.
- Preserve chapter and paragraph boundaries.
- Implement audio playback.
- Integrate whisper.cpp.
- Generate timestamped transcripts.

Deliverable:

The application can load a book and generate an audiobook transcript locally.

### Phase 3 — Alignment Engine

**Estimated duration: 2 weeks**

Tasks:

- Implement text normalization.
- Implement tokenization.
- Build n-gram indexes.
- Implement fuzzy matching.
- Implement candidate matching.
- Implement sequence alignment.
- Generate Alignment Maps.
- Implement bidirectional navigation.

Deliverable:

Working audio-to-text and text-to-audio navigation.

### Phase 4 — Application and Storage

**Estimated duration: 1 week**

Tasks:

- Implement SQLite repositories.
- Persist book metadata.
- Persist transcripts.
- Persist Alignment Maps.
- Implement progress tracking.
- Implement ProcessingManager.
- Add error handling and recovery.

Deliverable:

Persistent application state and background processing.

### Phase 5 — Desktop MVP

**Estimated duration: 2 weeks**

Tasks:

- Implement the library interface.
- Implement the EPUB reader.
- Integrate the audio player.
- Implement text highlighting.
- Integrate navigation.
- Add settings.
- Test complete user workflows.

Deliverable:

A functional Desktop MVP.

### Phase 6 — Mobile Deployment

**Estimated duration: To be determined**

Tasks:

- Configure Android build.
- Configure iOS build.
- Implement platform-specific file access.
- Implement background audio.
- Validate Whisper model execution.
- Optimize memory usage.
- Test application lifecycle behavior.

Deliverable:

A mobile application using the shared C++ core.

These estimates are planning targets for a two-developer team, not guaranteed delivery
dates.

---

## 21\. Developer Responsibilities

The project is intended to be developed by two developers.

### Developer A — Core and Algorithms

Responsibilities:

- Domain Models.
- EPUB Parser.
- Text Normalization.
- Tokenization.
- Alignment Engine.
- Navigation Engine.
- Algorithmic Unit Tests.

### Developer B — Application and Platform Integration

Responsibilities:

- Qt / QML.
- Audio Playback.
- whisper.cpp Integration.
- SQLite Storage.
- Application Services.
- Processing Manager.
- Platform Integration.

### Shared Responsibilities

Both developers are responsible for:

- Architectural decisions.
- Public interfaces.
- Integration testing.
- Code reviews.
- Performance evaluation.
- Cross-platform compatibility.

Before implementation begins, both developers must agree on shared data structures and
public interfaces.

---

## 22\. Initial MVP Acceptance Criteria

The MVP is considered functionally complete when:

1. A user can import an EPUB book.
2. A user can associate an audiobook with the book.
3. The application can perform local speech recognition.
4. The application can generate an Alignment Map.
5. A user can navigate from audio to text.
6. A user can navigate from text to audio.
7. The reader can highlight the current text position.
8. Reading progress is saved.
9. The application restores the previous position after restarting.
10. Core functionality works without an internet connection.
11. Audio playback remains responsive during background processing.

---

## 23\. Architectural Decisions and Future Considerations

The following decisions are intentionally deferred until the MVP has been validated.

- Cloud synchronization between devices.
- User accounts and authentication.
- Additional book formats.
- Additional audiobook formats.
- Advanced word-level highlighting.
- Improved forced alignment algorithms.
- Custom speech recognition models.
- Multiple audiobook tracks per book.
- Library synchronization.
- Distributed processing.

These features must not complicate the initial implementation unless they become necessary
for the core user experience.

### Future Architectural Extensions

The modular architecture should allow future additions through independent interfaces and
adapters.

The Alignment Engine should remain replaceable without requiring changes to the UI or
playback subsystem.

The shared C++ core should remain reusable across desktop and mobile applications.

---

## 24\. Final Architecture Summary

AudioReader will be implemented as a cross-platform C++ application following a modular
architecture.

The principal architectural decisions are:

- C++20 as the primary programming language.
- Qt 6 / QML as the presentation framework.
- CMake as the build system.
- SQLite as the local database.
- whisper.cpp for offline speech recognition.
- FFmpeg for audio preprocessing.
- A dedicated Alignment Engine for synchronization.
- A dedicated Navigation Engine for bidirectional position conversion.
- Background workers for computationally intensive operations.
- Shared C++ Core across desktop and mobile platforms.

The most important technical component is the Alignment Engine, which establishes the
relationship between the original book text and the audiobook transcript.

All other subsystems should be designed around this core capability while remaining
independently testable and replaceable.

**Primary development objective: Deliver a reliable, fully offline Desktop MVP with
accurate bidirectional text and audio synchronization before expanding to mobile
platforms.**

