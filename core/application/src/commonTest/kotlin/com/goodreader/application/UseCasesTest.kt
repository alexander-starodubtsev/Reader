package com.goodreader.application

import com.goodreader.domain.AudioTrackId
import com.goodreader.domain.Book
import com.goodreader.domain.BookId
import com.goodreader.domain.ListeningProgress
import com.goodreader.domain.ReadingProgress
import kotlinx.datetime.Instant
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertIs

class UseCasesTest {
    @Test
    fun trimsEmptyLibraryQueriesAndPropagatesRepositoryResults() =
        runTest {
            val repository = FakeBookRepository(AppResult.Success(emptyList()))

            assertEquals(AppResult.Success(emptyList()), SearchLibraryUseCase(repository)("   "))
            assertEquals(null, repository.searchQuery)

            val failure = AppResult.Failure(AppError.Unavailable("storage", "offline"))
            repository.searchResult = failure
            assertEquals(failure, SearchLibraryUseCase(repository)(" title "))
            assertEquals("title", repository.searchQuery)
        }

    @Test
    fun savesBothProgressTypesAndReturnsStorageFailures() =
        runTest {
            val repository = FakeProgressRepository()
            val timestamp = Instant.fromEpochMilliseconds(1000)
            val reading = ReadingProgress(BookId("book"), 10, timestamp)
            val listening = ListeningProgress(BookId("book"), AudioTrackId("track"), 20, timestamp)

            assertEquals(AppResult.Success(Unit), SaveReadingProgressUseCase(repository)(reading))
            assertEquals(AppResult.Success(Unit), SaveListeningProgressUseCase(repository)(listening))
            assertEquals(reading, repository.reading)
            assertEquals(listening, repository.listening)

            val failure = AppResult.Failure(AppError.Unavailable("storage", "offline"))
            repository.writeResult = failure
            assertEquals(failure, SaveReadingProgressUseCase(repository)(reading))
            assertEquals(failure, SaveListeningProgressUseCase(repository)(listening))
        }

    @Test
    fun validatesAndDelegatesPdfConversion() =
        runTest {
            val converter = FakeConverter(AppResult.Success(ConversionProgress(0.5)))
            val useCase = ConvertPdfUseCase(converter)

            val invalid = useCase(" ", ConversionFormat.EPUB)
            assertIs<AppResult.Failure>(invalid)
            assertEquals(0, converter.calls)
            assertEquals(AppResult.Success(ConversionProgress(0.5)), useCase("file:///book.pdf", ConversionFormat.FB2))
            assertEquals(1, converter.calls)
        }

    @Test
    fun rejectsInvalidPortValueObjects() {
        kotlin.test.assertFailsWith<IllegalArgumentException> { RecognizedSpeech(" ", 0, 1, "fake") }
        kotlin.test.assertFailsWith<IllegalArgumentException> { ConversionProgress(2.0) }
        kotlin.test.assertFailsWith<IllegalArgumentException> { LocalUser("", true) }
    }

    private class FakeBookRepository(
        var searchResult: AppResult<List<Book>>,
    ) : BookRepository {
        var searchQuery: String? = null

        override suspend fun findById(id: BookId): AppResult<Book?> = AppResult.Success(null)

        override suspend fun search(query: String?): AppResult<List<Book>> {
            searchQuery = query
            return searchResult
        }

        override suspend fun save(book: Book): AppResult<Unit> = AppResult.Success(Unit)
    }

    private class FakeProgressRepository : ProgressRepository {
        var reading: ReadingProgress? = null
        var listening: ListeningProgress? = null
        var writeResult: AppResult<Unit> = AppResult.Success(Unit)

        override suspend fun readingProgress(bookId: BookId): AppResult<ReadingProgress?> = AppResult.Success(reading)

        override suspend fun saveReadingProgress(progress: ReadingProgress): AppResult<Unit> {
            if (writeResult is AppResult.Success) reading = progress
            return writeResult
        }

        override suspend fun listeningProgress(bookId: BookId): AppResult<ListeningProgress?> = AppResult.Success(listening)

        override suspend fun saveListeningProgress(progress: ListeningProgress): AppResult<Unit> {
            if (writeResult is AppResult.Success) listening = progress
            return writeResult
        }
    }

    private class FakeConverter(
        private val result: AppResult<ConversionProgress>,
    ) : BookConverter {
        var calls: Int = 0

        override suspend fun convert(
            sourceUri: String,
            outputFormat: ConversionFormat,
        ): AppResult<ConversionProgress> {
            calls += 1
            return result
        }
    }
}
