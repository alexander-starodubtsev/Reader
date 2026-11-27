#pragma once

#include "goodreader/application/ports.hpp"

#include <memory>
#include <mutex>
#include <string>

struct sqlite3;

namespace goodreader::infrastructure {

/** SQLite repositories for the GoodReader local library and progress data. */
class SqliteLibraryRepository final : public application::BookRepository,
                                      public application::AudioTrackRepository,
                                      public application::ProgressRepository,
                                      public application::ReadingSessionRepository,
                                      public application::AlignmentIndexRepository,
                                      public application::LibraryImportRepository {
  public:
    static constexpr int current_schema_version = 3;

    /** Opens or creates a database and applies every required schema migration. */
    static application::AppResult<std::unique_ptr<SqliteLibraryRepository>>
    open(const std::string& database_path);

    ~SqliteLibraryRepository() override;

    application::AppResult<std::optional<domain::Book>>
    find_by_id(const domain::BookId& id) override;
    application::AppResult<std::vector<domain::Book>>
    search(const std::optional<std::string>& query) override;
    application::AppResult<application::Unit> save(const domain::Book& book) override;

    application::AppResult<std::vector<domain::AudioTrack>>
    list_for_book(const domain::BookId& book_id) override;
    application::AppResult<application::Unit> save(const domain::AudioTrack& track) override;
    application::AppResult<application::Unit>
    save_imported_book(const domain::Book& book,
                       const std::vector<domain::AudioTrack>& audio_tracks) override;

    application::AppResult<std::optional<domain::ReadingProgress>>
    reading_progress(const domain::BookId& book_id) override;
    application::AppResult<application::Unit>
    save_reading_progress(const domain::ReadingProgress& progress) override;
    application::AppResult<std::optional<domain::ListeningProgress>>
    listening_progress(const domain::BookId& book_id) override;
    application::AppResult<application::Unit>
    save_listening_progress(const domain::ListeningProgress& progress) override;

    application::AppResult<std::vector<domain::ReadingSession>>
    list_reading_sessions_for_book(const domain::BookId& book_id) override;
    application::AppResult<application::Unit> save(const domain::ReadingSession& session) override;

    application::AppResult<std::optional<domain::AlignmentIndex>>
    find(const domain::BookId& book_id, const std::string& audio_fingerprint) override;
    application::AppResult<application::Unit> save(const domain::BookId& book_id,
                                                   const std::string& audio_fingerprint,
                                                   const domain::AlignmentIndex& index) override;

  private:
    explicit SqliteLibraryRepository(sqlite3* database);

    application::AppResult<application::Unit> migrate();

    sqlite3* database_;
    std::mutex mutex_;
};

} // namespace goodreader::infrastructure
