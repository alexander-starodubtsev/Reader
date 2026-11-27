package com.goodreader.windows

import com.goodreader.application.AppError
import com.goodreader.application.AppResult
import com.goodreader.application.BookImportRequest
import com.goodreader.application.BookImporter
import com.goodreader.application.BookRepository
import com.goodreader.application.ImportBookUseCase
import com.goodreader.application.ImportedBook
import com.goodreader.application.SearchLibraryUseCase
import com.goodreader.domain.Book
import com.goodreader.domain.BookFormat
import com.goodreader.domain.BookId
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.UnconfinedTestDispatcher
import kotlinx.coroutines.test.runTest
import kotlin.test.Test
import kotlin.test.assertEquals

class WindowsLibraryViewModelTest {
    @OptIn(ExperimentalCoroutinesApi::class)
    @Test
    fun searchingUpdatesTheVisibleBooks() =
        runTest {
            val book =
                Book(
                    id = BookId("book-1"),
                    title = "Reader Test",
                    author = "A. Writer",
                    format = BookFormat.EPUB,
                    fileUri = "file:///books/book.epub",
                    wordCount = null,
                )
            val repository = TestBookRepository(listOf(book))
            val dispatcher = UnconfinedTestDispatcher(testScheduler)
            val viewModel =
                WindowsLibraryViewModel(
                    SearchLibraryUseCase(repository),
                    ImportBookUseCase(TestBookImporter()),
                    dispatcher,
                    dispatcher,
                )

            viewModel.search("Reader")
            testScheduler.advanceUntilIdle()

            assertEquals(listOf(book), viewModel.state.value.books)
            assertEquals("Reader", viewModel.state.value.query)
            viewModel.close()
        }

    private class TestBookRepository(
        private val books: List<Book>,
    ) : BookRepository {
        override suspend fun findById(id: BookId): AppResult<Book?> = AppResult.Success(books.find { it.id == id })

        override suspend fun search(query: String?): AppResult<List<Book>> =
            AppResult.Success(books.filter { query == null || it.title.contains(query, ignoreCase = true) })

        override suspend fun save(book: Book): AppResult<Unit> = AppResult.Success(Unit)
    }

    private class TestBookImporter : BookImporter {
        override suspend fun import(request: BookImportRequest): AppResult<ImportedBook> =
            AppResult.Failure(AppError.Unavailable("test", "Import is not used in this test."))
    }
}
