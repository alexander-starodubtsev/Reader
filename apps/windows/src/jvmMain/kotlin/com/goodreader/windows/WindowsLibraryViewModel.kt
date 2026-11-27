package com.goodreader.windows

import com.goodreader.application.AppError
import com.goodreader.application.AppResult
import com.goodreader.application.AudioImportFile
import com.goodreader.application.BookImportRequest
import com.goodreader.application.ImportBookUseCase
import com.goodreader.application.SearchLibraryUseCase
import com.goodreader.domain.AudioFormat
import com.goodreader.domain.Book
import com.goodreader.domain.BookFormat
import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File

/** State rendered by the Windows library and combined import dialog. */
data class WindowsLibraryState(
    val books: List<Book> = emptyList(),
    val lastOpenedBook: Book? = null,
    val lastImportedBook: Book? = null,
    val query: String = "",
    val importDialogOpen: Boolean = false,
    val selectedBook: File? = null,
    val selectedAudio: List<File> = emptyList(),
    val isImporting: Boolean = false,
    val message: String? = null,
)

/** Coordinates library persistence, search, and imports outside the Compose UI. */
class WindowsLibraryViewModel(
    private val searchLibrary: SearchLibraryUseCase,
    private val importBook: ImportBookUseCase,
    dispatcher: CoroutineDispatcher = Dispatchers.Main,
    private val ioDispatcher: CoroutineDispatcher = Dispatchers.IO,
) {
    private val scope = CoroutineScope(SupervisorJob() + dispatcher)
    private val mutableState = MutableStateFlow(WindowsLibraryState())
    private var searchJob: Job? = null

    /** State observed by the Compose Desktop screen. */
    val state: StateFlow<WindowsLibraryState> = mutableState.asStateFlow()

    init {
        search("")
    }

    /** Replaces the active title/author search and cancels an older query. */
    fun search(query: String) {
        mutableState.update { it.copy(query = query, message = null) }
        searchJob?.cancel()
        searchJob =
            scope.launch {
                when (val result = withContext(ioDispatcher) { searchLibrary(query) }) {
                    is AppResult.Success -> mutableState.update { it.copy(books = result.value) }
                    is AppResult.Failure -> mutableState.update { it.copy(message = result.error.toUserMessage()) }
                }
            }
    }

    /** Opens the single book-and-audio import form. */
    fun openImportDialog() {
        mutableState.update {
            it.copy(
                importDialogOpen = true,
                selectedBook = null,
                selectedAudio = emptyList(),
                message = null,
            )
        }
    }

    /** Closes the import form and clears the selected source-file references. */
    fun closeImportDialog() {
        mutableState.update {
            it.copy(importDialogOpen = false, selectedBook = null, selectedAudio = emptyList(), message = null)
        }
    }

    /** Accepts a supported user-selected book. */
    fun selectBook(file: File) {
        if (BookFormat.fromExtension(file.extension) == null) {
            mutableState.update { it.copy(message = "Choose a PDF, EPUB, or FB2 book file.") }
            return
        }
        mutableState.update { it.copy(selectedBook = file, message = null) }
    }

    /** Accepts optional chapter files in deterministic filename order. */
    fun selectAudio(files: List<File>) {
        if (files.any { AudioFormat.fromExtension(it.extension) == null }) {
            mutableState.update { it.copy(message = "Choose MP3, M4A, M4B, OGG, or WAV audio files.") }
            return
        }
        mutableState.update { it.copy(selectedAudio = files.sortedBy { file -> file.name.lowercase() }, message = null) }
    }

    /** Removes audio selection while retaining the selected book. */
    fun clearAudio() {
        mutableState.update { it.copy(selectedAudio = emptyList(), message = null) }
    }

    /** Acknowledges an import after navigation has opened the resulting book. */
    fun clearLastImportedBook() {
        mutableState.update { it.copy(lastImportedBook = null) }
    }

    /** Restores the most recently opened book independently of the active search filter. */
    fun restoreLastOpenedBook(bookId: String) {
        scope.launch {
            when (val result = withContext(ioDispatcher) { searchLibrary("") }) {
                is AppResult.Success ->
                    mutableState.update {
                        it.copy(lastOpenedBook = result.value.firstOrNull { book -> book.id.value == bookId })
                    }
                is AppResult.Failure ->
                    mutableState.update { it.copy(message = result.error.toUserMessage()) }
            }
        }
    }

    /** Records the book selected for the built-in reader. */
    fun markBookOpened(book: Book) {
        mutableState.update { it.copy(lastOpenedBook = book) }
    }

    /** Imports the selected book and optional audio chapters asynchronously. */
    fun importSelectedFiles() {
        val selected = mutableState.value
        val book = selected.selectedBook
        if (book == null) {
            mutableState.update { it.copy(message = "Choose a PDF, EPUB, or FB2 book file.") }
            return
        }
        scope.launch {
            mutableState.update { it.copy(isImporting = true, message = null) }
            val request =
                BookImportRequest(
                    bookUri = book.absolutePath,
                    bookExtension = book.extension,
                    audioFiles =
                        selected.selectedAudio.mapIndexed { index, file ->
                            AudioImportFile(file.absolutePath, file.extension, index)
                        },
                )
            when (val result = withContext(ioDispatcher) { importBook(request) }) {
                is AppResult.Success -> {
                    val query = mutableState.value.query
                    val refresh = withContext(ioDispatcher) { searchLibrary(query) }
                    val refreshedBooks =
                        when (refresh) {
                            is AppResult.Success -> refresh.value
                            is AppResult.Failure -> mutableState.value.books
                        }
                    val message =
                        when (refresh) {
                            is AppResult.Success -> "Added ${result.value.book.title} to your library."
                            is AppResult.Failure ->
                                "Added ${result.value.book.title}, but the library could not refresh: " +
                                    refresh.error.toUserMessage()
                        }
                    mutableState.update {
                        it.copy(
                            books = refreshedBooks,
                            lastImportedBook = result.value.book,
                            importDialogOpen = false,
                            selectedBook = null,
                            selectedAudio = emptyList(),
                            isImporting = false,
                            message = message,
                        )
                    }
                }
                is AppResult.Failure ->
                    mutableState.update { it.copy(isImporting = false, message = result.error.toUserMessage()) }
            }
        }
    }

    /** Cancels outstanding work when the desktop window is disposed. */
    fun close() {
        scope.cancel()
    }

    private fun AppError.toUserMessage(): String =
        when (this) {
            is AppError.Validation -> detail
            is AppError.NotFound -> "The requested $entity is no longer available."
            is AppError.Unavailable -> detail
            AppError.Cancelled -> "The operation was cancelled."
            is AppError.Unexpected -> "The operation could not be completed."
        }
}
