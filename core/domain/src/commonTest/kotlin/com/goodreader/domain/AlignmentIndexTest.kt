package com.goodreader.domain

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertNull

class AlignmentIndexTest {
    @Test
    fun interpolatesInBothDirectionsAndClampsOutsideTheIndex() {
        val index =
            AlignmentIndex(
                listOf(
                    AlignmentAnchor(WordOffset(0), 0, 1.0),
                    AlignmentAnchor(WordOffset(100), 200_000, 0.9),
                ),
            )

        assertEquals(WordOffset(50), index.wordAtAudioTime(100_000))
        assertEquals(100_000L, index.audioTimeAtWord(WordOffset(50)))
        assertEquals(WordOffset(0), index.wordAtAudioTime(-1))
        assertEquals(0.9, index.confidenceAtAudioTime(100_000))
    }

    @Test
    fun returnsNullWithoutAnchors() {
        assertNull(AlignmentIndex(emptyList()).wordAtAudioTime(1))
        assertNull(AlignmentIndex(emptyList()).audioTimeAtWord(WordOffset(1)))
        assertNull(AlignmentIndex(emptyList()).confidenceAtAudioTime(1))
    }

    @Test
    fun rejectsInvalidAndNonMonotonicAnchors() {
        assertFailsWith<IllegalArgumentException> {
            AlignmentAnchor(WordOffset(0), -1, 0.5)
        }
        assertFailsWith<IllegalArgumentException> {
            AlignmentAnchor(WordOffset(0), 0, 1.1)
        }
        assertFailsWith<IllegalArgumentException> {
            AlignmentIndex(
                listOf(
                    AlignmentAnchor(WordOffset(5), 10, 0.9),
                    AlignmentAnchor(WordOffset(4), 20, 0.9),
                ),
            )
        }
    }
}
