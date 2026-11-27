@file:Suppress("TooManyFunctions")

package com.goodreader.android

import android.content.Intent
import android.net.Uri
import android.provider.OpenableColumns
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.unit.dp
import androidx.lifecycle.ViewModel
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.viewModelScope
import androidx.lifecycle.viewmodel.compose.viewModel
import androidx.lifecycle.viewmodel.initializer
import androidx.lifecycle.viewmodel.viewModelFactory
import com.goodreader.application.AppError
import com.goodreader.application.AppResult
import com.goodreader.application.BookImportRequest
import com.goodreader.application.ImportBookUseCase
import com.goodreader.application.SearchLibraryUseCase
import com.goodreader.domain.AudioFormat
import com.goodreader.domain.Book
import com.goodreader.domain.BookFormat
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

/** A user-selected file with the name shown in the import dialog. */
data class SelectedImportFile(
    val uri: String,
    val displayName: String,
    val extension: String,
)

/** Product name shown in the library header. */
const val LIBRARY_TITLE = "GoodReader"

/** UI state for the local library and combined book/audio import dialog. */
data class LibraryUiState(
    val books: List<Book> = emptyList(),
    val lastImportedBook: Book? = null,
    val query: String = "",
    val isImportDialogOpen: Boolean = false,
    val selectedBook: SelectedImportFile? = null,
    val selectedAudio: List<SelectedImportFile> = emptyList(),
    val isImporting: Boolean = false,
    val message: String? = null,
)

