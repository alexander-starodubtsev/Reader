package com.goodreader.windows

import app.cash.sqldelight.driver.jdbc.sqlite.JdbcSqliteDriver
import com.goodreader.application.AppError
import com.goodreader.application.AppResult
import com.goodreader.application.AudioImportFile
import com.goodreader.application.BookImportRequest
import com.goodreader.domain.BookFormat
import com.goodreader.infrastructure.database.GoodReaderDatabase
import com.goodreader.infrastructure.database.SqlDelightLibraryRepository
import kotlinx.coroutines.runBlocking
import java.io.ByteArrayInputStream
import java.io.File
import java.net.URI
import java.nio.file.Files
import java.nio.file.Path
import javax.sound.sampled.AudioFileFormat
import javax.sound.sampled.AudioInputStream
import javax.sound.sampled.AudioSystem
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertIs
import kotlin.test.assertTrue

class WindowsBookImporterTest {
    @Test
    fun readsDurationFromSupportedWavAudio() {
        val directory = Files.createTempDirectory("goodreader-wav")
        val wav = directory.resolve("chapter.wav")
        val format = javax.sound.sampled.AudioFormat(8_000f, 16, 1, true, false)
        val stream = AudioInputStream(ByteArrayInputStream(ByteArray(16_000)), format, 8_000)
        try {
            AudioSystem.write(stream, AudioFileFormat.Type.WAVE, wav.toFile())
            val result = JaudiotaggerDurationReader().durationMillis(wav.toFile())
            assertEquals(1_000L, assertIs<AppResult.Success<Long>>(result).value)
        } finally {
            stream.close()
            deleteDirectory(directory.toFile())
        }
    }

    @Test
    fun importsBookAndOrderedAudioIntoAppStorage() =
        runBlocking {
            val sourceDirectory = Files.createTempDirectory("goodreader-source")
            val storageDirectory = Files.createTempDirectory("goodreader-storage")
            val databaseDriver = JdbcSqliteDriver("jdbc:sqlite::memory:")
            try {
                GoodReaderDatabase.Schema.create(databaseDriver)
                val repository = SqlDelightLibraryRepository(GoodReaderDatabase(databaseDriver))
                val bookFile = Files.writeString(sourceDirectory.resolve("A Novel.fb2"), "book")
                val chapterOne = Files.writeString(sourceDirectory.resolve("one.mp3"), "audio one")
                val chapterTwo = Files.writeString(sourceDirectory.resolve("two.ogg"), "audio two")
                val importer =
                    WindowsBookImporter(
                        library = repository,
                        storageDirectory = storageDirectory,
                        durationReader = AudioDurationReader { AppResult.Success(12_000L) },
                    )

                val result =
                    importer.import(
                        BookImportRequest(
                            bookUri = bookFile.toString(),
                            bookExtension = "fb2",
                            audioFiles =
                                listOf(
                                    AudioImportFile(chapterTwo.toString(), "ogg", 1),
                                    AudioImportFile(chapterOne.toString(), "mp3", 0),
                                ),
                        ),
                    )

                val imported = assertIs<AppResult.Success<com.goodreader.application.ImportedBook>>(result).value
                assertEquals(BookFormat.FB2, imported.book.format)
                assertEquals("A Novel", imported.book.title)
                assertTrue(Path.of(URI.create(imported.book.fileUri)).toFile().exists())
                assertEquals(listOf(0, 1), imported.audioTracks.map { it.chapterIndex })
                assertEquals(listOf("MP3", "OGG"), imported.audioTracks.map { it.format.name })
                assertTrue(imported.audioTracks.all { Path.of(URI.create(it.uri)).toFile().exists() })
                assertEquals(2, repository.listForBook(imported.book.id).getSuccess().size)
                assertEquals(1, repository.search("Novel").getSuccess().size)
            } finally {
                databaseDriver.close()
                deleteDirectory(sourceDirectory.toFile())
                deleteDirectory(storageDirectory.toFile())
            }
        }

    @Test
    fun doesNotCopyOrPersistWhenAudioMetadataCannotBeRead() =
        runBlocking {
            val sourceDirectory = Files.createTempDirectory("goodreader-invalid-source")
            val storageDirectory = Files.createTempDirectory("goodreader-invalid-storage")
            val databaseDriver = JdbcSqliteDriver("jdbc:sqlite::memory:")
            try {
                GoodReaderDatabase.Schema.create(databaseDriver)
                val repository = SqlDelightLibraryRepository(GoodReaderDatabase(databaseDriver))
                val bookFile = Files.writeString(sourceDirectory.resolve("A Novel.epub"), "book")
                val audioFile = Files.writeString(sourceDirectory.resolve("chapter.mp3"), "not media")
                val importer =
                    WindowsBookImporter(
                        library = repository,
                        storageDirectory = storageDirectory,
                        durationReader =
                            AudioDurationReader {
                                AppResult.Failure(
                                    AppError.Validation("audio_duration_unavailable", "Audio duration unavailable."),
                                )
                            },
                    )

                val result =
                    importer.import(
                        BookImportRequest(
                            bookUri = bookFile.toString(),
                            bookExtension = "epub",
                            audioFiles = listOf(AudioImportFile(audioFile.toString(), "mp3", 0)),
                        ),
                    )

                assertIs<AppResult.Failure>(result)
                assertTrue(repository.search(null).getSuccess().isEmpty())
                Files.list(storageDirectory).use { assertFalse(it.findAny().isPresent) }
            } finally {
                databaseDriver.close()
                deleteDirectory(sourceDirectory.toFile())
                deleteDirectory(storageDirectory.toFile())
            }
        }

    private fun <T> AppResult<T>.getSuccess(): T = assertIs<AppResult.Success<T>>(this).value

    private fun deleteDirectory(directory: File) {
        directory.walkBottomUp().forEach(File::delete)
    }
}
