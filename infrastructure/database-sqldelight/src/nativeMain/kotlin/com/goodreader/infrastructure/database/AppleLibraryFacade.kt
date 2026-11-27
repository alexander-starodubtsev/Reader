package com.goodreader.infrastructure.database

import app.cash.sqldelight.driver.native.NativeSqliteDriver
import com.goodreader.application.AppError
import com.goodreader.application.AppResult
import com.goodreader.application.SearchLibraryUseCase
import com.goodreader.domain.AudioFormat
import com.goodreader.domain.AudioTrack
import com.goodreader.domain.AudioTrackId
import com.goodreader.domain.Book
import com.goodreader.domain.BookFormat
import com.goodreader.domain.BookId
import platform.Foundation.NSUUID
import kotlin.coroutines.cancellation.CancellationException

/** Audio file information supplied by the native Apple file importer. */
data class AppleAudioFile(
    val uri: String,
    val fileExtension: String,
    val durationMillis: Long,
    val chapterIndex: Int,
)

/** Book and track metadata returned to the native Apple user interface. */
data class AppleLibraryBook(
    val id: String,
    val title: String,
    val author: String?,
    val format: String,
    val fileUri: String,
    val audioTracks: List<AppleLibraryAudioTrack>,
)

/** Ordered local audio chapter returned to the native Apple player. */
data class AppleLibraryAudioTrack(
    val id: String,
    val uri: String,
    val durationMillis: Long,
    val chapterIndex: Int,
)

/** Typed error raised by the Apple library bridge. */
class AppleLibraryException(
    val errorCode: String,
    override val message: String,
) : Exception(message)

/**
 * Native SwiftUI bridge for the shared SQLDelight library repository.
 *
 * File access and media metadata remain owned by Swift; this facade owns
 * validation, domain mapping, and transactional persistence.
 */
class AppleLibraryFacade {
    private val repository =
        SqlDelightLibraryRepository(
            GoodReaderDatabase(
                NativeSqliteDriver(GoodReaderDatabase.Schema, "goodreader.db"),
            ),
        )
    private val searchLibrary = SearchLibraryUseCase(repository)

    /** Searches persisted books and maps storage failures to a typed bridge error. */
    @Throws(AppleLibraryException::class, CancellationException::class)
    suspend fun searchBooks(query: String?): List<AppleLibraryBook> =
        searchLibrary(query).getOrThrow("search_books").map { book ->
            book.toAppleLibraryBook(repository.listForBook(book.id).getOrThrow("list_audio_tracks"))
        }

    /** Saves a user-selected book and its optional audio pairing atomically. */
    @Throws(AppleLibraryException::class, CancellationException::class)
    suspend fun importBook(
        bookUri: String,
        title: String,
        bookExtension: String,
        audioFiles: List<AppleAudioFile>,
    ): AppleLibraryBook {
        val format =
            BookFormat.fromExtension(bookExtension)
                ?: throw AppleLibraryException("unsupported_book_format", "Choose a PDF, EPUB, or FB2 file.")
        if (bookUri.isBlank() || title.isBlank()) {
            throw AppleLibraryException("book_metadata_required", "The selected book has incomplete metadata.")
        }
        val book =
            Book(
                id = BookId(NSUUID().UUIDString),
                title = title,
                author = null,
                format = format,
                fileUri = bookUri,
                wordCount = null,
            )
        val tracks = audioFiles.map { it.toDomain(book.id) }
        repository.saveImportedBook(book, tracks).getOrThrow("import_book")
        return book.toAppleLibraryBook(tracks)
    }

    private fun AppleAudioFile.toDomain(bookId: BookId): AudioTrack {
        val audioFormat =
            AudioFormat.fromExtension(fileExtension)
                ?: throw AppleLibraryException("unsupported_audio_format", "Choose supported audio files.")
        if (uri.isBlank() || durationMillis <= 0L || chapterIndex < 0) {
            throw AppleLibraryException("invalid_audio_metadata", "An audio file has invalid duration or chapter order.")
        }
        return AudioTrack(
            id = AudioTrackId(NSUUID().UUIDString),
            bookId = bookId,
            uri = uri,
            format = audioFormat,
            durationMillis = durationMillis,
            chapterIndex = chapterIndex,
        )
    }

    private fun <T> AppResult<T>.getOrThrow(operation: String): T =
        when (this) {
            is AppResult.Success -> value
            is AppResult.Failure -> throw error.toAppleException(operation)
        }

    private fun AppError.toAppleException(operation: String): AppleLibraryException =
        when (this) {
            is AppError.Validation -> AppleLibraryException(code, detail)
            is AppError.NotFound -> AppleLibraryException("not_found", "The requested $entity was not found.")
            is AppError.Unavailable -> AppleLibraryException(dependency, detail)
            AppError.Cancelled -> AppleLibraryException("cancelled", "The operation was cancelled.")
            is AppError.Unexpected -> AppleLibraryException(operation, "The operation could not be completed.")
        }

    private suspend fun Book.toAppleLibraryBook(repository: SqlDelightLibraryRepository): AppleLibraryBook {
        val tracks = repository.listForBook(id).getOrThrow("list_audio_tracks")
        return toAppleLibraryBook(tracks)
    }

    private fun Book.toAppleLibraryBook(tracks: List<AudioTrack>): AppleLibraryBook =
        AppleLibraryBook(
            id = id.value,
            title = title,
            author = author,
            format = format.name,
            fileUri = fileUri,
            audioTracks =
                tracks.map {
                    AppleLibraryAudioTrack(it.id.value, it.uri, it.durationMillis, it.chapterIndex)
                },
        )
}
