package com.goodreader.domain

/** A monotonic correspondence between a text word and an audio timestamp. */
data class AlignmentAnchor(
    val wordOffset: WordOffset,
    val audioTimeMillis: Long,
    val confidence: Double,
) {
    init {
        require(audioTimeMillis >= 0) { "Audio timestamp must not be negative." }
        require(confidence in 0.0..1.0) { "Confidence must be between zero and one." }
    }
}

/** Cached anchors linking audiobook time to a word position in the book. */
data class AlignmentIndex(
    val anchors: List<AlignmentAnchor>,
) {
    init {
        require(
            anchors.zipWithNext().all { (left, right) ->
                left.wordOffset.value < right.wordOffset.value && left.audioTimeMillis < right.audioTimeMillis
            },
        ) {
            "Alignment anchors must increase strictly in both text and audio time."
        }
    }

    /** Estimates a word offset at the given audio time using neighboring anchors. */
    fun wordAtAudioTime(audioTimeMillis: Long): WordOffset? =
        interpolate(
            value = audioTimeMillis,
            valueOf = { it.audioTimeMillis },
            positionOf = { it.wordOffset.value.toLong() },
        )?.toInt()
            ?.let(::WordOffset)

    /** Estimates an audio time at the given word offset using neighboring anchors. */
    fun audioTimeAtWord(wordOffset: WordOffset): Long? =
        interpolate(
            value = wordOffset.value.toLong(),
            valueOf = { it.wordOffset.value.toLong() },
            positionOf = { it.audioTimeMillis },
        )

    /** Returns the confidence of an exact or bracketed cached alignment point. */
    fun confidenceAtAudioTime(audioTimeMillis: Long): Double? {
        if (anchors.isEmpty()) return null
        anchors.firstOrNull { it.audioTimeMillis == audioTimeMillis }?.let { return it.confidence }
        if (audioTimeMillis < anchors.first().audioTimeMillis || audioTimeMillis > anchors.last().audioTimeMillis) {
            return null
        }
        val upperIndex = anchors.indexOfFirst { it.audioTimeMillis >= audioTimeMillis }
        return minOf(anchors[upperIndex - 1].confidence, anchors[upperIndex].confidence)
    }

    /** Returns the confidence of an exact or bracketed cached text position. */
    fun confidenceAtWord(wordOffset: WordOffset): Double? {
        if (anchors.isEmpty()) return null
        anchors.firstOrNull { it.wordOffset == wordOffset }?.let { return it.confidence }
        if (wordOffset.value < anchors.first().wordOffset.value ||
            wordOffset.value > anchors.last().wordOffset.value
        ) {
            return null
        }
        val upperIndex = anchors.indexOfFirst { it.wordOffset.value >= wordOffset.value }
        return minOf(anchors[upperIndex - 1].confidence, anchors[upperIndex].confidence)
    }

    private fun interpolate(
        value: Long,
        valueOf: (AlignmentAnchor) -> Long,
        positionOf: (AlignmentAnchor) -> Long,
    ): Long? {
        if (anchors.isEmpty()) return null
        if (value <= valueOf(anchors.first())) return positionOf(anchors.first())
        if (value >= valueOf(anchors.last())) return positionOf(anchors.last())

        val upperIndex = anchors.indexOfFirst { valueOf(it) >= value }
        val left = anchors[upperIndex - 1]
        val right = anchors[upperIndex]
        val fraction = (value - valueOf(left)).toDouble() / (valueOf(right) - valueOf(left))
        return (positionOf(left) + fraction * (positionOf(right) - positionOf(left))).toLong()
    }
}
