package com.goodreader.infrastructure.database

import app.cash.sqldelight.driver.jdbc.sqlite.JdbcSqliteDriver
import com.goodreader.application.AppError
import com.goodreader.application.AppResult
import com.goodreader.domain.AudioFormat
import com.goodreader.domain.AudioTrack
import com.goodreader.domain.AudioTrackId
import com.goodreader.domain.Book
import com.goodreader.domain.BookFormat
import com.goodreader.domain.BookId
import kotlinx.coroutines.test.runTest
import kotlin.test.AfterTest
import kotlin.test.BeforeTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertIs

class SqlDelightLibraryRepositoryTest {
    private lateinit var driver: JdbcSqliteDriver
    private lateinit var repository: SqlDelightLibraryRepository

    @BeforeTest
    fun createDatabase() {
        driver = JdbcSqliteDriver(JdbcSqliteDriver.IN_MEMORY)
        GoodReaderDatabase.Schema.create(driver)
        repository = SqlDelightLibraryRepository(createGoodReaderDatabase(driver))
    }

    @AfterTest
    fun closeDatabase() {
        driver.close()
    }

    @Test
    fun storesBooksAndSearchesTitleOrAuthorWithoutCaseSensitivity() =
        runTest {
            val book = book("b-1", "The Silent City", "A. Writer")
            repository.save(book)

            assertEquals(AppResult.Success(book), repository.findById(book.id))
            assertEquals(AppResult.Success(listOf(book)), repository.search("SILENT"))
            assertEquals(AppResult.Success(listOf(book)), repository.search("writer"))
            assertEquals(AppResult.Success(listOf(book)), repository.search(null))
            assertEquals(AppResult.Success(null), repository.findById(BookId("missing")))
        }

    @Test
    fun savesAudioTracksInChapterOrderAndReplacesThePairingAtomically() =
        runTest {
            val book = book("b-1", "Book", null)
            val laterChapter = track("t-2", book.id, 1)
            val firstChapter = track("t-1", book.id, 0)

            assertEquals(AppResult.Success(Unit), repository.saveImportedBook(book, listOf(laterChapter, firstChapter)))
            assertEquals(
                AppResult.Success(listOf(firstChapter, laterChapter)),
                repository.listForBook(book.id),
            )

            val replacement = track("t-3", book.id, 0)
            assertEquals(AppResult.Success(Unit), repository.saveImportedBook(book, listOf(replacement)))
            assertEquals(AppResult.Success(listOf(replacement)), repository.listForBook(book.id))
            assertEquals(AppResult.Success(book), repository.findById(book.id))
        }

    @Test
    fun rejectsMismatchedOrDuplicateTracksWithoutChangingStoredPairing() =
        runTest {
            val book = book("b-1", "Book", null)
            val original = track("t-1", book.id, 0)
            repository.saveImportedBook(book, listOf(original))

            val mismatched = track("t-2", BookId("other-book"), 1)
            val duplicateChapter = track("t-3", book.id, 0)
            val duplicateId = track("t-1", book.id, 1)
            val mismatchResult = repository.saveImportedBook(book, listOf(mismatched))
            val duplicateResult = repository.saveImportedBook(book, listOf(original, duplicateChapter))
            val duplicateIdResult = repository.saveImportedBook(book, listOf(original, duplicateId))

            assertEquals(
                AppResult.Failure(
                    AppError.Validation(
                        "invalid_audio_pairing",
                        "Audio tracks must belong to this book and have unique IDs and chapters.",
                    ),
                ),
                mismatchResult,
            )
            assertIs<AppResult.Failure>(duplicateResult)
            assertIs<AppResult.Failure>(duplicateIdResult)
            assertEquals(AppResult.Success(listOf(original)), repository.listForBook(book.id))
            assertEquals(AppResult.Success(null), repository.findById(BookId("other-book")))
        }

    @Test
    fun migratesLegacyBookSchemaAndPreservesExistingRows() =
        runTest {
            val legacyDriver = JdbcSqliteDriver(JdbcSqliteDriver.IN_MEMORY)
            legacyDriver.execute(
                null,
                """
                CREATE TABLE Book (
                    id TEXT NOT NULL PRIMARY KEY,
                    title TEXT NOT NULL,
                    author TEXT,
                    format TEXT NOT NULL,
                    word_count INTEGER
                )
                """.trimIndent(),
                0,
            )
            legacyDriver.execute(
                null,
                "INSERT INTO Book(id, title, author, format, word_count) VALUES ('old', 'Legacy', NULL, 'EPUB', 12)",
                0,
            )

            GoodReaderDatabase.Schema.migrate(legacyDriver, 1, 2)
            val migratedRepository = SqlDelightLibraryRepository(createGoodReaderDatabase(legacyDriver))

            assertEquals(
                AppResult.Success(
                    Book(BookId("old"), "Legacy", null, BookFormat.EPUB, "legacy://unavailable", 12),
                ),
                migratedRepository.findById(BookId("old")),
            )
            assertEquals(
                AppResult.Success(Unit),
                migratedRepository.saveImportedBook(book("new", "New", null), emptyList()),
            )
            legacyDriver.close()
        }

    private fun book(
        id: String,
        title: String,
        author: String?,
    ) = Book(BookId(id), title, author, BookFormat.EPUB, "file:///$id.epub", 100)

    private fun track(
        id: String,
        bookId: BookId,
        chapterIndex: Int,
    ) = AudioTrack(
        AudioTrackId(id),
        bookId,
        "file:///$id.m4b",
        AudioFormat.M4B,
        60_000,
        chapterIndex,
    )
}
