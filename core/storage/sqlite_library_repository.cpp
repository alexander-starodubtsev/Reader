#include "goodreader/infrastructure/sqlite_library_repository.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <cctype>
#include <limits>
#include <set>
#include <utility>

namespace goodreader::infrastructure {
namespace {

using application::AppError;
using application::AppResult;
using application::Unit;
using domain::AlignmentAnchor;
using domain::AlignmentIndex;
using domain::AudioFormat;
using domain::AudioTrack;
using domain::AudioTrackId;
using domain::Book;
using domain::BookFormat;
using domain::BookId;
using domain::ListeningProgress;
using domain::ReadingProgress;
using domain::ReadingSession;
using domain::Timestamp;

template <typename T> [[nodiscard]] AppResult<T> database_failure(const std::string& operation) {
    return AppResult<T>::failure(
        AppError{application::UnavailableError{"local_database", operation}});
}

template <typename T> [[nodiscard]] AppResult<T> invalid_record(const std::string& operation) {
    return AppResult<T>::failure(AppError{application::UnexpectedError{operation}});
}

[[nodiscard]] std::string book_format_name(BookFormat format) {
    switch (format) {
    case BookFormat::pdf:
        return "PDF";
    case BookFormat::epub:
        return "EPUB";
    case BookFormat::fb2:
        return "FB2";
    }
    return {};
}

[[nodiscard]] std::optional<BookFormat> parse_book_format(const std::string& value) {
    if (value == "PDF") {
        return BookFormat::pdf;
    }
    if (value == "EPUB") {
        return BookFormat::epub;
    }
    if (value == "FB2") {
        return BookFormat::fb2;
    }
    return std::nullopt;
}

[[nodiscard]] std::string audio_format_name(AudioFormat format) {
    switch (format) {
    case AudioFormat::mp3:
        return "MP3";
    case AudioFormat::m4a:
        return "M4A";
    case AudioFormat::m4b:
        return "M4B";
    case AudioFormat::ogg:
        return "OGG";
    case AudioFormat::wav:
        return "WAV";
    }
    return {};
}

[[nodiscard]] std::optional<AudioFormat> parse_audio_format(const std::string& value) {
    if (value == "MP3") {
        return AudioFormat::mp3;
    }
    if (value == "M4A") {
        return AudioFormat::m4a;
    }
    if (value == "M4B") {
        return AudioFormat::m4b;
    }
    if (value == "OGG") {
        return AudioFormat::ogg;
    }
    if (value == "WAV") {
        return AudioFormat::wav;
    }
    return std::nullopt;
}

[[nodiscard]] bool execute(sqlite3* database, const char* sql) {
    char* error_message = nullptr;
    const auto status = sqlite3_exec(database, sql, nullptr, nullptr, &error_message);
    sqlite3_free(error_message);
    return status == SQLITE_OK;
}

class Statement {
  public:
    Statement(sqlite3* database, const char* sql) {
        if (sqlite3_prepare_v2(database, sql, -1, &statement_, nullptr) != SQLITE_OK) {
            statement_ = nullptr;
        }
    }

    ~Statement() { sqlite3_finalize(statement_); }

    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    [[nodiscard]] sqlite3_stmt* get() const { return statement_; }

    [[nodiscard]] bool valid() const { return statement_ != nullptr; }

