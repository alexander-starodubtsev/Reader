package com.goodreader.windows

import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.remember
import androidx.compose.ui.window.Window
import androidx.compose.ui.window.application
import app.cash.sqldelight.driver.jdbc.sqlite.JdbcSqliteDriver
import com.goodreader.application.ImportBookUseCase
import com.goodreader.application.SearchLibraryUseCase
import com.goodreader.infrastructure.database.GoodReaderDatabase
import com.goodreader.infrastructure.database.SqlDelightLibraryRepository
import java.nio.file.Files
import java.nio.file.Path

/** Starts the Windows Compose Desktop application with local SQLDelight storage. */
fun main() {
    val appDirectory = Path.of(System.getProperty("user.home"), "AppData", "Local", "GoodReader")
    Files.createDirectories(appDirectory)
    val databasePath = appDirectory.resolve("goodreader.db")
    val isNewDatabase = Files.notExists(databasePath)
    val driver = JdbcSqliteDriver("jdbc:sqlite:$databasePath")
    if (isNewDatabase) {
        GoodReaderDatabase.Schema.create(driver)
    }
    val repository = SqlDelightLibraryRepository(GoodReaderDatabase(driver))

    application {
        Window(
            onCloseRequest = {
                driver.close()
                exitApplication()
            },
            title = "GoodReader",
        ) {
            val viewModel =
                remember {
                    WindowsLibraryViewModel(
                        SearchLibraryUseCase(repository),
                        ImportBookUseCase(WindowsBookImporter(repository, appDirectory)),
                    )
                }
            DisposableEffect(viewModel) {
                onDispose(viewModel::close)
            }
            GoodReaderTheme {
                GoodReaderNavigation(viewModel, repository)
            }
        }
    }
}
