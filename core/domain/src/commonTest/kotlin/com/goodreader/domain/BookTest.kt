package com.goodreader.domain

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith

class BookTest {
    @Test
    fun parsesFormatsWithoutCaseSensitivity() {
        assertEquals(BookFormat.EPUB, BookFormat.fromExtension(".Epub"))
        assertEquals(AudioFormat.M4B, AudioFormat.fromExtension("m4b"))
        assertEquals(null, BookFormat.fromExtension("txt"))
    }

    @Test
    fun rejectsInvalidBookAndTrackMetadata() {
        assertFailsWith<IllegalArgumentException> {
            Book(BookId("book-1"), " ", null, BookFormat.PDF, "file:///book.pdf", null)
        }
        assertFailsWith<IllegalArgumentException> {
            Book(BookId("book-1"), "Title", null, BookFormat.PDF, "file:///book.pdf", -1)
        }
        assertFailsWith<IllegalArgumentException> {
            AudioTrack(AudioTrackId("audio-1"), BookId("book-1"), "file.m4b", AudioFormat.M4B, 0, 0)
        }
        assertFailsWith<IllegalArgumentException> { BookId(" ") }
        assertFailsWith<IllegalArgumentException> { AudioTrackId("") }
        assertFailsWith<IllegalArgumentException> {
            AudioTrack(AudioTrackId("audio"), BookId("book"), "", AudioFormat.MP3, 10, 0)
        }
        assertFailsWith<IllegalArgumentException> {
            AudioTrack(AudioTrackId("audio"), BookId("book"), "track.mp3", AudioFormat.MP3, 10, -1)
        }
        assertFailsWith<IllegalArgumentException> {
            Book(BookId("book-1"), "Title", null, BookFormat.PDF, " ", 10)
        }
    }
}
