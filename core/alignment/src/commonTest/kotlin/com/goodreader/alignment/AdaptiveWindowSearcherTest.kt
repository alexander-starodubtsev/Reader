package com.goodreader.alignment

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertNull

class AdaptiveWindowSearcherTest {
    @Test
    fun findsTheCorrectRepeatedPhraseNearTheRatioEstimate() {
        val phrase = "the traveler crossed the quiet bridge before dawn".split(" ")
        val words =
            buildList {
                repeat(40) { add("opening$it") }
                addAll(phrase)
                repeat(40) { add("middle$it") }
                addAll(phrase)
                repeat(40) { add("ending$it") }
            }

        val match =
            AdaptiveWindowSearcher().search(
                bookWords = words,
                spokenWords = phrase,
                estimatedWordOffset = 90,
                config = AlignmentSearchConfig(initialWindowFraction = 0.4, minimumConfidence = 0.7),
            )

        assertNotNull(match)
        assertEquals(40 + phrase.size + 40, match.wordOffset.value)
        assertEquals(1.0, match.confidence)
    }

    @Test
    fun expandsWindowWhenTheEstimatedLocationIsFarFromTheMatch() {
        val phrase = "the lighthouse keeper watched the storm pass".split(" ")
        val words =
            buildList {
                repeat(90) { add("chapter$it") }
                addAll(phrase)
                repeat(10) { add("ending$it") }
            }

        val match =
            AdaptiveWindowSearcher().search(
                bookWords = words,
                spokenWords = phrase,
                estimatedWordOffset = 90,
                config = AlignmentSearchConfig(initialWindowFraction = 0.1, minimumConfidence = 0.7),
            )

        assertNotNull(match)
        assertEquals(90, match.wordOffset.value)
    }

    @Test
    fun returnsNullForEmptyAndUnrelatedWrongEditionText() {
        assertNull(AdaptiveWindowSearcher().search(emptyList(), listOf("audio"), 0))
        assertNull(
            AdaptiveWindowSearcher().search(
                bookWords = "completely unrelated chapter words".split(" "),
                spokenWords = "a different edition tells another story".split(" "),
                estimatedWordOffset = 1,
                config = AlignmentSearchConfig(minimumConfidence = 0.8),
            ),
        )
    }

    @Test
    fun toleratesNoisySpeechAndDifferentPhraseLengths() {
        val bookWords = "the old traveler crossed the quiet bridge before dawn".split(" ")
        val noisyTranscript = "old travaler crossed quiet bridge before dawn".split(" ")

        val match =
            AdaptiveWindowSearcher().search(
                bookWords = bookWords,
                spokenWords = noisyTranscript,
                estimatedWordOffset = 1,
                config = AlignmentSearchConfig(initialWindowFraction = 1.0, minimumConfidence = 0.55),
            )

        assertNotNull(match)
        assertEquals(1, match.wordOffset.value)
        kotlin.test.assertTrue(match.confidence > 0.7)
    }

    @Test
    fun handlesVeryShortAndVeryLongBooks() {
        assertNull(AdaptiveWindowSearcher().search(listOf("one", "two"), listOf("one", "two"), 0))
        val phrase = "the captain watched the distant horizon".split(" ")
        val longBook =
            buildList {
                repeat(50_000) { add("filler$it") }
                addAll(phrase)
                repeat(50_000) { add("ending$it") }
            }

        val match =
            AdaptiveWindowSearcher().search(
                bookWords = longBook,
                spokenWords = phrase,
                estimatedWordOffset = 50_010,
                config = AlignmentSearchConfig(initialWindowFraction = 0.01, minimumConfidence = 0.7),
            )

        assertNotNull(match)
        assertEquals(50_000, match.wordOffset.value)
    }

    @Test
    fun windowIsBiasedTowardEarlierTextAndStaysInsideBookBounds() {
        val window = AdaptiveWindowSearcher().windowFor(100, 50, 0.2, 0.7)

        assertEquals(36, window.startWord)
        assertEquals(56, window.endWordExclusive)
        assertEquals(0, AdaptiveWindowSearcher().windowFor(100, 0, 0.2).startWord)
        assertEquals(100, AdaptiveWindowSearcher().windowFor(100, 100, 0.2).endWordExclusive)
    }
}