  private:
    sqlite3_stmt* statement_ = nullptr;
};

[[nodiscard]] bool bind_text(sqlite3_stmt* statement, int index, const std::string& value) {
    if (value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return false;
    }
    return sqlite3_bind_text(statement, index, value.data(), static_cast<int>(value.size()),
                             SQLITE_TRANSIENT) == SQLITE_OK;
}

[[nodiscard]] bool bind_optional_text(sqlite3_stmt* statement, int index,
                                      const std::optional<std::string>& value) {
    return value.has_value() ? bind_text(statement, index, *value)
                             : sqlite3_bind_null(statement, index) == SQLITE_OK;
}

[[nodiscard]] std::string column_text(sqlite3_stmt* statement, int column) {
    const auto* value = sqlite3_column_text(statement, column);
    if (value == nullptr) {
        return {};
    }
    return reinterpret_cast<const char*>(value);
}

[[nodiscard]] std::optional<std::string> column_optional_text(sqlite3_stmt* statement, int column) {
    if (sqlite3_column_type(statement, column) == SQLITE_NULL) {
        return std::nullopt;
    }
    return column_text(statement, column);
}

[[nodiscard]] bool is_blank(const std::string& value) {
    return std::all_of(value.begin(), value.end(),
                       [](unsigned char character) { return std::isspace(character) != 0; });
}

[[nodiscard]] std::optional<Book> read_book(sqlite3_stmt* statement) {
    const auto format = parse_book_format(column_text(statement, 3));
    if (!format.has_value()) {
        return std::nullopt;
    }
    std::optional<std::int32_t> word_count;
    if (sqlite3_column_type(statement, 5) != SQLITE_NULL) {
        const auto value = sqlite3_column_int64(statement, 5);
        if (value < 0 || value > std::numeric_limits<std::int32_t>::max()) {
            return std::nullopt;
        }
        word_count = static_cast<std::int32_t>(value);
    }
    try {
        return Book(BookId(column_text(statement, 0)), column_text(statement, 1),
                    column_optional_text(statement, 2), *format, column_text(statement, 4),
                    word_count);
    } catch (const std::invalid_argument&) {
        return std::nullopt;
    }
}

[[nodiscard]] std::optional<AudioTrack> read_audio_track(sqlite3_stmt* statement) {
    const auto format = parse_audio_format(column_text(statement, 3));
    const auto chapter_index = sqlite3_column_int64(statement, 5);
    if (!format.has_value() || chapter_index < 0 ||
        chapter_index > std::numeric_limits<std::int32_t>::max()) {
        return std::nullopt;
    }
    try {
        return AudioTrack(AudioTrackId(column_text(statement, 0)),
                          BookId(column_text(statement, 1)), column_text(statement, 2), *format,
                          sqlite3_column_int64(statement, 4),
                          static_cast<std::int32_t>(chapter_index));
    } catch (const std::invalid_argument&) {
        return std::nullopt;
    }
}

[[nodiscard]] std::int64_t timestamp_millis(Timestamp timestamp) {
    return timestamp.time_since_epoch().count();
}

[[nodiscard]] Timestamp timestamp_from_millis(std::int64_t value) {
    return Timestamp{domain::Milliseconds{value}};
}

class Transaction {
  public:
    explicit Transaction(sqlite3* database)
        : database_(database), active_(execute(database, "BEGIN")) {}

    ~Transaction() {
        if (active_) {
            static_cast<void>(execute(database_, "ROLLBACK"));
        }
    }

    [[nodiscard]] bool active() const { return active_; }

    [[nodiscard]] bool commit() {
        if (!active_ || !execute(database_, "COMMIT")) {
            return false;
        }
        active_ = false;
        return true;
    }

