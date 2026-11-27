package com.goodreader.domain

import kotlinx.datetime.Instant
import kotlin.test.Test
import kotlin.test.assertFailsWith

class ProgressTest {
    @Test
    fun rejectsNegativePositions() {
        assertFailsWith<IllegalArgumentException> {
            ReadingProgress(BookId("book"), -1, Instant.fromEpochMilliseconds(0))
        }
        assertFailsWith<IllegalArgumentException> {
            ListeningProgress(
                BookId("book"),
                AudioTrackId("track"),
                -1,
                Instant.fromEpochMilliseconds(0),
            )
        }
        assertFailsWith<IllegalArgumentException> { WordOffset(-1) }
    }
}
