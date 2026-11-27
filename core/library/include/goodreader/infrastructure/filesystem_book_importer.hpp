#pragma once

#include "goodreader/application/ports.hpp"

namespace goodreader::infrastructure {

/** Generates opaque identifiers using fresh local entropy for each request. */
class RandomIdGenerator final : public application::IdGenerator {
  public:
    /** Returns a new opaque identifier. */
    std::string next_id() override;
};

/** Imports selected local book/audio files and persists the complete pairing atomically. */
class FilesystemBookImporter final : public application::BookImporter {
  public:
    /** Creates an importer with transactional persistence and an injected ID generator. */
    FilesystemBookImporter(application::LibraryImportRepository& library,
                           application::IdGenerator& ids);

    /** Inspects user-selected files, creates metadata, and saves the book/audio pair. */
    application::AppResult<application::ImportedBook>
    import(const application::BookImportRequest& request) override;

  private:
    application::LibraryImportRepository& library_;
    application::IdGenerator& ids_;
};

} // namespace goodreader::infrastructure
