package com.goodreader.android

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import app.cash.sqldelight.driver.android.AndroidSqliteDriver
import com.goodreader.application.ImportBookUseCase
import com.goodreader.application.SearchLibraryUseCase
import com.goodreader.infrastructure.database.GoodReaderDatabase
import com.goodreader.infrastructure.database.SqlDelightLibraryRepository

/** Entry point for the Android application. */
class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val database =
            GoodReaderDatabase(
                AndroidSqliteDriver(GoodReaderDatabase.Schema, this, "goodreader.db"),
            )
        val repository = SqlDelightLibraryRepository(database)
        val importer = AndroidBookImporter(this, repository)
        setContent {
            GoodReaderTheme {
                GoodReaderNavigation(
                    searchLibrary = SearchLibraryUseCase(repository),
                    importBook = ImportBookUseCase(importer),
                    audioTrackRepository = repository,
                )
            }
        }
    }
}
