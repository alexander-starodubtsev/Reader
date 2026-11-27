package com.goodreader.android

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import java.io.File

@RunWith(AndroidJUnit4::class)
class NativeCoreTest {
    @Test
    fun validatesLocalBookPathsThroughTheCppCore() {
        val context = InstrumentationRegistry.getInstrumentation().targetContext
        val book = File(context.cacheDir, "測試-book.epub").apply { writeText("book") }
        val unsupported = File(context.cacheDir, "unsupported.txt").apply { writeText("data") }
        val missing = File(context.cacheDir, "missing.pdf")

        assertEquals(
            NativeCore.STATUS_OK,
            NativeCore.validateBookFile(book.absolutePath.toByteArray(Charsets.UTF_8)),
        )
        assertEquals(
            NativeCore.STATUS_UNSUPPORTED_FORMAT,
            NativeCore.validateBookFile(unsupported.absolutePath.toByteArray(Charsets.UTF_8)),
        )
        assertEquals(
            NativeCore.STATUS_NOT_FOUND,
            NativeCore.validateBookFile(missing.absolutePath.toByteArray(Charsets.UTF_8)),
        )

        book.delete()
        unsupported.delete()
    }
}
