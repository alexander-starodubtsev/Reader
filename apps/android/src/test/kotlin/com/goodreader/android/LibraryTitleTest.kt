package com.goodreader.android

import kotlin.test.Test
import kotlin.test.assertEquals

class LibraryTitleTest {
    @Test
    fun exposesTheApplicationTitle() {
        assertEquals("GoodReader", LIBRARY_TITLE)
    }
}
