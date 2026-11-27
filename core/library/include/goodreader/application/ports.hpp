#pragma once

#include "goodreader/application/result.hpp"
#include "goodreader/domain/types.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace goodreader::application {

using Unit = std::monostate;

/** Persistence contract for locally stored books. */
class BookRepository {
  public:
    virtual ~BookRepository() = default;
    /** Finds a book by stable identifier. */
    virtual AppResult<std::optional<domain::Book>> find_by_id(const domain::BookId& id) = 0;
    /** Searches books using an optional trimmed, case-insensitive query. */
    virtual AppResult<std::vector<domain::Book>>
    search(const std::optional<std::string>& query) = 0;
    /** Inserts or updates a book. */
    virtual AppResult<Unit> save(const domain::Book& book) = 0;
};

/** Persistence contract for audiobook tracks. */
class AudioTrackRepository {
  public:
    virtual ~AudioTrackRepository() = default;
    /** Lists a book's tracks ordered by chapter index. */
    virtual AppResult<std::vector<domain::AudioTrack>>
    list_for_book(const domain::BookId& book_id) = 0;
    /** Inserts or updates an audio track. */
    virtual AppResult<Unit> save(const domain::AudioTrack& track) = 0;
};

/** Persistence contract for reading and listening positions. */
class ProgressRepository {
  public:
    virtual ~ProgressRepository() = default;
    /** Reads the last text position for a book. */
    virtual AppResult<std::optional<domain::ReadingProgress>>
    reading_progress(const domain::BookId& book_id) = 0;
    /** Saves the last text position for a book. */
    virtual AppResult<Unit> save_reading_progress(const domain::ReadingProgress& progress) = 0;
    /** Reads the last audio position for a book. */
    virtual AppResult<std::optional<domain::ListeningProgress>>
    listening_progress(const domain::BookId& book_id) = 0;
    /** Saves the last audio position for a book. */
    virtual AppResult<Unit> save_listening_progress(const domain::ListeningProgress& progress) = 0;
};

/** Persistence contract for reading activity used by streak calculations. */
class ReadingSessionRepository {
  public:
    virtual ~ReadingSessionRepository() = default;
    /** Lists recorded sessions for a book. */
    virtual AppResult<std::vector<domain::ReadingSession>>
    list_reading_sessions_for_book(const domain::BookId& book_id) = 0;
    /** Saves a completed reading session. */
    virtual AppResult<Unit> save(const domain::ReadingSession& session) = 0;
};

/** Asynchronous audio playback boundary implemented by each platform. */
class AudioPlayer {
  public:
    virtual ~AudioPlayer() = default;
    /** Starts playback at an audio-file URI. */
    virtual AppResult<Unit> play(const std::string& uri) = 0;
    /** Pauses current playback. */
    virtual AppResult<Unit> pause() = 0;
    /** Seeks to a non-negative audio position in milliseconds. */
    virtual AppResult<Unit> seek(std::int64_t position_millis) = 0;
    /** Returns the current playback position in milliseconds. */
    virtual AppResult<std::int64_t> position_millis() = 0;
};

/** Text and timing metadata returned by an ASR engine. */
struct RecognizedSpeech {
    std::string text;
    std::int64_t start_millis;
    std::int64_t end_millis;
    std::string engine_version;

    /** Creates recognized text with a valid audio interval and engine identifier. */
    RecognizedSpeech(std::string text, std::int64_t start_millis, std::int64_t end_millis,
                     std::string engine_version);
    bool operator==(const RecognizedSpeech&) const = default;
};

/** On-device speech recognition boundary; implementations must not upload user audio. */
class SpeechRecognizer {
  public:
    virtual ~SpeechRecognizer() = default;
    /** Transcribes a bounded fragment asynchronously using on-device recognition. */
    virtual AppResult<RecognizedSpeech> transcribe(const std::string& audio_uri,
                                                   std::int64_t start_millis,
                                                   std::int64_t duration_millis) = 0;
};

