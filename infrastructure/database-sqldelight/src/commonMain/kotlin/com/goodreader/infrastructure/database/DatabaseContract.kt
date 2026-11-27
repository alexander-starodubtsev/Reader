package com.goodreader.infrastructure.database

/** Database schema generation used by migrations and repository adapters. */
object DatabaseContract {
    /** Initial schema version; subsequent changes must add a SQLDelight migration. */
    const val INITIAL_SCHEMA_VERSION: Int = 1
}
