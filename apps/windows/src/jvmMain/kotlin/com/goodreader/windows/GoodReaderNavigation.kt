package com.goodreader.windows

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.NavigationRail
import androidx.compose.material3.NavigationRailItem
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.SideEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveableStateHolder
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import com.goodreader.application.AudioTrackRepository
import com.goodreader.domain.Book
import java.util.ResourceBundle
import java.util.prefs.Preferences

private enum class GoodReaderDestination(
    val key: String,
    val labelKey: String,
) {
    Home("home", "tab.home"),
    Library("library", "tab.library"),
    AddBook("add-book", "tab.addBook"),
    Profile("profile", "tab.profile"),
}

private val strings: ResourceBundle
    get() = ResourceBundle.getBundle("com.goodreader.windows.Messages")

internal fun text(key: String): String = strings.getString(key)

@Composable
@Suppress("FunctionNaming", "ktlint:standard:function-naming")
fun GoodReaderNavigation(
    viewModel: WindowsLibraryViewModel,
    audioTrackRepository: AudioTrackRepository? = null,
) {
    val preferences = remember { Preferences.userRoot().node("com/goodreader/navigation") }
    val readerPreferences = remember { Preferences.userRoot().node("com/goodreader/reader") }
    var selected by remember {
        mutableStateOf(
            GoodReaderDestination.entries.firstOrNull { it.key == preferences.get("selectedTab", "home") }
                ?: GoodReaderDestination.Home,
        )
    }
    var openedBook by remember { mutableStateOf<Book?>(null) }
    val libraryState by viewModel.state.collectAsState()
    val lastOpenedBook = libraryState.lastOpenedBook
    androidx.compose.runtime.LaunchedEffect(viewModel) {
        val savedBookId = readerPreferences.get("lastOpenedBook", "")
        if (savedBookId.isNotBlank()) viewModel.restoreLastOpenedBook(savedBookId)
    }
    val stateHolder = rememberSaveableStateHolder()
    SideEffect { preferences.put("selectedTab", selected.key) }

    val activeBook = openedBook
    if (activeBook != null) {
        WindowsBookReaderScreen(activeBook, audioTrackRepository) { openedBook = null }
    } else {
        Row(
            Modifier
                .fillMaxSize()
                .background(MaterialTheme.colorScheme.background)
                .padding(GoodReaderSpacing.medium),
        ) {
            NavigationRail {
                GoodReaderDestination.entries.forEach { destination ->
                    NavigationRailItem(
                        selected = selected == destination,
                        onClick = { selected = destination },
                        icon = {},
                        label = { Text(text(destination.labelKey)) },
                        alwaysShowLabel = true,
                    )
                }
            }
            stateHolder.SaveableStateProvider(selected.key) {
                when (selected) {
                    GoodReaderDestination.Home ->
                        HomeScreen(lastOpenedBook) {
                            openBook(it, readerPreferences, viewModel) { openedBook = it }
                        }
                    GoodReaderDestination.Library ->
                        LibraryScreen(viewModel) {
                            openBook(it, readerPreferences, viewModel) { openedBook = it }
                        }
                    GoodReaderDestination.AddBook ->
                        LibraryScreen(
                            viewModel,
                            startInAddBook = true,
                        ) {
                            openBook(it, readerPreferences, viewModel) { openedBook = it }
                        }
                    GoodReaderDestination.Profile ->
                        EmptyNavigationScreen(
                            title = text("profile.empty.title"),
                            description = text("profile.empty.description"),
                        )
                }
            }
        }
    }
}

private fun openBook(
    book: Book,
    preferences: Preferences,
    viewModel: WindowsLibraryViewModel,
    showReader: (Book) -> Unit,
) {
    preferences.put("lastOpenedBook", book.id.value)
    viewModel.markBookOpened(book)
    showReader(book)
}

@Composable
@Suppress("FunctionNaming", "ktlint:standard:function-naming")
private fun HomeScreen(
    lastOpenedBook: Book?,
    onOpenBook: (Book) -> Unit,
) {
    androidx.compose.foundation.layout.Box(
        modifier = Modifier.fillMaxSize().padding(GoodReaderSpacing.large),
        contentAlignment = Alignment.Center,
    ) {
        if (lastOpenedBook == null) {
            Text(text("home.empty.description"), style = MaterialTheme.typography.bodyMedium)
        } else {
            Card {
                Column(
                    modifier = Modifier.padding(GoodReaderSpacing.large),
                    verticalArrangement = Arrangement.spacedBy(GoodReaderSpacing.medium),
                ) {
                    Text(lastOpenedBook.title, style = MaterialTheme.typography.headlineMedium)
                    lastOpenedBook.author?.let { Text(it, style = MaterialTheme.typography.bodyMedium) }
                    Button(onClick = { onOpenBook(lastOpenedBook) }) {
                        Text(text("home.continue"))
                    }
                }
            }
        }
    }
}

@Composable
@Suppress("FunctionNaming", "ktlint:standard:function-naming")
private fun EmptyNavigationScreen(
    title: String,
    description: String,
) {
    Column(
        modifier = Modifier.fillMaxSize().padding(GoodReaderSpacing.large),
        verticalArrangement = Arrangement.Center,
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Card {
            Column(
                modifier = Modifier.padding(GoodReaderSpacing.large),
                verticalArrangement = Arrangement.spacedBy(GoodReaderSpacing.medium),
            ) {
                Text(title, style = MaterialTheme.typography.headlineMedium)
                Text(description, style = MaterialTheme.typography.bodyMedium)
            }
        }
    }
}