/** One user-selected audiobook file and its intended chapter order. */
struct AudioImportFile {
    std::string uri;
    std::string extension;
    std::int32_t chapter_index;
    bool operator==(const AudioImportFile&) const = default;
};

/** Files and metadata requested for a single book import. */
struct BookImportRequest {
    std::string book_uri;
    std::string book_extension;
    std::vector<AudioImportFile> audio_files;
    bool operator==(const BookImportRequest&) const = default;
};

/** Result of importing a book and its optional audio tracks. */
struct ImportedBook {
    domain::Book book;
    std::vector<domain::AudioTrack> audio_tracks;
    bool operator==(const ImportedBook&) const = default;
};

/** Atomic persistence boundary for an imported book and its complete audio pairing. */
class LibraryImportRepository {
  public:
    virtual ~LibraryImportRepository() = default;
    /** Replaces a book and all its tracks in one transaction. */
    virtual AppResult<Unit>
    save_imported_book(const domain::Book& book,
                       const std::vector<domain::AudioTrack>& audio_tracks) = 0;
};

/** Provides stable-unique identifiers without introducing process-global state. */
class IdGenerator {
  public:
    virtual ~IdGenerator() = default;
    /** Returns a new opaque identifier. */
    virtual std::string next_id() = 0;
};

/** Import boundary for user-selected book and audio files. */
class BookImporter {
  public:
    virtual ~BookImporter() = default;
    /** Imports only the files supplied in the request; it must not fetch remote content. */
    virtual AppResult<ImportedBook> import(const BookImportRequest& request) = 0;
};

/** Conversion progress reported by a background converter. */
struct ConversionProgress {
    double completed;

    /** Creates a progress value in the inclusive range [0, 1]. */
    explicit ConversionProgress(double completed);
    bool operator==(const ConversionProgress&) const = default;
};

/** Supported output formats for PDF conversion. */
enum class ConversionFormat { epub, fb2 };

/** Background conversion boundary for supported book formats. */
class BookConverter {
  public:
    virtual ~BookConverter() = default;
    /** Converts a PDF to the requested format and reports background progress. */
    virtual AppResult<ConversionProgress> convert(const std::string& source_uri,
                                                  ConversionFormat output_format) = 0;
};

/** Persistence contract for cached text/audio alignment anchors. */
class AlignmentIndexRepository {
  public:
    virtual ~AlignmentIndexRepository() = default;
    /** Loads a compatible cached index or returns no value when one is unavailable. */
    virtual AppResult<std::optional<domain::AlignmentIndex>>
    find(const domain::BookId& book_id, const std::string& audio_fingerprint) = 0;
    /** Stores an index for a particular book/audio pair. */
    virtual AppResult<Unit> save(const domain::BookId& book_id,
                                 const std::string& audio_fingerprint,
                                 const domain::AlignmentIndex& index) = 0;
};

/** User identity exposed without authentication UI assumptions. */
struct LocalUser {
    std::string id;
    bool is_local_only;

    /** Creates an identity with a non-blank stable identifier. */
    LocalUser(std::string id, bool is_local_only);
    bool operator==(const LocalUser&) const = default;
};

/** Authentication extension point; the MVP implementation returns a local-only user. */
class AuthService {
  public:
    virtual ~AuthService() = default;
    /** Returns the current local or authenticated user. */
    virtual AppResult<LocalUser> current_user() = 0;
};

/** Feature identifiers used by the entitlement boundary. */
enum class Feature { book_import, audio_sync, pdf_conversion };

/** Subscription and feature-flag extension point. */
class EntitlementService {
  public:
    virtual ~EntitlementService() = default;
    /** Indicates whether the current user can use a feature. */
    virtual AppResult<bool> is_enabled(Feature feature) = 0;
};

} // namespace goodreader::application
