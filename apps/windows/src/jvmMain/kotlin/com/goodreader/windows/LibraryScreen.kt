package com.goodreader.windows

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
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.goodreader.domain.Book
import java.io.File
import javax.swing.JFileChooser
import javax.swing.filechooser.FileNameExtensionFilter

/** Displays the searchable Windows library and its combined book/audio import form. */
@Composable
@Suppress("FunctionNaming", "ktlint:standard:function-naming")
fun LibraryScreen(
    viewModel: WindowsLibraryViewModel,
    startInAddBook: Boolean = false,
    onOpenBook: (Book) -> Unit = {},
) {
    val state by viewModel.state.collectAsState()
    LaunchedEffect(startInAddBook) {
        if (startInAddBook) viewModel.openImportDialog()
    }
    LaunchedEffect(state.lastImportedBook?.id?.value) {
        state.lastImportedBook?.let {
            onOpenBook(it)
            viewModel.clearLastImportedBook()
        }
    }
    Surface(modifier = Modifier.fillMaxSize(), color = MaterialTheme.colorScheme.background) {
        Column(
            modifier = Modifier.fillMaxSize().padding(24.dp),
            verticalArrangement = Arrangement.spacedBy(14.dp),
        ) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
            ) {
                Text("GoodReader", style = MaterialTheme.typography.headlineMedium)
                if (startInAddBook) {
                    Button(onClick = viewModel::openImportDialog) { Text("Add book") }
                }
            }
            OutlinedTextField(
                value = state.query,
                onValueChange = viewModel::search,
                label = { Text("Search books") },
                modifier = Modifier.fillMaxWidth(),
                singleLine = true,
            )
            state.message?.let { Text(it, color = MaterialTheme.colorScheme.error) }
            if (state.books.isEmpty()) {
                Text(if (state.query.isBlank()) "Your library is empty. Add a book to get started." else "No books match your search.")
            } else {
                LazyColumn(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    items(state.books, key = { it.id.value }) { book ->
                        Column(
                            modifier =
                                Modifier
                                    .fillMaxWidth()
                                    .padding(vertical = 8.dp)
                                    .clickable { onOpenBook(book) },
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
    }
    if (state.importDialogOpen) {
        AlertDialog(
            onDismissRequest = {
                if (!state.isImporting) viewModel.closeImportDialog()
            },
            title = { Text("Add a book") },
            text = {
                Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
                    OutlinedButton(
                        onClick = {
                            chooseFiles("Books", listOf("pdf", "epub", "fb2"), false).firstOrNull()?.let(viewModel::selectBook)
                        },
                        enabled = !state.isImporting,
                    ) {
                        Text(state.selectedBook?.name ?: "Choose PDF, EPUB, or FB2")
                    }
                    OutlinedButton(
                        onClick = {
                            viewModel.selectAudio(chooseFiles("Audio", listOf("mp3", "m4a", "m4b", "ogg", "wav"), true))
                        },
                        enabled = !state.isImporting,
                    ) {
                        Text(
                            if (state.selectedAudio.isEmpty()) {
                                "Add audio chapters (optional)"
                            } else {
                                "${state.selectedAudio.size} audio file(s) selected"
                            },
                        )
                    }
                    if (state.selectedAudio.isNotEmpty()) {
                        TextButton(onClick = viewModel::clearAudio, enabled = !state.isImporting) {
                            Text("Clear audio selection")
                        }
                    }
                    state.message?.let { Text(it, color = MaterialTheme.colorScheme.error) }
                }
            },
            confirmButton = {
                Button(
                    onClick = viewModel::importSelectedFiles,
                    enabled = !state.isImporting,
                ) {
                    if (state.isImporting) {
                        CircularProgressIndicator()
                    } else {
                        Text("Add to library")
                    }
                }
            },
            dismissButton = {
                TextButton(
                    onClick = viewModel::closeImportDialog,
                    enabled = !state.isImporting,
                ) {
                    Text("Cancel")
                }
            },
        )
    }
}

private fun chooseFiles(
    description: String,
    extensions: List<String>,
    multiple: Boolean,
): List<File> {
    val chooser =
        JFileChooser().apply {
            dialogTitle = "Choose $description"
            isMultiSelectionEnabled = multiple
            fileFilter = FileNameExtensionFilter(description, *extensions.toTypedArray())
        }
    if (chooser.showOpenDialog(null) != JFileChooser.APPROVE_OPTION) return emptyList()
    return if (multiple) chooser.selectedFiles.toList() else listOfNotNull(chooser.selectedFile)
}