/** Coordinates library search and import work away from the Compose view. */
class LibraryViewModel(
    private val searchLibrary: SearchLibraryUseCase,
    private val importBook: ImportBookUseCase,
) : ViewModel() {
    private val mutableState = MutableStateFlow(LibraryUiState())
    private var searchJob: Job? = null

    /** Observable state consumed by the library screen. */
    val state: StateFlow<LibraryUiState> = mutableState.asStateFlow()

    init {
        search("")
    }

    /** Searches the persisted library using the supplied title or author query. */
    fun search(query: String) {
        mutableState.update { it.copy(query = query, message = null) }
        searchJob?.cancel()
        searchJob =
            viewModelScope.launch {
                when (val result = withContext(Dispatchers.IO) { searchLibrary(query) }) {
                    is AppResult.Success -> mutableState.update { it.copy(books = result.value) }
                    is AppResult.Failure -> mutableState.update { it.copy(message = result.error.toUserMessage()) }
                }
            }
    }

    /** Opens the single import form. */
    fun openImportDialog() {
        mutableState.update {
            it.copy(
                isImportDialogOpen = true,
                selectedBook = null,
                selectedAudio = emptyList(),
                message = null,
            )
        }
    }

    /** Closes the import form without starting an import. */
    fun closeImportDialog() {
        mutableState.update { it.copy(isImportDialogOpen = false, message = null) }
    }

    /** Adds a selected book document to the import form. */
    fun selectBook(file: SelectedImportFile) {
        if (BookFormat.fromExtension(file.extension) == null) {
            mutableState.update { it.copy(message = "Choose a PDF, EPUB, or FB2 book file.") }
            return
        }
        mutableState.update { it.copy(selectedBook = file, message = null) }
    }

    /** Replaces the optional chapter-file selection. */
    fun selectAudio(files: List<SelectedImportFile>) {
        if (files.any { AudioFormat.fromExtension(it.extension) == null }) {
            mutableState.update { it.copy(message = "Choose MP3, M4A, M4B, OGG, or WAV audio files.") }
            return
        }
        mutableState.update { it.copy(selectedAudio = files, message = null) }
    }

    /** Displays a file-picker or permission error. */
    fun reportSelectionError(message: String) {
        mutableState.update { it.copy(message = message) }
    }

    /** Clears the one-shot import result after navigation has handled it. */
    fun clearLastImportedBook() {
        mutableState.update { it.copy(lastImportedBook = null) }
    }

    /** Validates and imports the currently selected book and optional audio chapters. */
    fun importSelectedFiles() {
        val selected = mutableState.value
        val book = selected.selectedBook
        if (book == null) {
            mutableState.update { it.copy(message = "Choose a PDF, EPUB, or FB2 book file.") }
            return
        }
        viewModelScope.launch {
            mutableState.update { it.copy(isImporting = true, message = null) }
            val request =
                BookImportRequest(
                    bookUri = book.uri,
                    bookExtension = book.extension,
                    audioFiles =
                        selected.selectedAudio.mapIndexed { index, file ->
                            com.goodreader.application.AudioImportFile(file.uri, file.extension, index)
                        },
                )
            when (val result = withContext(Dispatchers.IO) { importBook(request) }) {
                is AppResult.Success -> {
                    val importedBook = result.value.book
                    searchJob?.cancel()
                    val query = mutableState.value.query
                    val refreshResult = withContext(Dispatchers.IO) { searchLibrary(query) }
                    val refreshedBooks =
                        when (refreshResult) {
                            is AppResult.Success -> refreshResult.value
                            is AppResult.Failure -> mutableState.value.books
                        }
                    val refreshMessage =
                        when (refreshResult) {
                            is AppResult.Success -> "Added ${result.value.book.title} to your library."
                            is AppResult.Failure ->
                                "Added ${result.value.book.title}, but the library could not refresh: " +
                                    refreshResult.error.toUserMessage()
                        }
                    mutableState.update {
                        it.copy(
                            books = refreshedBooks,
                            lastImportedBook = importedBook,
                            isImportDialogOpen = false,
                            selectedBook = null,
                            selectedAudio = emptyList(),
                            isImporting = false,
                            message = refreshMessage,
                        )
                    }
                }
                is AppResult.Failure -> {
                    mutableState.update { it.copy(isImporting = false, message = result.error.toUserMessage()) }
                }
            }
        }
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

/** Displays the searchable library and combined book/audio import form. */
@Composable
@Suppress("FunctionNaming", "ktlint:standard:function-naming")
fun LibraryScreen(
    searchLibrary: SearchLibraryUseCase,
    importBook: ImportBookUseCase,
    startInAddBook: Boolean = false,
    onOpenBook: (Book) -> Unit = {},
    onBookImported: (Book) -> Unit = {},
) {
    val factory =
        viewModelFactory {
            initializer {
                LibraryViewModel(searchLibrary, importBook)
            }
        }
    val viewModel: LibraryViewModel = viewModel(factory = factory)
    val state by viewModel.state.collectAsStateWithLifecycle()
    val context = LocalContext.current

    LaunchedEffect(state.lastImportedBook?.id?.value) {
        state.lastImportedBook?.let {
            onBookImported(it)
            viewModel.clearLastImportedBook()
        }
    }

    LaunchedEffect(startInAddBook) {
        if (startInAddBook) viewModel.openImportDialog()
    }

    val bookPicker =
        rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
            if (uri != null) {
                val file = context.persistSelection(uri)
                if (file == null) {
                    viewModel.reportSelectionError("The selected book cannot be kept for later access.")
                } else {
                    viewModel.selectBook(file)
                }
            }
        }
    val audioPicker =
        rememberLauncherForActivityResult(ActivityResultContracts.OpenMultipleDocuments()) { uris ->
            val files = uris.mapNotNull(context::persistSelection)
            if (files.size == uris.size) {
                viewModel.selectAudio(files)
            } else {
                viewModel.reportSelectionError("One or more selected audio files cannot be kept for later access.")
            }
        }

    LibraryContent(
        state = state,
        showImportAction = startInAddBook,
        onSearch = viewModel::search,
        onAddBook = viewModel::openImportDialog,
        onDismissImport = viewModel::closeImportDialog,
        onImport = viewModel::importSelectedFiles,
        onPickBook = { bookPicker.launch(arrayOf("*/*")) },
        onPickAudio = { audioPicker.launch(arrayOf("*/*")) },
        onOpenBook = onOpenBook,
    )
}

@Composable
@Suppress("FunctionNaming", "ktlint:standard:function-naming")
private fun LibraryContent(
    state: LibraryUiState,
    showImportAction: Boolean,
    onSearch: (String) -> Unit,
    onAddBook: () -> Unit,
    onDismissImport: () -> Unit,
    onImport: () -> Unit,
    onPickBook: () -> Unit,
    onPickAudio: () -> Unit,
    onOpenBook: (Book) -> Unit,
) {
    Surface(
        modifier = Modifier.fillMaxSize(),
        color = MaterialTheme.colorScheme.background,
    ) {
        Column(
            modifier = Modifier.fillMaxSize().padding(20.dp),
            verticalArrangement = Arrangement.spacedBy(14.dp),
        ) {
            LibraryHeader(onAddBook, showImportAction)
            OutlinedTextField(
                value = state.query,
                onValueChange = onSearch,
                label = { Text("Search books") },
                modifier = Modifier.fillMaxWidth(),
                singleLine = true,
            )
            state.message?.let { Text(it, color = MaterialTheme.colorScheme.error) }
            LibraryBookList(state.books, state.query, onOpenBook)
        }
    }

    if (state.isImportDialogOpen) {
        BookImportDialog(
            state = state,
            onDismiss = onDismissImport,
            onImport = onImport,
            onPickBook = onPickBook,
            onPickAudio = onPickAudio,
        )
    }
}

@Composable
@Suppress("FunctionNaming", "ktlint:standard:function-naming")
private fun LibraryHeader(
    onAddBook: () -> Unit,
    showImportAction: Boolean,
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.SpaceBetween,
    ) {
        Text(LIBRARY_TITLE, style = MaterialTheme.typography.headlineMedium)
        if (showImportAction) {
            Button(onClick = onAddBook) { Text("Add book") }
        }
    }
}

