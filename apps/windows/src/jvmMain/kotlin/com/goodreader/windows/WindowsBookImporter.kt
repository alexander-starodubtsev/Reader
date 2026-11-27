package com.goodreader.windows

import com.goodreader.application.AppError
import com.goodreader.application.AppResult
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
import org.jaudiotagger.audio.AudioFileIO
import java.io.File
import java.io.IOException
import java.nio.file.Files
import java.nio.file.Path
import java.nio.file.StandardCopyOption
import java.util.UUID

/** Reads audio duration from the media container without decoding the full recording. */
fun interface AudioDurationReader {
    /** Returns the duration in milliseconds, or an explicit failure for unreadable media. */
    fun durationMillis(file: File): AppResult<Long>
}

/** Jaudiotagger-backed duration reader for the audio formats supported by Windows import. */
class JaudiotaggerDurationReader : AudioDurationReader {
    override fun durationMillis(file: File): AppResult<Long> =
        try {
            val seconds = AudioFileIO.read(file).audioHeader.trackLength
            if (seconds <= 0) {
                AppResult.Failure(
                    AppError.Validation(
                        "audio_duration_unavailable",
                        "The selected audio file has no readable duration.",
                    ),
                )
            } else {
                AppResult.Success(seconds * MILLIS_PER_SECOND)
            }
        } catch (_: Exception) {
            AppResult.Failure(
                AppError.Validation(
                    "audio_duration_unavailable",
                    "The selected audio file has no readable duration.",
                ),
            )
        }

    private companion object {
        const val MILLIS_PER_SECOND = 1_000L
    }
}

/** Imports selected Windows files into app-owned storage and persists their pairing. */
class WindowsBookImporter(
    private val library: SqlDelightLibraryRepository,
    private val storageDirectory: Path,
    private val durationReader: AudioDurationReader = JaudiotaggerDurationReader(),
) : BookImporter {
    override suspend fun import(request: BookImportRequest): AppResult<ImportedBook> =
        withContext(Dispatchers.IO) {
            importSelectedFiles(request)
        }

    private suspend fun importSelectedFiles(request: BookImportRequest): AppResult<ImportedBook> {
        val sourceBook = File(request.bookUri)
        val bookFormat = BookFormat.fromExtension(request.bookExtension)
        if (!sourceBook.isFile || bookFormat == null) {
            return AppResult.Failure(
                AppError.Validation("unsupported_book_format", "Choose an existing PDF, EPUB, or FB2 file."),
            )
        }
        val sourceAudio =
            request.audioFiles.map { audio ->
                val file = File(audio.uri)
                val format = AudioFormat.fromExtension(audio.extension)
                if (!file.isFile || format == null) {
                    return AppResult.Failure(
                        AppError.Validation("unsupported_audio_format", "Choose existing supported audio files."),
                    )
                }
                val duration =
                    when (val result = durationReader.durationMillis(file)) {
                        is AppResult.Success -> result.value
                        is AppResult.Failure -> return result
                    }
                if (duration <= 0L) {
                    return AppResult.Failure(
                        AppError.Validation(
                            "audio_duration_unavailable",
                            "The selected audio file has no readable duration.",
                        ),
                    )
                }
                PreparedAudio(file, format, duration, audio.chapterIndex)
            }
        if (sourceAudio.map { it.chapterIndex }.distinct().size != sourceAudio.size ||
            sourceAudio.any { it.chapterIndex < 0 }
        ) {
            return AppResult.Failure(
                AppError.Validation("invalid_audio_order", "Audio chapter positions must be unique and non-negative."),
            )
        }

        val id = BookId(UUID.randomUUID().toString())
        val stagedFiles = mutableListOf<Path>()
        return try {
            val bookPath = copyIntoLibrary(sourceBook.toPath(), bookFormat.name.lowercase(), id.value, stagedFiles)
            val book =
                Book(
                    id = id,
                    title = sourceBook.name.substringBeforeLast('.').ifBlank { "Untitled" },
                    author = null,
                    format = bookFormat,
                    fileUri = bookPath.toUri().toString(),
                    wordCount = null,
                )
            val tracks =
                sourceAudio.sortedBy { it.chapterIndex }.map { audio ->
                    val trackId = AudioTrackId(UUID.randomUUID().toString())
                    val audioPath =
                        copyIntoLibrary(
                            audio.file.toPath(),
                            audio.format.name.lowercase(),
                            trackId.value,
                            stagedFiles,
                        )
                    AudioTrack(
                        id = trackId,
                        bookId = id,
                        uri = audioPath.toUri().toString(),
                        format = audio.format,
                        durationMillis = audio.durationMillis,
                        chapterIndex = audio.chapterIndex,
                    )
                }
            when (val result = library.saveImportedBook(book, tracks)) {
                is AppResult.Success -> AppResult.Success(ImportedBook(book, tracks))
                is AppResult.Failure -> {
                    if (removeStagedFiles(stagedFiles)) {
                        AppResult.Failure(result.error)
                    } else {
                        AppResult.Failure(
                            AppError.Unavailable(
                                "local_file_storage",
                                "The library could not save the import and temporary copies could not be removed.",
                            ),
                        )
                    }
                }
            }
        } catch (_: Exception) {
            val detail =
                if (removeStagedFiles(stagedFiles)) {
                    "The selected files could not be imported."
                } else {
                    "The selected files could not be imported and temporary copies could not be removed."
                }
            AppResult.Failure(AppError.Unavailable("local_file_storage", detail))
        }
    }

    private fun copyIntoLibrary(
        source: Path,
        extension: String,
        fileId: String,
        stagedFiles: MutableList<Path>,
    ): Path {
        val directory = storageDirectory.resolve("books")
        Files.createDirectories(directory)
        val destination = directory.resolve("$fileId.$extension")
        stagedFiles.add(destination)
        Files.copy(source, destination, StandardCopyOption.REPLACE_EXISTING)
        return destination
    }

    private fun removeStagedFiles(files: List<Path>): Boolean =
        try {
            files.asReversed().forEach(Files::deleteIfExists)
            true
        } catch (_: IOException) {
            false
        } catch (_: SecurityException) {
            false
        }

    private data class PreparedAudio(
        val file: File,
        val format: AudioFormat,
        val durationMillis: Long,
        val chapterIndex: Int,
    )
}
