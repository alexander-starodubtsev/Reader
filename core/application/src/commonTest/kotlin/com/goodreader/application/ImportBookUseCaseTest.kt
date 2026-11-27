package com.goodreader.application

import com.goodreader.domain.AudioFormat
import com.goodreader.domain.AudioTrack
import com.goodreader.domain.AudioTrackId
import com.goodreader.domain.Book
import com.goodreader.domain.BookFormat
import com.goodreader.domain.BookId
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertIs

class ImportBookUseCaseTest {
    @Test
    fun acceptsBookWithoutAudioAndDelegatesToImporter() =
        runTest {
            val imported =
                ImportedBook(
                    Book(BookId("book"), "Title", null, BookFormat.EPUB, "file:///book.epub", 20),
                    emptyList(),
                )
            val importer = FakeImporter(AppResult.Success(imported))

            val result = ImportBookUseCase(importer)(BookImportRequest("file:///book.epub", "epub"))

            assertEquals(AppResult.Success(imported), result)
            assertEquals(1, importer.calls)
        }

    @Test
    fun validatesBookAndAudioFilesBeforeImport() =
        runTest {
            val importer = FakeImporter(AppResult.Failure(AppError.Unavailable("import", "unavailable")))
            val useCase = ImportBookUseCase(importer)

            val invalidBook = useCase(BookImportRequest("file:///book.txt", "txt"))
            val invalidAudio =
                useCase(
                    BookImportRequest(
                        bookUri = "file:///book.pdf",
                        bookExtension = "pdf",
                        audioFiles = listOf(AudioImportFile("file:///track.flac", "flac", 0)),
                    ),
                )

            assertIs<AppResult.Failure>(invalidBook)
            assertEquals("unsupported_book_format", invalidBook.errorCode())
            assertEquals("unsupported_audio_format", invalidAudio.errorCode())
            assertEquals(0, importer.calls)
        }

    @Test
    fun rejectsDuplicateChapterPositionsAndAcceptsValidPairing() =
        runTest {
            val track = AudioTrack(AudioTrackId("audio"), BookId("book"), "file:///chapter.m4b", AudioFormat.M4B, 1000, 0)
            val imported =
                ImportedBook(
                    Book(BookId("book"), "Title", null, BookFormat.PDF, "file:///book.pdf", null),
                    listOf(track),
                )
            val importer = FakeImporter(AppResult.Success(imported))
            val useCase = ImportBookUseCase(importer)

            val duplicate =
                useCase(
                    BookImportRequest(
                        "file:///book.pdf",
                        "pdf",
                        listOf(
                            AudioImportFile("file:///one.mp3", "mp3", 0),
                            AudioImportFile("file:///two.mp3", "mp3", 0),
                        ),
                    ),
                )
            val valid =
                useCase(
                    BookImportRequest("file:///book.pdf", "pdf", listOf(AudioImportFile("file:///a.m4b", "m4b", 0))),
                )

            assertEquals("invalid_audio_order", duplicate.errorCode())
            assertEquals(
                AppResult.Success(imported.copy(book = imported.book.copy(audioTrackCount = 1))),
                valid,
            )
            assertEquals(1, importer.calls)
        }

    private fun AppResult<*>.errorCode(): String = (this as AppResult.Failure).error.let { (it as AppError.Validation).code }

    private class FakeImporter(
        private val result: AppResult<ImportedBook>,
    ) : BookImporter {
        var calls: Int = 0

        override suspend fun import(request: BookImportRequest): AppResult<ImportedBook> {
            calls += 1
            return result
        }
    }
}
