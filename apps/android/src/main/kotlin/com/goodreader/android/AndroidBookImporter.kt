package com.goodreader.android

import android.content.ContentResolver
import android.content.Context
import android.media.MediaMetadataRetriever
import android.net.Uri
import android.provider.OpenableColumns
import com.goodreader.application.AppError
import com.goodreader.application.AppResult
import com.goodreader.application.AudioImportFile
import com.goodreader.application.BookImportRequest
import com.goodreader.application.BookImporter
import com.goodreader.application.ImportedBook
import com.goodreader.domain.AudioFormat
import com.goodreader.domain.AudioTrack
import com.goodreader.domain.AudioTrackId
import com.goodreader.domain.Book
import com.goodreader.domain.BookFormat
import com.goodreader.domain.BookId
import com.goodreader.infrastructure.database.SqlDelightLibraryRepository
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.io.File
import java.util.Locale
import java.util.UUID

/** Imports Android document-provider URIs and persists their book/audio pairing. */
class AndroidBookImporter(
    context: Context,
    private val library: SqlDelightLibraryRepository,
) : BookImporter {
    private val applicationContext = context.applicationContext
    private val contentResolver: ContentResolver = applicationContext.contentResolver

    override suspend fun import(request: BookImportRequest): AppResult<ImportedBook> =
        withContext(Dispatchers.IO) {
            try {
                importSelectedFiles(request)
            } catch (_: SecurityException) {
                AppResult.Failure(
                    AppError.Unavailable("document_provider", "The selected file is no longer accessible."),
                )
            } catch (_: Exception) {
                AppResult.Failure(
                    AppError.Unavailable("document_import", "The selected files could not be read."),
                )
            }
        }

    private suspend fun importSelectedFiles(request: BookImportRequest): AppResult<ImportedBook> {
        val bookFormat =
            BookFormat.fromExtension(request.bookExtension)
                ?: return AppResult.Failure(AppError.Validation("unsupported_book_format", "Unsupported book format."))
        val bookUri = Uri.parse(request.bookUri)
        val validationFailure = validateBookWithCore(bookUri, request.bookExtension)
        if (validationFailure != null) {
            return AppResult.Failure(validationFailure)
        }
        val title = displayName(contentResolver, bookUri).substringBeforeLast('.').ifBlank { "Untitled" }
        val book =
            Book(
                id = BookId(UUID.randomUUID().toString()),
                title = title,
                author = null,
                format = bookFormat,
                fileUri = request.bookUri,
                wordCount = null,
            )

        return when (val trackResult = createAudioTracks(request.audioFiles, book.id)) {
            is AppResult.Failure -> AppResult.Failure(trackResult.error)
            is AppResult.Success ->
                when (val saved = library.saveImportedBook(book, trackResult.value)) {
                    is AppResult.Success -> AppResult.Success(ImportedBook(book, trackResult.value))
                    is AppResult.Failure -> AppResult.Failure(saved.error)
                }
        }
    }

    private fun validateBookWithCore(
        uri: Uri,
        extension: String,
    ): AppError? {
        val stagedFile =
            File.createTempFile(
                "goodreader-book-",
                ".${extension.lowercase(Locale.ROOT)}",
                applicationContext.cacheDir,
            )
        return try {
            val input =
                contentResolver.openInputStream(uri)
                    ?: return AppError.Unavailable(
                        "document_provider",
                        "The selected book file could not be opened.",
                    )
            input.use { source ->
                stagedFile.outputStream().use { target -> source.copyTo(target) }
            }
            when (NativeCore.validateBookFile(stagedFile.absolutePath.toByteArray(Charsets.UTF_8))) {
                NativeCore.STATUS_OK -> null
                NativeCore.STATUS_UNSUPPORTED_FORMAT ->
                    AppError.Validation("unsupported_book_format", "Unsupported book format.")
                NativeCore.STATUS_NOT_FOUND -> AppError.NotFound("book_file", uri.toString())
                else -> AppError.Unavailable("native_core", "The selected book file could not be validated.")
            }
        } finally {
            stagedFile.delete()
        }
    }

    private fun createAudioTracks(
        files: List<AudioImportFile>,
        bookId: BookId,
    ): AppResult<List<AudioTrack>> {
        val tracks = mutableListOf<AudioTrack>()
        var failure: AppError? = null
        files.sortedBy(AudioImportFile::chapterIndex).forEach { file ->
            if (failure == null) {
                when (val result = createAudioTrack(file, bookId)) {
                    is AppResult.Success -> tracks += result.value
                    is AppResult.Failure -> failure = result.error
                }
            }
        }
        return failure?.let { AppResult.Failure(it) } ?: AppResult.Success(tracks)
    }

    private fun createAudioTrack(
        file: AudioImportFile,
        bookId: BookId,
    ): AppResult<AudioTrack> {
        val format = AudioFormat.fromExtension(file.extension)
        if (format == null) {
            return AppResult.Failure(AppError.Validation("unsupported_audio_format", "Unsupported audio format."))
        }
        val duration = audioDurationMillis(Uri.parse(file.uri))
        return if (duration <= 0L) {
            AppResult.Failure(
                AppError.Validation(
                    "audio_duration_unavailable",
                    "The selected audio file has no readable duration.",
                ),
            )
        } else {
            AppResult.Success(
                AudioTrack(
                    id = AudioTrackId(UUID.randomUUID().toString()),
                    bookId = bookId,
                    uri = file.uri,
                    format = format,
                    durationMillis = duration,
                    chapterIndex = file.chapterIndex,
                ),
            )
        }
    }

    private fun displayName(
        resolver: ContentResolver,
        uri: Uri,
    ): String {
        resolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)?.use { cursor ->
            if (cursor.moveToFirst()) {
                val index = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
                if (index >= 0) return cursor.getString(index)
            }
        }
        return uri.lastPathSegment?.substringAfterLast('/') ?: "Untitled"
    }

    private fun audioDurationMillis(uri: Uri): Long {
        val retriever = MediaMetadataRetriever()
        return try {
            retriever.setDataSource(applicationContext, uri)
            retriever.extractMetadata(MediaMetadataRetriever.METADATA_KEY_DURATION)?.toLongOrNull() ?: 0L
        } finally {
            retriever.release()
        }
    }
}
