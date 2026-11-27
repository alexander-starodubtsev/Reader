package com.goodreader.alignment

import com.goodreader.domain.WordOffset
import kotlin.math.abs
import kotlin.math.max
import kotlin.math.min

/** A ranked text location for a recognized audio fragment. */
data class TextMatch(
    val wordOffset: WordOffset,
    val confidence: Double,
    val margin: Double,
) {
    init {
        require(confidence in 0.0..1.0)
        require(margin in 0.0..1.0)
    }
}

/** Finds approximate ordered phrase matches in a bounded range of book words. */
class NGramMatcher(
    private val gramSize: Int = 3,
) {
    init {
        require(gramSize >= 2) { "N-gram size must be at least two." }
    }

    /** Matches a transcript against `[windowStart, windowEnd)` without scanning outside it. */
    fun findBestMatch(
        bookWords: List<String>,
        spokenWords: List<String>,
        windowStart: Int,
        windowEnd: Int,
        expectedWordOffset: Int,
    ): TextMatch? {
        val start = windowStart.coerceIn(0, bookWords.size)
        val end = windowEnd.coerceIn(start, bookWords.size)
        if (spokenWords.size < gramSize || end - start < gramSize) return null

        val candidates = candidateStarts(bookWords, spokenWords, start, end, expectedWordOffset)
        if (candidates.isEmpty()) return null
        val ranked =
            candidates
                .flatMap { candidateStart -> candidateLengths(spokenWords.size).map { length -> candidateStart to length } }
                .filter { (candidateStart, length) -> candidateStart >= start && candidateStart + length <= end }
                .map { (candidateStart, length) ->
                    val candidateWords = bookWords.subList(candidateStart, candidateStart + length)
                    val score = similarity(spokenWords, candidateWords)
                    RankedMatch(candidateStart, score, abs(candidateStart - expectedWordOffset))
                }.sortedWith(compareByDescending<RankedMatch> { it.score }.thenBy { it.distanceFromEstimate })
        val best = ranked.firstOrNull() ?: return null
        val secondScore = ranked.firstOrNull { it.start != best.start }?.score ?: 0.0
        return TextMatch(
            wordOffset = WordOffset(best.start),
            confidence = best.score.coerceIn(0.0, 1.0),
            margin = (best.score - secondScore).coerceIn(0.0, 1.0),
        )
    }

    private fun candidateStarts(
        bookWords: List<String>,
        spokenWords: List<String>,
        start: Int,
        end: Int,
        expectedWordOffset: Int,
    ): Set<Int> {
        val starts = mutableSetOf<Int>()
        val queryGramPositions = mutableMapOf<String, MutableList<Int>>()
        grams(spokenWords).forEachIndexed { queryIndex, gram ->
            queryGramPositions.getOrPut(gram, ::mutableListOf).add(queryIndex)
        }
        for (bookIndex in start..(end - gramSize)) {
            val gram = bookWords.subList(bookIndex, bookIndex + gramSize).joinToString(" ")
            queryGramPositions[gram].orEmpty().forEach { queryIndex ->
                val candidateStart = bookIndex - queryIndex
                for (offset in -2..2) {
                    val adjustedStart = candidateStart + offset
                    if (adjustedStart in start..(end - gramSize)) starts += adjustedStart
                }
            }
        }
        if (starts.isEmpty()) {
            starts += expectedWordOffset.coerceIn(start, max(start, end - spokenWords.size))
            val stride = max(1, (end - start) / 128)
            var sample = start
            while (sample <= end - gramSize) {
                starts += sample
                sample += stride
            }
        }
        return starts
    }

    private fun candidateLengths(spokenWordCount: Int): IntRange {
        val minLength = max(gramSize, (spokenWordCount * 0.8).toInt())
        val maxLength = max(minLength, (spokenWordCount * 1.2).toInt())
        return minLength..maxLength
    }

    private fun similarity(
        spoken: List<String>,
        candidate: List<String>,
    ): Double {
        val sequenceScore = lcsSimilarity(spoken, candidate)
        val spokenGrams = grams(spoken).toSet()
        val candidateGrams = grams(candidate).toSet()
        val overlap = spokenGrams.intersect(candidateGrams).size.toDouble()
        val gramScore =
            if (spokenGrams.isEmpty() && candidateGrams.isEmpty()) {
                1.0
            } else {
                2.0 * overlap / (spokenGrams.size + candidateGrams.size)
            }
        return 0.75 * sequenceScore + 0.25 * gramScore
    }

    private fun lcsSimilarity(
        left: List<String>,
        right: List<String>,
    ): Double {
        var previous = DoubleArray(right.size + 1)
        left.forEach { leftWord ->
            val current = DoubleArray(right.size + 1)
            right.forEachIndexed { index, rightWord ->
                val match = previous[index] + wordSimilarity(leftWord, rightWord)
                current[index + 1] = maxOf(previous[index + 1], current[index], match)
            }
            previous = current
        }
        return (previous.last() / max(left.size, right.size)).coerceIn(0.0, 1.0)
    }

    private fun wordSimilarity(
        left: String,
        right: String,
    ): Double {
        if (left == right) return 1.0
        if (left.isEmpty() || right.isEmpty()) return 0.0
        if (min(left.length, right.length) < 4) return 0.0
        val distance = levenshteinDistance(left, right)
        return (1.0 - distance.toDouble() / max(left.length, right.length)).coerceAtLeast(0.0)
    }

    private fun levenshteinDistance(
        left: String,
        right: String,
    ): Int {
        var previous = IntArray(right.length + 1) { it }
        left.forEachIndexed { leftIndex, leftChar ->
            val current = IntArray(right.length + 1)
            current[0] = leftIndex + 1
            right.forEachIndexed { rightIndex, rightChar ->
                current[rightIndex + 1] =
                    minOf(
                        current[rightIndex] + 1,
                        previous[rightIndex + 1] + 1,
                        previous[rightIndex] + if (leftChar == rightChar) 0 else 1,
                    )
            }
            previous = current
        }
        return previous.last()
    }

    private fun grams(words: List<String>): List<String> =
        if (words.size < gramSize) {
            emptyList()
        } else {
            (0..words.size - gramSize).map { words.subList(it, it + gramSize).joinToString(" ") }
        }

    private data class RankedMatch(
        val start: Int,
        val score: Double,
        val distanceFromEstimate: Int,
    )
}
