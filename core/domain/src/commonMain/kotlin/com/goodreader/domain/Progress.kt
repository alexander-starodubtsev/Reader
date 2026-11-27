package com.goodreader.domain

import kotlinx.datetime.Instant

/** The last saved text position for a book, measured as a zero-based character offset. */
data class ReadingProgress(
    val bookId: BookId,
    val characterOffset: Long,
    val updatedAt: Instant,
) {
    init {
        require(characterOffset >= 0) { "Character offset must not be negative." }
    }
}

/** The last saved audio position in a specific track. */
data class ListeningProgress(
    val bookId: BookId,
    val trackId: AudioTrackId,
    val positionMillis: Long,
    val updatedAt: Instant,
) {
    init {
        require(positionMillis >= 0) { "Audio position must not be negative." }
    }
}

/** A word offset in a normalized book text. */
data class WordOffset(
    val value: Int,
) {
    init {
        require(value >= 0) { "Word offset must not be negative." }
    }
}
