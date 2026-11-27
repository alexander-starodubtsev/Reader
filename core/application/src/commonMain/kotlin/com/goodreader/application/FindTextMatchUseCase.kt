package com.goodreader.application

import com.goodreader.alignment.AdaptiveWindowSearcher
import com.goodreader.alignment.AlignmentSearchConfig
import com.goodreader.alignment.TextMatch
import com.goodreader.alignment.TextNormalizer

/** Audio-derived text location and the evidence used to find it. */
data class AudioTextMatch(
    val match: TextMatch,
    val stopWordOffset: Int,
    val transcript: RecognizedSpeech,
    val fragmentStartMillis: Long,
)

/** Transcribes a short audio fragment and searches only an adaptive book-text window. */
class FindTextMatchUseCase(
    private val speechRecognizer: SpeechRecognizer,
    private val searcher: AdaptiveWindowSearcher = AdaptiveWindowSearcher(),
) {
    /** Finds the likely text location corresponding to an audiobook playback position. */
    suspend operator fun invoke(
        bookText: String,
        audioUri: String,
        audioPositionMillis: Long,
        totalAudioDurationMillis: Long,
        config: AlignmentSearchConfig = AlignmentSearchConfig(),
    ): AppResult<AudioTextMatch> {
        if (audioUri.isBlank()) {
            return AppResult.Failure(AppError.Validation("audio_uri_required", "Choose an audio file."))
        }
        if (audioPositionMillis < 0 ||
            totalAudioDurationMillis <= 0 ||
            audioPositionMillis > totalAudioDurationMillis
        ) {
            return AppResult.Failure(
                AppError.Validation("invalid_audio_position", "Audio position must be within the track."),
            )
        }
        val bookWords = TextNormalizer.tokenize(bookText)
        if (bookWords.isEmpty()) {
            return AppResult.Failure(AppError.Validation("empty_book_text", "The book contains no searchable text."))
        }

        val fragmentDuration =
            minOf(
                config.fragmentDurationMillis,
                if (audioPositionMillis == 0L) totalAudioDurationMillis else audioPositionMillis,
            )
        val fragmentStart = (audioPositionMillis - fragmentDuration).coerceAtLeast(0)
        val transcription = speechRecognizer.transcribe(audioUri, fragmentStart, fragmentDuration)
        if (transcription is AppResult.Failure) return transcription
        val recognized = (transcription as AppResult.Success).value
        val spokenWords = TextNormalizer.tokenize(recognized.text)
        if (spokenWords.isEmpty()) {
            return AppResult.Failure(AppError.Validation("empty_transcript", "Speech recognition returned no words."))
        }

        val estimate =
            (
                audioPositionMillis.toDouble() / totalAudioDurationMillis * bookWords.size
            ).toInt().coerceIn(0, bookWords.lastIndex)
        val match =
            searcher.search(bookWords, spokenWords, estimate, config)
                ?: return AppResult.Failure(
                    AppError.Unavailable("alignment", "No confident text match was found."),
                )
        val stopWordOffset = (match.wordOffset.value + spokenWords.size - 1).coerceIn(match.wordOffset.value, bookWords.lastIndex)
        return AppResult.Success(AudioTextMatch(match, stopWordOffset, recognized, fragmentStart))
    }
}