  private:
    sqlite3* database_;
    bool active_;
};

[[nodiscard]] bool bind_book(sqlite3_stmt* statement, const Book& book) {
    const bool success = bind_text(statement, 1, book.id.value) &&
                         bind_text(statement, 2, book.title) &&
                         bind_optional_text(statement, 3, book.author) &&
                         bind_text(statement, 4, book_format_name(book.format)) &&
                         bind_text(statement, 5, book.file_uri);
    if (!success) {
        return false;
    }
    if (book.word_count.has_value()) {
        return sqlite3_bind_int64(statement, 6, *book.word_count) == SQLITE_OK;
    }
    return sqlite3_bind_null(statement, 6) == SQLITE_OK;
}

[[nodiscard]] bool save_book(sqlite3* database, const Book& book) {
    Statement statement(
        database,
        "INSERT INTO Book(id, title, author, format, file_uri, word_count) "
        "VALUES (?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(id) DO UPDATE SET title=excluded.title, author=excluded.author, "
        "format=excluded.format, file_uri=excluded.file_uri, word_count=excluded.word_count");
    return statement.valid() && bind_book(statement.get(), book) &&
           sqlite3_step(statement.get()) == SQLITE_DONE;
}

[[nodiscard]] bool save_audio_track(sqlite3* database, const AudioTrack& track,
                                    const std::string& book_id) {
    Statement statement(
        database,
        "INSERT OR REPLACE INTO AudioTrack"
        "(id, book_id, uri, format, duration_millis, chapter_index) VALUES (?, ?, ?, ?, ?, ?)");
    return statement.valid() && bind_text(statement.get(), 1, track.id.value) &&
           bind_text(statement.get(), 2, book_id) && bind_text(statement.get(), 3, track.uri) &&
           bind_text(statement.get(), 4, audio_format_name(track.format)) &&
           sqlite3_bind_int64(statement.get(), 5, track.duration_millis) == SQLITE_OK &&
           sqlite3_bind_int64(statement.get(), 6, track.chapter_index) == SQLITE_OK &&
           sqlite3_step(statement.get()) == SQLITE_DONE;
}

[[nodiscard]] std::optional<ReadingProgress> read_reading_progress(sqlite3_stmt* statement) {
    try {
        return ReadingProgress(BookId(column_text(statement, 0)),
                               sqlite3_column_int64(statement, 1),
                               timestamp_from_millis(sqlite3_column_int64(statement, 2)));
    } catch (const std::invalid_argument&) {
        return std::nullopt;
    }
}

[[nodiscard]] std::optional<ListeningProgress> read_listening_progress(sqlite3_stmt* statement) {
    try {
        return ListeningProgress(BookId(column_text(statement, 0)),
                                 AudioTrackId(column_text(statement, 1)),
                                 sqlite3_column_int64(statement, 2),
                                 timestamp_from_millis(sqlite3_column_int64(statement, 3)));
    } catch (const std::invalid_argument&) {
        return std::nullopt;
    }
}

} // namespace

SqliteLibraryRepository::SqliteLibraryRepository(sqlite3* database) : database_(database) {}

SqliteLibraryRepository::~SqliteLibraryRepository() { sqlite3_close_v2(database_); }

AppResult<std::unique_ptr<SqliteLibraryRepository>>
SqliteLibraryRepository::open(const std::string& database_path) {
    if (database_path.empty()) {
        return AppResult<std::unique_ptr<SqliteLibraryRepository>>::failure(AppError{
            application::ValidationError{"database_path_required", "Choose a database path."}});
    }
    sqlite3* database = nullptr;
    const auto open_status = sqlite3_open_v2(
        database_path.c_str(), &database,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr);
    if (open_status != SQLITE_OK) {
        sqlite3_close_v2(database);
        return database_failure<std::unique_ptr<SqliteLibraryRepository>>("open_database");
    }
    sqlite3_extended_result_codes(database, 1);
    sqlite3_busy_timeout(database, 5000);

    auto repository =
        std::unique_ptr<SqliteLibraryRepository>(new SqliteLibraryRepository(database));
    const auto migration = repository->migrate();
    if (!migration.has_value()) {
        return AppResult<std::unique_ptr<SqliteLibraryRepository>>::failure(migration.error());
    }
    return AppResult<std::unique_ptr<SqliteLibraryRepository>>::success(std::move(repository));
}

AppResult<Unit> SqliteLibraryRepository::migrate() {
    const std::lock_guard lock(mutex_);
    if (!execute(database_, "PRAGMA foreign_keys = ON")) {
        return database_failure<Unit>("enable_foreign_keys");
    }

    int version = 0;
    {
        Statement version_statement(database_, "PRAGMA user_version");
        if (!version_statement.valid() || sqlite3_step(version_statement.get()) != SQLITE_ROW) {
            return database_failure<Unit>("read_schema_version");
        }
        version = sqlite3_column_int(version_statement.get(), 0);
    }
    if (version > current_schema_version) {
        return AppResult<Unit>::failure(AppError{application::ValidationError{
            "database_version_newer", "The database was created by a newer GoodReader version."}});
    }

    if (version == 0) {
        Transaction transaction(database_);
        if (!transaction.active() ||
            !execute(database_, "CREATE TABLE Book("
                                "id TEXT NOT NULL PRIMARY KEY,"
                                "title TEXT NOT NULL,"
                                "author TEXT,"
                                "format TEXT NOT NULL,"
                                "word_count INTEGER)") ||
            !execute(database_, "PRAGMA user_version = 1") || !transaction.commit()) {
            return database_failure<Unit>("create_initial_schema");
        }
        version = 1;
    }

    if (version == 1) {
        Transaction transaction(database_);
        if (!transaction.active() ||
            !execute(database_, "ALTER TABLE Book ADD COLUMN file_uri TEXT NOT NULL DEFAULT "
                                "'legacy://unavailable'") ||
            !execute(database_, "CREATE TABLE AudioTrack("
                                "id TEXT NOT NULL PRIMARY KEY,"
                                "book_id TEXT NOT NULL REFERENCES Book(id) ON DELETE CASCADE,"
                                "uri TEXT NOT NULL,"
                                "format TEXT NOT NULL,"
                                "duration_millis INTEGER NOT NULL CHECK(duration_millis > 0),"
                                "chapter_index INTEGER NOT NULL CHECK(chapter_index >= 0),"
                                "UNIQUE(book_id, chapter_index))") ||
            !execute(database_, "CREATE INDEX book_title_idx ON Book(title COLLATE NOCASE)") ||
            !execute(database_, "CREATE INDEX book_author_idx ON Book(author COLLATE NOCASE)") ||
            !execute(database_,
                     "CREATE INDEX audio_track_book_idx ON AudioTrack(book_id, chapter_index)") ||
            !execute(database_, "PRAGMA user_version = 2") || !transaction.commit()) {
            return database_failure<Unit>("migrate_schema_1_to_2");
        }
        version = 2;
    }

    if (version == 2) {
        Transaction transaction(database_);
        if (!transaction.active() ||
            !execute(database_,
                     "CREATE TABLE ReadingProgress("
                     "book_id TEXT NOT NULL PRIMARY KEY REFERENCES Book(id) ON DELETE CASCADE,"
                     "character_offset INTEGER NOT NULL CHECK(character_offset >= 0),"
                     "updated_at_millis INTEGER NOT NULL)") ||
            !execute(database_,
                     "CREATE TABLE ListeningProgress("
                     "book_id TEXT NOT NULL PRIMARY KEY REFERENCES Book(id) ON DELETE CASCADE,"
                     "track_id TEXT NOT NULL REFERENCES AudioTrack(id) ON DELETE CASCADE,"
                     "position_millis INTEGER NOT NULL CHECK(position_millis >= 0),"
                     "updated_at_millis INTEGER NOT NULL)") ||
            !execute(database_, "CREATE TABLE ReadingSession("
                                "id TEXT NOT NULL PRIMARY KEY,"
                                "book_id TEXT NOT NULL REFERENCES Book(id) ON DELETE CASCADE,"
                                "time_zone_id TEXT NOT NULL)") ||
            !execute(database_,
                     "CREATE TABLE ActiveReadingInterval("
                     "session_id TEXT NOT NULL REFERENCES ReadingSession(id) ON DELETE CASCADE,"
                     "ordinal INTEGER NOT NULL CHECK(ordinal >= 0),"
                     "started_at_millis INTEGER NOT NULL,"
                     "ended_at_millis INTEGER NOT NULL CHECK(ended_at_millis > started_at_millis),"
                     "PRIMARY KEY(session_id, ordinal))") ||
            !execute(database_, "CREATE TABLE AlignmentIndex("
                                "book_id TEXT NOT NULL REFERENCES Book(id) ON DELETE CASCADE,"
                                "audio_fingerprint TEXT NOT NULL,"
                                "PRIMARY KEY(book_id, audio_fingerprint))") ||
            !execute(database_,
                     "CREATE TABLE AlignmentAnchor("
                     "book_id TEXT NOT NULL,"
                     "audio_fingerprint TEXT NOT NULL,"
                     "ordinal INTEGER NOT NULL CHECK(ordinal >= 0),"
                     "word_offset INTEGER NOT NULL CHECK(word_offset >= 0),"
                     "audio_time_millis INTEGER NOT NULL CHECK(audio_time_millis >= 0),"
                     "confidence REAL NOT NULL CHECK(confidence >= 0 AND confidence <= 1),"
                     "PRIMARY KEY(book_id, audio_fingerprint, ordinal),"
                     "UNIQUE(book_id, audio_fingerprint, word_offset),"
                     "UNIQUE(book_id, audio_fingerprint, audio_time_millis),"
                     "FOREIGN KEY(book_id, audio_fingerprint) "
                     "REFERENCES AlignmentIndex(book_id, audio_fingerprint) ON DELETE CASCADE)") ||
            !execute(database_, "PRAGMA user_version = 3") || !transaction.commit()) {
            return database_failure<Unit>("migrate_schema_2_to_3");
        }
    }
    return AppResult<Unit>::success(Unit{});
}

AppResult<std::optional<Book>> SqliteLibraryRepository::find_by_id(const BookId& id) {
    const std::lock_guard lock(mutex_);
    Statement statement(
        database_, "SELECT id, title, author, format, file_uri, word_count FROM Book WHERE id = ?");
    if (!statement.valid() || !bind_text(statement.get(), 1, id.value)) {
        return database_failure<std::optional<Book>>("find_book");
    }
    const auto status = sqlite3_step(statement.get());
    if (status == SQLITE_DONE) {
        return AppResult<std::optional<Book>>::success(std::nullopt);
    }
    if (status != SQLITE_ROW) {
        return database_failure<std::optional<Book>>("find_book");
    }
    auto book = read_book(statement.get());
    if (!book.has_value()) {
        return invalid_record<std::optional<Book>>("find_book");
    }
    return AppResult<std::optional<Book>>::success(std::move(book));
}

AppResult<std::vector<Book>>
SqliteLibraryRepository::search(const std::optional<std::string>& query) {
    const std::lock_guard lock(mutex_);
    const bool search_query = query.has_value() && !is_blank(*query);
    Statement statement(
        database_,
        search_query
            ? "SELECT id, title, author, format, file_uri, word_count FROM Book "
              "WHERE title LIKE ? COLLATE NOCASE OR COALESCE(author, '') LIKE ? COLLATE NOCASE "
              "ORDER BY title COLLATE NOCASE"
            : "SELECT id, title, author, format, file_uri, word_count FROM Book "
              "ORDER BY title COLLATE NOCASE");
    if (!statement.valid()) {
        return database_failure<std::vector<Book>>("search_books");
    }
    if (search_query) {
        const auto pattern = "%" + *query + "%";
        if (!bind_text(statement.get(), 1, pattern) || !bind_text(statement.get(), 2, pattern)) {
            return database_failure<std::vector<Book>>("search_books");
        }
    }

    std::vector<Book> books;
    while (true) {
        const auto status = sqlite3_step(statement.get());
        if (status == SQLITE_DONE) {
            break;
        }
        if (status != SQLITE_ROW) {
            return database_failure<std::vector<Book>>("search_books");
        }
        auto book = read_book(statement.get());
        if (!book.has_value()) {
            return invalid_record<std::vector<Book>>("search_books");
        }
        books.push_back(std::move(*book));
    }
    return AppResult<std::vector<Book>>::success(std::move(books));
}

AppResult<Unit> SqliteLibraryRepository::save(const Book& book) {
    const std::lock_guard lock(mutex_);
    if (!save_book(database_, book)) {
        return database_failure<Unit>("save_book");
    }
    return AppResult<Unit>::success(Unit{});
}

AppResult<std::vector<AudioTrack>> SqliteLibraryRepository::list_for_book(const BookId& book_id) {
    const std::lock_guard lock(mutex_);
    Statement statement(database_,
                        "SELECT id, book_id, uri, format, duration_millis, chapter_index "
                        "FROM AudioTrack WHERE book_id = ? ORDER BY chapter_index");
    if (!statement.valid() || !bind_text(statement.get(), 1, book_id.value)) {
        return database_failure<std::vector<AudioTrack>>("list_audio_tracks");
    }

    std::vector<AudioTrack> tracks;
    while (true) {
        const auto status = sqlite3_step(statement.get());
        if (status == SQLITE_DONE) {
            break;
        }
        if (status != SQLITE_ROW) {
            return database_failure<std::vector<AudioTrack>>("list_audio_tracks");
        }
        auto track = read_audio_track(statement.get());
        if (!track.has_value()) {
            return invalid_record<std::vector<AudioTrack>>("list_audio_tracks");
        }
        tracks.push_back(std::move(*track));
    }
    return AppResult<std::vector<AudioTrack>>::success(std::move(tracks));
}

AppResult<Unit> SqliteLibraryRepository::save(const AudioTrack& track) {
    const std::lock_guard lock(mutex_);
    if (!save_audio_track(database_, track, track.book_id.value)) {
        return database_failure<Unit>("save_audio_track");
    }
    return AppResult<Unit>::success(Unit{});
}

AppResult<Unit>
SqliteLibraryRepository::save_imported_book(const Book& book,
                                            const std::vector<AudioTrack>& audio_tracks) {
    std::set<std::string> ids;
    std::set<std::int32_t> chapters;
    for (const auto& track : audio_tracks) {
        if (track.book_id != book.id || !ids.insert(track.id.value).second ||
            !chapters.insert(track.chapter_index).second) {
            return AppResult<Unit>::failure(AppError{application::ValidationError{
                "invalid_audio_pairing",
                "Audio tracks must belong to this book and have unique IDs and chapters."}});
        }
    }

    const std::lock_guard lock(mutex_);
    Transaction transaction(database_);
    if (!transaction.active() || !save_book(database_, book)) {
        return database_failure<Unit>("save_imported_book");
    }

    Statement delete_tracks(database_, "DELETE FROM AudioTrack WHERE book_id = ?");
    if (!delete_tracks.valid() || !bind_text(delete_tracks.get(), 1, book.id.value) ||
        sqlite3_step(delete_tracks.get()) != SQLITE_DONE) {
        return database_failure<Unit>("save_imported_book");
    }
    for (const auto& track : audio_tracks) {
        if (!save_audio_track(database_, track, book.id.value)) {
            return database_failure<Unit>("save_imported_book");
        }
    }
    if (!transaction.commit()) {
        return database_failure<Unit>("save_imported_book");
    }
    return AppResult<Unit>::success(Unit{});
}

AppResult<std::optional<ReadingProgress>>
SqliteLibraryRepository::reading_progress(const BookId& book_id) {
    const std::lock_guard lock(mutex_);
    Statement statement(database_, "SELECT book_id, character_offset, updated_at_millis "
                                   "FROM ReadingProgress WHERE book_id = ?");
    if (!statement.valid() || !bind_text(statement.get(), 1, book_id.value)) {
        return database_failure<std::optional<ReadingProgress>>("read_reading_progress");
    }
    const auto status = sqlite3_step(statement.get());
    if (status == SQLITE_DONE) {
        return AppResult<std::optional<ReadingProgress>>::success(std::nullopt);
    }
    if (status != SQLITE_ROW) {
        return database_failure<std::optional<ReadingProgress>>("read_reading_progress");
    }
    auto progress = read_reading_progress(statement.get());
    if (!progress.has_value()) {
        return invalid_record<std::optional<ReadingProgress>>("read_reading_progress");
    }
    return AppResult<std::optional<ReadingProgress>>::success(std::move(progress));
}

AppResult<Unit> SqliteLibraryRepository::save_reading_progress(const ReadingProgress& progress) {
    const std::lock_guard lock(mutex_);
    Statement statement(
        database_,
        "INSERT INTO ReadingProgress(book_id, character_offset, updated_at_millis) "
        "VALUES (?, ?, ?) ON CONFLICT(book_id) DO UPDATE SET "
        "character_offset=excluded.character_offset, updated_at_millis=excluded.updated_at_millis");
    if (!statement.valid() || !bind_text(statement.get(), 1, progress.book_id.value) ||
        sqlite3_bind_int64(statement.get(), 2, progress.character_offset) != SQLITE_OK ||
        sqlite3_bind_int64(statement.get(), 3, timestamp_millis(progress.updated_at)) !=
            SQLITE_OK ||
        sqlite3_step(statement.get()) != SQLITE_DONE) {
        return database_failure<Unit>("save_reading_progress");
    }
    return AppResult<Unit>::success(Unit{});
}

AppResult<std::optional<ListeningProgress>>
SqliteLibraryRepository::listening_progress(const BookId& book_id) {
    const std::lock_guard lock(mutex_);
    Statement statement(database_, "SELECT book_id, track_id, position_millis, updated_at_millis "
                                   "FROM ListeningProgress WHERE book_id = ?");
    if (!statement.valid() || !bind_text(statement.get(), 1, book_id.value)) {
        return database_failure<std::optional<ListeningProgress>>("read_listening_progress");
    }
    const auto status = sqlite3_step(statement.get());
    if (status == SQLITE_DONE) {
        return AppResult<std::optional<ListeningProgress>>::success(std::nullopt);
    }
    if (status != SQLITE_ROW) {
        return database_failure<std::optional<ListeningProgress>>("read_listening_progress");
    }
    auto progress = read_listening_progress(statement.get());
    if (!progress.has_value()) {
        return invalid_record<std::optional<ListeningProgress>>("read_listening_progress");
    }
    return AppResult<std::optional<ListeningProgress>>::success(std::move(progress));
}

AppResult<Unit>
SqliteLibraryRepository::save_listening_progress(const ListeningProgress& progress) {
    const std::lock_guard lock(mutex_);
    Statement statement(
        database_,
        "INSERT INTO ListeningProgress(book_id, track_id, position_millis, updated_at_millis) "
        "VALUES (?, ?, ?, ?) ON CONFLICT(book_id) DO UPDATE SET "
        "track_id=excluded.track_id, position_millis=excluded.position_millis, "
        "updated_at_millis=excluded.updated_at_millis");
    if (!statement.valid() || !bind_text(statement.get(), 1, progress.book_id.value) ||
        !bind_text(statement.get(), 2, progress.track_id.value) ||
        sqlite3_bind_int64(statement.get(), 3, progress.position_millis) != SQLITE_OK ||
        sqlite3_bind_int64(statement.get(), 4, timestamp_millis(progress.updated_at)) !=
            SQLITE_OK ||
        sqlite3_step(statement.get()) != SQLITE_DONE) {
        return database_failure<Unit>("save_listening_progress");
    }
    return AppResult<Unit>::success(Unit{});
}

AppResult<std::vector<ReadingSession>>
SqliteLibraryRepository::list_reading_sessions_for_book(const BookId& book_id) {
    const std::lock_guard lock(mutex_);
    Statement sessions(
        database_,
        "SELECT id, book_id, time_zone_id FROM ReadingSession WHERE book_id = ? ORDER BY id");
    if (!sessions.valid() || !bind_text(sessions.get(), 1, book_id.value)) {
        return database_failure<std::vector<ReadingSession>>("list_reading_sessions");
    }

    std::vector<ReadingSession> result;
    while (true) {
        const auto session_status = sqlite3_step(sessions.get());
        if (session_status == SQLITE_DONE) {
            break;
        }
        if (session_status != SQLITE_ROW) {
            return database_failure<std::vector<ReadingSession>>("list_reading_sessions");
        }
        const auto session_id = column_text(sessions.get(), 0);
        const auto session_book_id = column_text(sessions.get(), 1);
        const auto time_zone_id = column_text(sessions.get(), 2);
        Statement intervals(database_,
                            "SELECT started_at_millis, ended_at_millis FROM ActiveReadingInterval "
                            "WHERE session_id = ? ORDER BY ordinal");
        if (!intervals.valid() || !bind_text(intervals.get(), 1, session_id)) {
            return database_failure<std::vector<ReadingSession>>("list_reading_intervals");
        }
        std::vector<domain::ActiveReadingInterval> active_intervals;
        while (true) {
            const auto interval_status = sqlite3_step(intervals.get());
            if (interval_status == SQLITE_DONE) {
                break;
            }
            if (interval_status != SQLITE_ROW) {
                return database_failure<std::vector<ReadingSession>>("list_reading_intervals");
            }
            try {
                active_intervals.emplace_back(
                    timestamp_from_millis(sqlite3_column_int64(intervals.get(), 0)),
                    timestamp_from_millis(sqlite3_column_int64(intervals.get(), 1)));
            } catch (const std::invalid_argument&) {
                return invalid_record<std::vector<ReadingSession>>("list_reading_intervals");
            }
        }
        try {
            result.emplace_back(session_id, BookId(session_book_id), time_zone_id,
                                std::move(active_intervals));
        } catch (const std::invalid_argument&) {
            return invalid_record<std::vector<ReadingSession>>("list_reading_sessions");
        }
    }
    return AppResult<std::vector<ReadingSession>>::success(std::move(result));
}

AppResult<Unit> SqliteLibraryRepository::save(const ReadingSession& session) {
    const std::lock_guard lock(mutex_);
    Transaction transaction(database_);
    if (!transaction.active()) {
        return database_failure<Unit>("save_reading_session");
    }
    Statement save_session(database_,
                           "INSERT INTO ReadingSession(id, book_id, time_zone_id) VALUES (?, ?, ?) "
                           "ON CONFLICT(id) DO UPDATE SET book_id=excluded.book_id, "
                           "time_zone_id=excluded.time_zone_id");
    if (!save_session.valid() || !bind_text(save_session.get(), 1, session.id) ||
        !bind_text(save_session.get(), 2, session.book_id.value) ||
        !bind_text(save_session.get(), 3, session.time_zone_id) ||
        sqlite3_step(save_session.get()) != SQLITE_DONE) {
        return database_failure<Unit>("save_reading_session");
    }

    Statement delete_intervals(database_, "DELETE FROM ActiveReadingInterval WHERE session_id = ?");
    if (!delete_intervals.valid() || !bind_text(delete_intervals.get(), 1, session.id) ||
        sqlite3_step(delete_intervals.get()) != SQLITE_DONE) {
        return database_failure<Unit>("save_reading_session");
    }
    for (std::size_t ordinal = 0; ordinal < session.active_intervals.size(); ++ordinal) {
        Statement interval(
            database_,
            "INSERT INTO ActiveReadingInterval"
            "(session_id, ordinal, started_at_millis, ended_at_millis) VALUES (?, ?, ?, ?)");
        if (!interval.valid() || !bind_text(interval.get(), 1, session.id) ||
            sqlite3_bind_int64(interval.get(), 2, static_cast<std::int64_t>(ordinal)) !=
                SQLITE_OK ||
            sqlite3_bind_int64(interval.get(), 3,
                               timestamp_millis(session.active_intervals[ordinal].started_at)) !=
                SQLITE_OK ||
            sqlite3_bind_int64(interval.get(), 4,
                               timestamp_millis(session.active_intervals[ordinal].ended_at)) !=
                SQLITE_OK ||
            sqlite3_step(interval.get()) != SQLITE_DONE) {
            return database_failure<Unit>("save_reading_session");
        }
    }
    if (!transaction.commit()) {
        return database_failure<Unit>("save_reading_session");
    }
    return AppResult<Unit>::success(Unit{});
}

AppResult<std::optional<AlignmentIndex>>
SqliteLibraryRepository::find(const BookId& book_id, const std::string& audio_fingerprint) {
    const std::lock_guard lock(mutex_);
    Statement index(database_,
                    "SELECT 1 FROM AlignmentIndex WHERE book_id = ? AND audio_fingerprint = ?");
    if (!index.valid() || !bind_text(index.get(), 1, book_id.value) ||
        !bind_text(index.get(), 2, audio_fingerprint)) {
        return database_failure<std::optional<AlignmentIndex>>("find_alignment_index");
    }
    const auto index_status = sqlite3_step(index.get());
    if (index_status == SQLITE_DONE) {
        return AppResult<std::optional<AlignmentIndex>>::success(std::nullopt);
    }
    if (index_status != SQLITE_ROW) {
        return database_failure<std::optional<AlignmentIndex>>("find_alignment_index");
    }

    Statement anchors(database_,
                      "SELECT word_offset, audio_time_millis, confidence FROM AlignmentAnchor "
                      "WHERE book_id = ? AND audio_fingerprint = ? ORDER BY ordinal");
    if (!anchors.valid() || !bind_text(anchors.get(), 1, book_id.value) ||
        !bind_text(anchors.get(), 2, audio_fingerprint)) {
        return database_failure<std::optional<AlignmentIndex>>("read_alignment_anchors");
    }
    std::vector<AlignmentAnchor> values;
    while (true) {
        const auto status = sqlite3_step(anchors.get());
        if (status == SQLITE_DONE) {
            break;
        }
        if (status != SQLITE_ROW) {
            return database_failure<std::optional<AlignmentIndex>>("read_alignment_anchors");
        }
        const auto word_offset = sqlite3_column_int64(anchors.get(), 0);
        if (word_offset < 0 || word_offset > std::numeric_limits<std::int32_t>::max()) {
            return invalid_record<std::optional<AlignmentIndex>>("read_alignment_anchors");
        }
        try {
            values.emplace_back(domain::WordOffset(static_cast<std::int32_t>(word_offset)),
                                sqlite3_column_int64(anchors.get(), 1),
                                sqlite3_column_double(anchors.get(), 2));
        } catch (const std::invalid_argument&) {
            return invalid_record<std::optional<AlignmentIndex>>("read_alignment_anchors");
        }
    }
    try {
        return AppResult<std::optional<AlignmentIndex>>::success(AlignmentIndex(std::move(values)));
    } catch (const std::invalid_argument&) {
        return invalid_record<std::optional<AlignmentIndex>>("read_alignment_anchors");
    }
}

AppResult<Unit> SqliteLibraryRepository::save(const BookId& book_id,
                                              const std::string& audio_fingerprint,
                                              const AlignmentIndex& index) {
    const std::lock_guard lock(mutex_);
    Transaction transaction(database_);
    if (!transaction.active()) {
        return database_failure<Unit>("save_alignment_index");
    }

    Statement save_index(database_,
                         "INSERT INTO AlignmentIndex(book_id, audio_fingerprint) VALUES (?, ?) "
                         "ON CONFLICT(book_id, audio_fingerprint) DO NOTHING");
    if (!save_index.valid() || !bind_text(save_index.get(), 1, book_id.value) ||
        !bind_text(save_index.get(), 2, audio_fingerprint) ||
        sqlite3_step(save_index.get()) != SQLITE_DONE) {
        return database_failure<Unit>("save_alignment_index");
    }
    Statement delete_anchors(
        database_, "DELETE FROM AlignmentAnchor WHERE book_id = ? AND audio_fingerprint = ?");
    if (!delete_anchors.valid() || !bind_text(delete_anchors.get(), 1, book_id.value) ||
        !bind_text(delete_anchors.get(), 2, audio_fingerprint) ||
        sqlite3_step(delete_anchors.get()) != SQLITE_DONE) {
        return database_failure<Unit>("save_alignment_index");
    }

    for (std::size_t ordinal = 0; ordinal < index.anchors.size(); ++ordinal) {
        const auto& anchor = index.anchors[ordinal];
        Statement insert_anchor(
            database_,
            "INSERT INTO AlignmentAnchor"
            "(book_id, audio_fingerprint, ordinal, word_offset, audio_time_millis, confidence) "
            "VALUES (?, ?, ?, ?, ?, ?)");
        if (!insert_anchor.valid() || !bind_text(insert_anchor.get(), 1, book_id.value) ||
            !bind_text(insert_anchor.get(), 2, audio_fingerprint) ||
            sqlite3_bind_int64(insert_anchor.get(), 3, static_cast<std::int64_t>(ordinal)) !=
                SQLITE_OK ||
            sqlite3_bind_int64(insert_anchor.get(), 4, anchor.word_offset.value) != SQLITE_OK ||
            sqlite3_bind_int64(insert_anchor.get(), 5, anchor.audio_time_millis) != SQLITE_OK ||
            sqlite3_bind_double(insert_anchor.get(), 6, anchor.confidence) != SQLITE_OK ||
            sqlite3_step(insert_anchor.get()) != SQLITE_DONE) {
            return database_failure<Unit>("save_alignment_index");
        }
    }
    if (!transaction.commit()) {
        return database_failure<Unit>("save_alignment_index");
    }
    return AppResult<Unit>::success(Unit{});
}

} // namespace goodreader::infrastructure
