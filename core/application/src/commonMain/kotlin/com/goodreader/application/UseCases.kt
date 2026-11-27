package com.goodreader.application

import com.goodreader.domain.AudioFormat
import com.goodreader.domain.Book
import com.goodreader.domain.BookFormat
import com.goodreader.domain.ListeningProgress
import com.goodreader.domain.ReadingProgress

/** Searches the local library. */
class SearchLibraryUseCase(
    private val books: BookRepository,
) {
    /** Returns matching books, propagating repository errors unchanged. */
    suspend operator fun invoke(query: String? = null): AppResult<List<Book>> = books.search(query?.trim()?.takeIf { it.isNotEmpty() })
}

/** Imports one user-selected book and zero or more user-selected audio files. */
class ImportBookUseCase(
    private val importer: BookImporter,
) {
    /** Validates file extensions and delegates the import operation. */
    suspend operator fun invoke(request: BookImportRequest): AppResult<ImportedBook> {
        if (request.bookUri.isBlank()) {
            return AppResult.Failure(AppError.Validation("book_uri_required", "Choose a book file."))
        }
        if (BookFormat.fromExtension(request.bookExtension) == null) {
            return AppResult.Failure(
                AppError.Validation("unsupported_book_format", "Choose a PDF, EPUB, or FB2 file."),
            )
        }
        val audioFiles = request.audioFiles
        if (audioFiles.any { it.uri.isBlank() || AudioFormat.fromExtension(it.extension) == null }) {
            return AppResult.Failure(
                AppError.Validation("unsupported_audio_format", "Choose supported audio files."),
            )
        }
        if (audioFiles.map { it.chapterIndex }.any { it < 0 } ||
            audioFiles.map { it.chapterIndex }.distinct().size != audioFiles.size
        ) {
            return AppResult.Failure(
                AppError.Validation("invalid_audio_order", "Audio chapter positions must be unique and non-negative."),
            )
        }
        return when (val result = importer.import(request)) {
            is AppResult.Success ->
                AppResult.Success(
                    result.value.copy(book = result.value.book.copy(audioTrackCount = result.value.audioTracks.size)),
                )
            is AppResult.Failure -> result
        }
    }
}

/** Saves the most recently viewed text position. */
class SaveReadingProgressUseCase(
    private val progress: ProgressRepository,
) {
    /** Persists a text position without hiding storage errors. */
    suspend operator fun invoke(value: ReadingProgress): AppResult<Unit> = progress.saveReadingProgress(value)
}

/** Saves the most recently played audio position. */
class SaveListeningProgressUseCase(
    private val progress: ProgressRepository,
) {
    /** Persists an audio position without hiding storage errors. */
    suspend operator fun invoke(value: ListeningProgress): AppResult<Unit> = progress.saveListeningProgress(value)
}

/** Checks a book's supported conversion output before dispatching conversion. */
class ConvertPdfUseCase(
    private val converter: BookConverter,
) {
    /** Starts a cancellable background conversion for a non-empty PDF URI. */
    suspend operator fun invoke(
        sourceUri: String,
        outputFormat: ConversionFormat,
    ): AppResult<ConversionProgress> {
        if (sourceUri.isBlank()) {
            return AppResult.Failure(AppError.Validation("source_uri_required", "Choose a PDF file."))
        }
        return converter.convert(sourceUri, outputFormat)
    }
}

/** Indicates whether an application feature is currently available. */
class CheckFeatureUseCase(
    private val entitlements: EntitlementService,
) {
    /** Returns the entitlement service's decision. */
    suspend operator fun invoke(feature: Feature): AppResult<Boolean> = entitlements.isEnabled(feature)
}

/** Loads the current local identity for attribution of persisted data. */
class GetCurrentUserUseCase(
    private val authentication: AuthService,
) {
    /** Returns the current local-only identity or the authentication error. */
    suspend operator fun invoke(): AppResult<LocalUser> = authentication.currentUser()
}
