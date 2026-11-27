package com.goodreader.alignment

import kotlin.math.ceil
import kotlin.math.floor

/** Tunable limits for alignment search and audio-fragment transcription. */
data class AlignmentSearchConfig(
    val initialWindowFraction: Double = 0.20,
    val leftBias: Double = 0.65,
    val expansionFactor: Double = 1.8,
    val minimumConfidence: Double = 0.62,
    val minimumMargin: Double = 0.05,
    val fragmentDurationMillis: Long = 25_000,
) {
    init {
        require(initialWindowFraction in 0.01..1.0)
        require(leftBias in 0.5..1.0)
        require(expansionFactor > 1.0)
        require(minimumConfidence in 0.0..1.0)
        require(minimumMargin in 0.0..1.0)
        require(fragmentDurationMillis > 0)
    }
}

/** The exact text interval considered during one search attempt. */
data class SearchWindow(
    val startWord: Int,
    val endWordExclusive: Int,
) {
    init {
        require(startWord >= 0)
        require(endWordExclusive >= startWord)
    }
}

/** Expands a left-biased candidate range around a ratio-based position estimate. */
class AdaptiveWindowSearcher(
    private val matcher: NGramMatcher = NGramMatcher(),
) {
    /** Returns the best confident match, expanding the window only as needed. */
    fun search(
        bookWords: List<String>,
        spokenWords: List<String>,
        estimatedWordOffset: Int,
        config: AlignmentSearchConfig = AlignmentSearchConfig(),
    ): TextMatch? {
        if (bookWords.isEmpty() || spokenWords.isEmpty()) return null
        val estimate = estimatedWordOffset.coerceIn(0, bookWords.lastIndex)
        var fraction = config.initialWindowFraction
        var best: TextMatch? = null
        while (true) {
            val window = windowFor(bookWords.size, estimate, fraction, config.leftBias)
            val candidate =
                matcher.findBestMatch(
                    bookWords = bookWords,
                    spokenWords = spokenWords,
                    windowStart = window.startWord,
                    windowEnd = window.endWordExclusive,
                    expectedWordOffset = estimate,
                )
            if (candidate != null && (best == null || candidate.confidence > best.confidence)) {
                best = candidate
            }
            if (candidate != null &&
                candidate.confidence >= config.minimumConfidence &&
                candidate.margin >= config.minimumMargin
            ) {
                return candidate
            }
            if (fraction >= 1.0) return best?.takeIf { it.confidence >= config.minimumConfidence }
            fraction = (fraction * config.expansionFactor).coerceAtMost(1.0)
        }
    }

    /** Calculates a bounded text window, assigning more of its width to the left side. */
    fun windowFor(
        wordCount: Int,
        estimate: Int,
        fraction: Double,
        leftBias: Double = 0.65,
    ): SearchWindow {
        require(wordCount >= 0)
        require(fraction in 0.0..1.0)
        require(leftBias in 0.5..1.0)
        if (wordCount == 0) return SearchWindow(0, 0)

        val center = estimate.coerceIn(0, wordCount - 1)
        val windowSize = ceil(wordCount * fraction).toInt().coerceIn(1, wordCount)
        val wordsToLeft = floor(windowSize * leftBias).toInt()
        var start = (center - wordsToLeft).coerceAtLeast(0)
        var end = (start + windowSize).coerceAtMost(wordCount)
        start = (end - windowSize).coerceAtLeast(0)
        end = (start + windowSize).coerceAtMost(wordCount)
        return SearchWindow(start, end)
    }
}