@Composable
@Suppress("FunctionNaming", "ktlint:standard:function-naming")
private fun LibraryBookList(
    books: List<Book>,
    query: String,
    onOpenBook: (Book) -> Unit,
) {
    if (books.isEmpty()) {
        val message =
            if (query.isBlank()) {
                "Your library is empty. Add a book to get started."
            } else {
                "No books match your search."
            }
        Text(message)
    } else {
        LazyColumn(verticalArrangement = Arrangement.spacedBy(8.dp)) {
            items(books, key = { it.id.value }) { book ->
                Column(
                    modifier =
                        Modifier
                            .fillMaxWidth()
                            .clickable(role = Role.Button) { onOpenBook(book) }
                            .padding(vertical = 8.dp),
                ) {
                    Text(book.title, style = MaterialTheme.typography.titleMedium)
                    Text(
                        listOfNotNull(book.author, book.format.name).joinToString(" · "),
                        style = MaterialTheme.typography.bodyMedium,
                    )
                }
            }
        }
    }
}

@Composable
@Suppress("FunctionNaming", "ktlint:standard:function-naming")
private fun BookImportDialog(
    state: LibraryUiState,
    onDismiss: () -> Unit,
    onImport: () -> Unit,
    onPickBook: () -> Unit,
    onPickAudio: () -> Unit,
) {
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Add a book") },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
                OutlinedButton(onClick = onPickBook, enabled = !state.isImporting) {
                    Text(state.selectedBook?.displayName ?: "Choose PDF, EPUB, or FB2")
                }
                OutlinedButton(onClick = onPickAudio, enabled = !state.isImporting) {
                    Text(
                        if (state.selectedAudio.isEmpty()) {
                            "Add audio chapters (optional)"
                        } else {
                            "${state.selectedAudio.size} audio file(s) selected"
                        },
                    )
                }
                state.selectedAudio.forEach { Text(it.displayName, style = MaterialTheme.typography.bodySmall) }
                if (state.isImporting) CircularProgressIndicator()
                state.message?.let { Text(it, color = MaterialTheme.colorScheme.error) }
            }
        },
        confirmButton = {
            Button(onClick = onImport, enabled = state.selectedBook != null && !state.isImporting) {
                Text(if (state.isImporting) "Importing..." else "Add to library")
            }
        },
        dismissButton = {
            TextButton(onClick = onDismiss, enabled = !state.isImporting) {
                Text("Cancel")
            }
        },
    )
}

private fun android.content.Context.persistSelection(uri: Uri): SelectedImportFile? =
    try {
        contentResolver.takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION)
        val name = documentName(uri) ?: uri.lastPathSegment?.substringAfterLast('/') ?: return null
        SelectedImportFile(uri.toString(), name, name.substringAfterLast('.', ""))
    } catch (_: SecurityException) {
        null
    }

private fun android.content.Context.documentName(uri: Uri): String? =
    contentResolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)?.use { cursor ->
        if (!cursor.moveToFirst()) {
            null
        } else {
            val index = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
            if (index >= 0) cursor.getString(index) else null
        }
    }
