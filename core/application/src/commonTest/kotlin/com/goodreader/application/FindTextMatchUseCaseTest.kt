package com.goodreader.application

import com.goodreader.alignment.AlignmentSearchConfig
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertIs
import kotlin.test.assertTrue

class FindTextMatchUseCaseTest {
    @Test
    fun transcribesOnlyTheBoundedFragmentAndMatchesNearTheRatioEstimate() =
        runTest {
            val phrase = "the traveler crossed the quiet bridge before dawn"
            val bookText =
                buildString {
                    repeat(80) { append("opening$it ") }
                    append(phrase)
                    repeat(10) { append(" ending$it") }
                }
            val asr =
                FakeSpeechRecognizer(
                    AppResult.Success(RecognizedSpeech(phrase, 75_000, 100_000, "fake-1")),
                )

            val result =
                FindTextMatchUseCase(asr)(
                    bookText = bookText,
                    audioUri = "file:///book.m4b",
                    audioPositionMillis = 100_000,
                    totalAudioDurationMillis = 200_000,
                    config =
                        AlignmentSearchConfig(
                            initialWindowFraction = 0.8,
                            minimumConfidence = 0.6,
                            fragmentDurationMillis = 25_000,
                        ),
                )

            assertIs<AppResult.Success<AudioTextMatch>>(result)
            assertEquals("file:///book.m4b", asr.requestedUri)
            assertEquals(75_000, asr.requestedStartMillis)
            assertEquals(25_000, asr.requestedDurationMillis)
            assertEquals(80, result.value.match.wordOffset.value)
            assertEquals(87, result.value.stopWordOffset)
            assertTrue(result.value.match.confidence > 0.9)
        }

    @Test
    fun reportsInvalidPositionsWithoutCallingAsr() =
        runTest {
            val asr = FakeSpeechRecognizer(AppResult.Success(RecognizedSpeech("words", 0, 1000, "fake")))

            val result = FindTextMatchUseCase(asr)("book words", "audio", 2000, 1000)

            assertIs<AppResult.Failure>(result)
            assertEquals("invalid_audio_position", (result.error as AppError.Validation).code)
            assertEquals(null, asr.requestedUri)
        }

    @Test
    fun reportsUnmatchedWrongEditionAudioExplicitly() =
        runTest {
            val asr =
                FakeSpeechRecognizer(
                    AppResult.Success(RecognizedSpeech("a wholly different story", 0, 20_000, "fake")),
                )

            val result =
                FindTextMatchUseCase(asr)(
                    bookText = "the original edition contains entirely unrelated material",
                    audioUri = "file:///audio.mp3",
                    audioPositionMillis = 0,
                    totalAudioDurationMillis = 100_000,
                    config = AlignmentSearchConfig(initialWindowFraction = 1.0, minimumConfidence = 0.8),
                )

            assertIs<AppResult.Failure>(result)
            assertIs<AppError.Unavailable>(result.error)
        }
}
