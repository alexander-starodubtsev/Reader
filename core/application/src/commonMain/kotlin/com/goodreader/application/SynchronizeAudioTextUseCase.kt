package com.goodreader.application

import com.goodreader.alignment.AlignmentSearchConfig
import com.goodreader.alignment.TextNormalizer
import com.goodreader.domain.AlignmentAnchor
import com.goodreader.domain.AlignmentIndex
import com.goodreader.domain.BookId
import com.goodreader.domain.WordOffset

/** A text location returned when switching from audio to reading. */
data class AudioTextPosition(
    val wordOffset: WordOffset,
    val confidence: Double,
    val usedCachedAnchor: Boolean,
    val needsConfirmation: Boolean,
    val warning: String? = null,
)

/** Maps read/listen positions and caches ASR matches for later, faster switches. */
class SynchronizeAudioTextUseCase(
    private val indexes: AlignmentIndexRepository,
    private val findTextMatch: FindTextMatchUseCase,
) {
    /** Maps a text position to the cumulative audiobook timeline. */
    suspend fun readToListen(
        bookId: BookId,
        audioFingerprint: String,
        wordOffset: WordOffset,
        totalWordCount: Int,
        totalAudioDurationMillis: Long,
        minimumCachedConfidence: Double = 0.75,
    ): AppResult<Long> {
        val validation = validateTimeline(audioFingerprint, totalWordCount, totalAudioDurationMillis)
        if (validation != null) return validation
        if (wordOffset.value >= totalWordCount) {
            return AppResult.Failure(
                AppError.Validation("invalid_word_position", "The text position is outside the book."),
            )
        }
        val lookup = indexes.find(bookId, audioFingerprint)
        if (lookup is AppResult.Failure) return lookup
        val index = (lookup as AppResult.Success).value
        val cachedConfidence = index?.confidenceAtWord(wordOffset)
        val cachedTime = index?.audioTimeAtWord(wordOffset)
        if (cachedTime != null && cachedConfidence != null && cachedConfidence >= minimumCachedConfidence) {
            return AppResult.Success(cachedTime.coerceIn(0L, totalAudioDurationMillis))
        }
        val estimatedTime =
            (wordOffset.value.toDouble() / (totalWordCount - 1).coerceAtLeast(1) *
                totalAudioDurationMillis).toLong()
        return AppResult.Success(estimatedTime.coerceIn(0L, totalAudioDurationMillis))
    }

    /** Finds the text corresponding to an audio position, preferring cached anchors over ASR. */
    suspend fun listenToRead(
        bookId: BookId,
        audioFingerprint: String,
        audioUri: String,
        audioPositionMillis: Long,
        totalAudioDurationMillis: Long,
        bookText: String,
        config: AlignmentSearchConfig = AlignmentSearchConfig(),
    ): AppResult<AudioTextPosition> {
        val words = TextNormalizer.tokenize(bookText)
        val validation = validateTimeline(audioFingerprint, words.size, totalAudioDurationMillis)
        if (validation != null) return validation
        if (audioUri.isBlank() || audioPositionMillis !in 0..totalAudioDurationMillis) {
            return AppResult.Failure(
                AppError.Validation("invalid_audio_position", "Audio position must be within the recording."),
            )
        }
        val lookup = indexes.find(bookId, audioFingerprint)
        if (lookup is AppResult.Failure) return lookup
        val existing = (lookup as AppResult.Success).value
        val cachedConfidence = existing?.confidenceAtAudioTime(audioPositionMillis)
        val cachedWord = existing?.wordAtAudioTime(audioPositionMillis)
        if (cachedWord != null && cachedConfidence != null && cachedConfidence >= config.minimumConfidence) {
            return AppResult.Success(
                AudioTextPosition(cachedWord, cachedConfidence, usedCachedAnchor = true, needsConfirmation = false),
            )
        }

        return when (
            val result =
                findTextMatch(
                    bookText,
                    audioUri,
                    audioPositionMillis,
                    totalAudioDurationMillis,
                    config,
                )
        ) {
            is AppResult.Success -> {
                val match = result.value
                val anchor =
                    AlignmentAnchor(
                        WordOffset(match.stopWordOffset),
                        audioPositionMillis,
                        match.match.confidence,
                    )
                val saved = indexes.save(bookId, audioFingerprint, merge(existing, anchor, words.size, totalAudioDurationMillis))
                if (saved is AppResult.Failure) {
                    saved
                } else {
                    AppResult.Success(
                        AudioTextPosition(
                            anchor.wordOffset,
                            match.match.confidence,
                            usedCachedAnchor = false,
                            needsConfirmation = false,
                        ),
                    )
                }
            }
            is AppResult.Failure -> {
                val estimated =
                    (audioPositionMillis.toDouble() / totalAudioDurationMillis * words.size)
                        .toInt()
                        .coerceIn(0, words.lastIndex)
                when (val error = result.error) {
                    is AppError.Unavailable ->
                        AppResult.Success(
                            AudioTextPosition(
                                WordOffset(estimated),
                                0.0,
                                usedCachedAnchor = false,
                                needsConfirmation = true,
                                warning = error.detail,
                            ),
                        )
                    else -> result
                }
            }
        }
    }

    private fun merge(
        existing: AlignmentIndex?,
        anchor: AlignmentAnchor,
        wordCount: Int,
        durationMillis: Long,
    ): AlignmentIndex {
        val before =
            existing?.anchors.orEmpty().filter {
                it.wordOffset.value < anchor.wordOffset.value && it.audioTimeMillis < anchor.audioTimeMillis
            }
        val after =
            existing?.anchors.orEmpty().filter {
                it.wordOffset.value > anchor.wordOffset.value && it.audioTimeMillis > anchor.audioTimeMillis
            }
        val anchors = mutableListOf<AlignmentAnchor>()
        if (before.isEmpty() && anchor.wordOffset.value > 0 && anchor.audioTimeMillis > 0) {
            anchors += AlignmentAnchor(WordOffset(0), 0, LOW_CONFIDENCE)
        }
        anchors += before
        anchors += anchor
        if (after.isEmpty() &&
            anchor.wordOffset.value < wordCount - 1 &&
            anchor.audioTimeMillis < durationMillis
        ) {
            anchors += AlignmentAnchor(WordOffset(wordCount - 1), durationMillis, LOW_CONFIDENCE)
        }
        anchors += after
        return AlignmentIndex(anchors)
    }

    private fun validateTimeline(
        fingerprint: String,
        wordCount: Int,
        durationMillis: Long,
    ): AppResult.Failure? =
        when {
            fingerprint.isBlank() ->
                AppResult.Failure(AppError.Validation("audio_fingerprint_required", "Audio identity is required."))
            wordCount <= 0 ->
                AppResult.Failure(AppError.Validation("empty_book_text", "The book contains no searchable text."))
            durationMillis <= 0 ->
                AppResult.Failure(AppError.Validation("invalid_audio_duration", "Audio duration must be positive."))
            else -> null
        }

    private companion object {
        const val LOW_CONFIDENCE = 0.2
    }
}
