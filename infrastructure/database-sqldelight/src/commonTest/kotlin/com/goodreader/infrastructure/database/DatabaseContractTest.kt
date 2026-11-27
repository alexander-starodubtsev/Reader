package com.goodreader.infrastructure.database

import kotlin.test.Test
import kotlin.test.assertEquals

class DatabaseContractTest {
    @Test
    fun startsAtTheInitialSchemaVersion() {
        assertEquals(1, DatabaseContract.INITIAL_SCHEMA_VERSION)
    }
}
