package com.goodreader.windows

import kotlin.test.Test
import kotlin.test.assertEquals

class WindowsLibraryStateTest {
    @Test
    fun startsWithAnEmptyLibrarySearch() {
        val state = WindowsLibraryState()
        assertEquals("", state.query)
        assertEquals(emptyList(), state.books)
    }
}
