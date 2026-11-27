#pragma once

#include "goodreader/application/ports.hpp"

namespace goodreader::application {

/** Searches the local library and propagates repository failures. */
class SearchLibraryUseCase {
  public:
    /** Creates a use case backed by the local book repository. */
    explicit SearchLibraryUseCase(BookRepository& books);
    /** Returns matching books and propagates repository failures unchanged. */
    AppResult<std::vector<domain::Book>>
    operator()(std::optional<std::string> query = std::nullopt);

  private:
    BookRepository& books_;
};

/** Imports one user-selected book and zero or more user-selected audio files. */
class ImportBookUseCase {
  public:
    /** Creates a use case backed by the importer port. */
    explicit ImportBookUseCase(BookImporter& importer);
    /** Validates selected book/audio files and delegates the import operation. */
    AppResult<ImportedBook> operator()(const BookImportRequest& request);

  private:
    BookImporter& importer_;
};

/** Saves the most recently viewed text position. */
class SaveReadingProgressUseCase {
  public:
    /** Creates a use case backed by the progress repository. */
    explicit SaveReadingProgressUseCase(ProgressRepository& progress);
    /** Persists a text position without hiding storage errors. */
    AppResult<Unit> operator()(const domain::ReadingProgress& progress);

  private:
    ProgressRepository& progress_;
};

/** Saves the most recently played audio position. */
class SaveListeningProgressUseCase {
  public:
    /** Creates a use case backed by the progress repository. */
    explicit SaveListeningProgressUseCase(ProgressRepository& progress);
    /** Persists an audio position without hiding storage errors. */
    AppResult<Unit> operator()(const domain::ListeningProgress& progress);

  private:
    ProgressRepository& progress_;
};

/** Checks a book's supported conversion output before dispatching conversion. */
class ConvertPdfUseCase {
  public:
    /** Creates a use case backed by the converter port. */
    explicit ConvertPdfUseCase(BookConverter& converter);
    /** Validates a source URI and starts a background PDF conversion. */
    AppResult<ConversionProgress> operator()(const std::string& source_uri,
                                             ConversionFormat output_format);

  private:
    BookConverter& converter_;
};

/** Indicates whether an application feature is currently available. */
class CheckFeatureUseCase {
  public:
    /** Creates a use case backed by the entitlement service. */
    explicit CheckFeatureUseCase(EntitlementService& entitlements);
    /** Returns the entitlement service's decision. */
    AppResult<bool> operator()(Feature feature);

  private:
    EntitlementService& entitlements_;
};

/** Loads the current local identity for attribution of persisted data. */
class GetCurrentUserUseCase {
  public:
    /** Creates a use case backed by the authentication service. */
    explicit GetCurrentUserUseCase(AuthService& authentication);
    /** Returns the current local-only identity or the authentication error. */
    AppResult<LocalUser> operator()();

  private:
    AuthService& authentication_;
};

} // namespace goodreader::application
