package com.goodreader.android

import androidx.compose.material3.MaterialTheme
import androidx.compose.ui.test.assertCountEquals
import androidx.compose.ui.test.assertIsDisplayed
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onAllNodesWithText
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performTextInput
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.goodreader.application.AppResult
import com.goodreader.application.BookImportRequest
import com.goodreader.application.BookImporter
import com.goodreader.application.BookRepository
import com.goodreader.application.ImportBookUseCase
import com.goodreader.application.ImportedBook
import com.goodreader.application.SearchLibraryUseCase
import com.goodreader.domain.Book
import com.goodreader.domain.BookId
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class LibraryScreenTest {
    @get:Rule
    val composeRule = createComposeRule()

    @Test
    fun addBookTabShowsTheCombinedBookAndOptionalAudioForm() {
        composeRule.setContent {
            MaterialTheme {
                LibraryScreen(
                    searchLibrary = SearchLibraryUseCase(EmptyBookRepository()),
                    importBook = ImportBookUseCase(RejectingImporter()),
                    startInAddBook = true,
                )
            }
        }

        composeRule.onNodeWithText("Choose PDF, EPUB, or FB2").assertIsDisplayed()
        composeRule.onNodeWithText("Add audio chapters (optional)").assertIsDisplayed()
        composeRule.onNodeWithText("Add to library").assertIsDisplayed()
    }

    @Test
    fun homeShowsOnlyTheEmptyMessageAndTheNavigationTab() {
        composeRule.setContent {
            GoodReaderTheme {
                GoodReaderNavigation(
                    searchLibrary = SearchLibraryUseCase(EmptyBookRepository()),
                    importBook = ImportBookUseCase(RejectingImporter()),
                )
            }
        }

        composeRule.onNodeWithText("No book has been opened yet.").assertIsDisplayed()
        composeRule.onAllNodesWithText("Add book").assertCountEquals(1)
    }

    @Test
    fun navigationRestoresLibrarySearchWhenSwitchingTabs() {
        composeRule.setContent {
            GoodReaderTheme {
                GoodReaderNavigation(
                    searchLibrary = SearchLibraryUseCase(EmptyBookRepository()),
                    importBook = ImportBookUseCase(RejectingImporter()),
                )
            }
        }

        composeRule.onNodeWithText("Library").performClick()
        composeRule.onNodeWithText("Search books").performTextInput("quiet river")
        composeRule.onNodeWithText("Profile").performClick()
        composeRule.onNodeWithText("Home").performClick()
        composeRule.onNodeWithText("Library").performClick()

        composeRule.onNodeWithText("quiet river").assertIsDisplayed()
    }

    private class EmptyBookRepository : BookRepository {
        override suspend fun findById(id: BookId): AppResult<Book?> = AppResult.Success(null)

        override suspend fun search(query: String?): AppResult<List<Book>> = AppResult.Success(emptyList())

        override suspend fun save(book: Book): AppResult<Unit> = AppResult.Success(Unit)
    }

    private class RejectingImporter : BookImporter {
        override suspend fun import(request: BookImportRequest): AppResult<ImportedBook> =
            error("The picker is not invoked in this UI test.")
    }
}
