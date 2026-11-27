package com.goodreader.android

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.NavigationRail
import androidx.compose.material3.NavigationRailItem
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.saveable.rememberSaveableStateHolder
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import com.goodreader.application.AppResult
import com.goodreader.application.AudioTrackRepository
import com.goodreader.application.ImportBookUseCase
import com.goodreader.application.SearchLibraryUseCase
import com.goodreader.domain.Book
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

private enum class GoodReaderDestination(
    val labelResource: Int,
) {
    Home(R.string.tab_home),
    Library(R.string.tab_library),
    AddBook(R.string.tab_add_book),
    Profile(R.string.tab_profile),
}

@Composable
@Suppress("FunctionNaming", "ktlint:standard:function-naming")
fun GoodReaderNavigation(
    searchLibrary: SearchLibraryUseCase,
    importBook: ImportBookUseCase,
    audioTrackRepository: AudioTrackRepository? = null,
) {
    val context = LocalContext.current
    val preferences =
        remember {
            context.getSharedPreferences("goodreader.reader", android.content.Context.MODE_PRIVATE)
        }
    var selected by rememberSaveable { mutableStateOf(GoodReaderDestination.Home) }
    var openedBook by remember { mutableStateOf<Book?>(null) }
    var lastOpenedBook by remember { mutableStateOf<Book?>(null) }
    val stateHolder = rememberSaveableStateHolder()

    LaunchedEffect(searchLibrary) {
        val savedId = preferences.getString("lastOpenedBookId", null) ?: return@LaunchedEffect
        val result = withContext(Dispatchers.IO) { searchLibrary("") }
        if (result is AppResult.Success) {
            lastOpenedBook = result.value.firstOrNull { it.id.value == savedId }
        }
    }
    val openBook: (Book) -> Unit = { book ->
        openedBook = book
        lastOpenedBook = book
        preferences.edit().putString("lastOpenedBookId", book.id.value).apply()
    }

    BoxWithConstraints(Modifier.fillMaxSize()) {
        val compact = maxWidth < 700.dp
        val readerBook = openedBook
        if (readerBook != null) {
            ReaderScreen(
                book = readerBook,
                onClose = { openedBook = null },
                audioTrackRepository = audioTrackRepository,
            )
        } else {
            Scaffold(
                containerColor = MaterialTheme.colorScheme.background,
                bottomBar = {
                    if (compact) {
                        NavigationBar {
                            GoodReaderDestination.entries.forEach { destination ->
                                NavigationBarItem(
                                    selected = selected == destination,
                                    onClick = { selected = destination },
                                    icon = {},
                                    label = { Text(stringResource(destination.labelResource)) },
                                )
                            }
                        }
                    }
                },
            ) { contentPadding ->
                Row(Modifier.fillMaxSize().padding(contentPadding)) {
                    if (!compact) {
                        NavigationRail {
                            GoodReaderDestination.entries.forEach { destination ->
                                NavigationRailItem(
                                    selected = selected == destination,
                                    onClick = { selected = destination },
                                    icon = {},
                                    label = { Text(stringResource(destination.labelResource)) },
                                    alwaysShowLabel = true,
                                )
                            }
                        }
                    }
                    Box(Modifier.weight(1f).fillMaxHeight()) {
                        stateHolder.SaveableStateProvider(selected.name) {
                            when (selected) {
                                GoodReaderDestination.Home ->
                                    HomeScreen(lastOpenedBook, openBook)
                                GoodReaderDestination.Library ->
                                    LibraryScreen(
                                        searchLibrary,
                                        importBook,
                                        onOpenBook = openBook,
                                        onBookImported = openBook,
                                    )
                                GoodReaderDestination.AddBook ->
                                    LibraryScreen(
                                        searchLibrary,
                                        importBook,
                                        startInAddBook = true,
                                        onOpenBook = openBook,
                                        onBookImported = openBook,
                                    )
                                GoodReaderDestination.Profile -> ProfileScreen()
                            }
                        }
                    }
                }
            }
        }
    }
}

@Composable
@Suppress("FunctionNaming", "ktlint:standard:function-naming")
private fun HomeScreen(
    lastOpenedBook: Book?,
    onOpenBook: (Book) -> Unit,
) {
    Column(
        modifier = Modifier.fillMaxSize().padding(GoodReaderSpacing.large),
        verticalArrangement = Arrangement.Center,
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        if (lastOpenedBook == null) {
            Text(
                stringResource(R.string.home_empty_description),
                style = MaterialTheme.typography.bodyMedium,
            )
        } else {
            androidx.compose.material3.Card {
                Column(
                    modifier = Modifier.padding(GoodReaderSpacing.large),
                    verticalArrangement = Arrangement.spacedBy(GoodReaderSpacing.medium),
                ) {
                    Text(lastOpenedBook.title, style = MaterialTheme.typography.headlineMedium)
                    lastOpenedBook.author?.let { Text(it, style = MaterialTheme.typography.bodyMedium) }
                    androidx.compose.material3.Button(onClick = { onOpenBook(lastOpenedBook) }) {
                        Text(stringResource(R.string.reader_continue_reading))
                    }
                }
            }
        }
    }
}

@Composable
@Suppress("FunctionNaming", "ktlint:standard:function-naming")
private fun ProfileScreen() {
    Box(
        modifier = Modifier.fillMaxSize().padding(GoodReaderSpacing.large),
        contentAlignment = Alignment.Center,
    ) {
        androidx.compose.material3.Card {
            Column(
                modifier = Modifier.padding(GoodReaderSpacing.large),
                verticalArrangement = Arrangement.spacedBy(GoodReaderSpacing.medium),
            ) {
                Text(stringResource(R.string.profile_empty_title), style = MaterialTheme.typography.headlineMedium)
                Text(stringResource(R.string.profile_empty_description), style = MaterialTheme.typography.bodyMedium)
            }
        }
    }
}
