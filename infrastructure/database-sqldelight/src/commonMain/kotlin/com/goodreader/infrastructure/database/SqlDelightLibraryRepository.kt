package com.goodreader.infrastructure.database

import app.cash.sqldelight.db.SqlDriver
import com.goodreader.application.AppError
import com.goodreader.application.AppResult
import com.goodreader.application.AudioTrackRepository
import com.goodreader.application.BookRepository
import com.goodreader.domain.AudioFormat
import com.goodreader.domain.AudioTrack
import com.goodreader.domain.AudioTrackId
import com.goodreader.domain.Book
import com.goodreader.domain.BookFormat
import com.goodreader.domain.BookId

/** SQLDelight-backed repositories for books and ordered audiobook tracks. */
class SqlDelightLibraryRepository(
    private val database: GoodReaderDatabase,
) : BookRepository,
    AudioTrackRepository {
    override suspend fun findById(id: BookId): AppResult<Book?> =
        databaseResult("find_book") {
            database.bookQueries
                .selectBookById(id.value)
                .executeAsOneOrNull()
                ?.toDomain()
                ?.withAudioTrackCount()
        }

    override suspend fun search(query: String?): AppResult<List<Book>> =
        databaseResult("search_books") {
            val rows =
                if (query.isNullOrBlank()) {
                    database.bookQueries.selectAllBooks().executeAsList()
                } else {
                    database.bookQueries.searchBooks(query, query).executeAsList()
                }
            rows.map { it.toDomain().withAudioTrackCount() }
        }

    override suspend fun save(book: Book): AppResult<Unit> =
        databaseResult("save_book") {
            database.transaction {
                persistBook(book)
            }
        }

    private fun persistBook(book: Book) {
        val queries = database.bookQueries
        val existing = queries.selectBookById(book.id.value).executeAsOneOrNull()
        if (existing == null) {
            queries.insertBook(
                book.id.value,
                book.title,
                book.author,
                book.format.name,
                book.fileUri,
                book.wordCount?.toLong(),
            )
        } else {
            queries.updateBook(
                book.title,
                book.author,
                book.format.name,
                book.fileUri,
                book.wordCount?.toLong(),
                book.id.value,
            )
        }
    }

    override suspend fun listForBook(bookId: BookId): AppResult<List<AudioTrack>> =
        databaseResult("list_audio_tracks") {
            database.audioTrackQueries
                .selectAudioTracksForBook(bookId.value)
                .executeAsList()
                .map { it.toDomain() }
        }

    override suspend fun save(track: AudioTrack): AppResult<Unit> =
        databaseResult("save_audio_track") {
            database.audioTrackQueries.upsertAudioTrack(
                track.id.value,
                track.bookId.value,
                track.uri,
                track.format.name,
                track.durationMillis,
                track.chapterIndex.toLong(),
            )
        }

    /** Replaces a book and its complete audio pairing atomically. */
    suspend fun saveImportedBook(
        book: Book,
        tracks: List<AudioTrack>,
    ): AppResult<Unit> {
        val hasDuplicateTrackIds = tracks.map { it.id }.distinct().size != tracks.size
        val hasDuplicateChapters = tracks.map { it.chapterIndex }.distinct().size != tracks.size
        if (tracks.any { it.bookId != book.id } || hasDuplicateTrackIds || hasDuplicateChapters) {
            return AppResult.Failure(
                AppError.Validation(
                    "invalid_audio_pairing",
                    "Audio tracks must belong to this book and have unique IDs and chapters.",
                ),
            )
        }
        return databaseResult("save_imported_book") {
            database.transaction {
                persistBook(book)
                database.audioTrackQueries.deleteAudioTracksForBook(book.id.value)
                tracks.forEach { track ->
                    database.audioTrackQueries.upsertAudioTrack(
                        track.id.value,
                        book.id.value,
                        track.uri,
                        track.format.name,
                        track.durationMillis,
                        track.chapterIndex.toLong(),
                    )
                }
            }
        }
    }

    private inline fun <T> databaseResult(
        operation: String,
        action: () -> T,
    ): AppResult<T> =
        try {
            AppResult.Success(action())
        } catch (_: InvalidLibraryRecordException) {
            AppResult.Failure(AppError.Unexpected(operation))
        } catch (_: Exception) {
            AppResult.Failure(AppError.Unavailable("local_database", operation))
        }

    private fun com.goodreader.infrastructure.database.Book.toDomain(): Book =
        Book(
            id = BookId(id),
            title = title,
            author = author,
            format = BookFormat.entries.firstOrNull { it.name == format } ?: throw InvalidLibraryRecordException(),
            fileUri = file_uri,
            wordCount = word_count?.toIntChecked(),
        )

    private fun com.goodreader.infrastructure.database.AudioTrack.toDomain(): AudioTrack =
        AudioTrack(
            id = AudioTrackId(id),
            bookId = BookId(book_id),
            uri = uri,
            format = AudioFormat.entries.firstOrNull { it.name == format } ?: throw InvalidLibraryRecordException(),
            durationMillis = duration_millis,
            chapterIndex = chapter_index.toIntChecked(),
        )

    private fun Book.withAudioTrackCount(): Book =
        copy(
            audioTrackCount =
                database.audioTrackQueries
                    .selectAudioTracksForBook(id.value)
                    .executeAsList()
                    .size,
        )

    private fun Long.toIntChecked(): Int {
        if (this !in 0..Int.MAX_VALUE.toLong()) {
            throw InvalidLibraryRecordException()
        }
        return toInt()
    }

    private class InvalidLibraryRecordException : Exception()
}

/** Creates the SQLDelight database using the current platform's SQLite driver. */
fun createGoodReaderDatabase(driver: SqlDriver): GoodReaderDatabase = GoodReaderDatabase(driver)
