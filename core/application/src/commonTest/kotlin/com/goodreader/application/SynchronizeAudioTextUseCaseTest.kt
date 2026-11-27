package com.goodreader.application

import com.goodreader.alignment.AlignmentSearchConfig
import com.goodreader.domain.AlignmentIndex
import com.goodreader.domain.AlignmentAnchor
import com.goodreader.domain.BookId
import com.goodreader.domain.WordOffset
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertIs
import kotlin.test.assertTrue

class SynchronizeAudioTextUseCaseTest {
    @Test
    fun usesConfidentCachedAnchorsForBothSwitchDirections() =
        runTest {
            val repository =
                FakeAlignmentIndexRepository(
                    AlignmentIndex(
                        listOf(
                            AlignmentAnchor(WordOffset(0), 0, 0.95),
                            AlignmentAnchor(WordOffset(100), 200_000, 0.95),
                        ),
                    ),
                )
            val asr = FakeSpeechRecognizer(AppResult.Success(RecognizedSpeech("never used", 1, 2, "fake")))
            val useCase = SynchronizeAudioTextUseCase(repository, FindTextMatchUseCase(asr))

            val audioTime = useCase.readToListen(BookId("book"), "edition-a", WordOffset(50), 101, 200_000)
            val textPosition =
                useCase.listenToRead(
                    BookId("book"),
                    "edition-a",
                    "file:///audio.wav",
                    100_000,
                    200_000,
                    "one two three four five six seven eight",
                )

            assertEquals(AppResult.Success(100_000L), audioTime)
            assertIs<AppResult.Success<AudioTextPosition>>(textPosition)
            assertEquals(WordOffset(50), textPosition.value.wordOffset)
            assertTrue(textPosition.value.usedCachedAnchor)
            assertEquals(null, asr.requestedUri)
        }

    @Test
    fun transcribesOncePersistsTheMatchAndReusesItOnTheNextSwitch() =
        runTest {
            val phrase = "the traveler crossed the quiet bridge before dawn"
            val text = buildString {
                repeat(80) { append("opening$it ") }
                append(phrase)
                repeat(30) { append(" ending$it") }
            }
            val repository = FakeAlignmentIndexRepository()
            val asr =
                FakeSpeechRecognizer(
                    AppResult.Success(RecognizedSpeech(phrase, 75_000, 100_000, "fake-1")),
                )
            val useCase = SynchronizeAudioTextUseCase(repository, FindTextMatchUseCase(asr))
            val config = AlignmentSearchConfig(initialWindowFraction = 0.8, minimumConfidence = 0.6)

            val first = useCase.listenToRead(BookId("book"), "edition-a", "file:///audio.m4b", 100_000, 200_000, text, config)
            val second = useCase.listenToRead(BookId("book"), "edition-a", "file:///audio.m4b", 100_000, 200_000, text, config)

            assertIs<AppResult.Success<AudioTextPosition>>(first)
            assertFalse(first.value.usedCachedAnchor)
            assertTrue(first.value.confidence > 0.9)
            assertEquals(first.value.wordOffset, (second as AppResult.Success).value.wordOffset)
            assertTrue(second.value.usedCachedAnchor)
            assertEquals(87, first.value.wordOffset.value)
            assertEquals(75_000, asr.requestedStartMillis)
            assertEquals(25_000, asr.requestedDurationMillis)
        }

    @Test
    fun returnsAnExplicitApproximationWhenAudioDoesNotMatchTheBook() =
        runTest {
            val asr =
                FakeSpeechRecognizer(
                    AppResult.Success(RecognizedSpeech("a wholly different story with other words", 0, 25_000, "fake")),
                )
            val repository = FakeAlignmentIndexRepository()
            val useCase = SynchronizeAudioTextUseCase(repository, FindTextMatchUseCase(asr))

            val result =
                useCase.listenToRead(
                    BookId("book"),
                    "wrong-edition",
                    "file:///audio.wav",
                    50_000,
                    100_000,
                    "the original edition contains entirely unrelated material and a distinct ending",
                    AlignmentSearchConfig(initialWindowFraction = 1.0, minimumConfidence = 0.9),
                )

            assertIs<AppResult.Success<AudioTextPosition>>(result)
            assertTrue(result.value.needsConfirmation)
            assertFalse(result.value.usedCachedAnchor)
            assertEquals(0.0, result.value.confidence)
            assertTrue(result.value.warning?.isNotBlank() == true)
            assertEquals(null, repository.index)
        }

    @Test
    fun readToListenUsesTheRatioWhenNoReliableAnchorExists() =
        runTest {
            val useCase =
                SynchronizeAudioTextUseCase(
                    FakeAlignmentIndexRepository(),
                    FindTextMatchUseCase(FakeSpeechRecognizer(AppResult.Success(RecognizedSpeech("words", 1, 2, "fake")))),
                )

            assertEquals(
                AppResult.Success(50_000L),
                useCase.readToListen(BookId("book"), "edition-a", WordOffset(25), 51, 100_000),
            )
        }

    private class FakeAlignmentIndexRepository(
        var index: AlignmentIndex? = null,
    ) : AlignmentIndexRepository {
        override suspend fun find(bookId: BookId, audioFingerprint: String): AppResult<AlignmentIndex?> =
            AppResult.Success(index)

        override suspend fun save(
            bookId: BookId,
            audioFingerprint: String,
            index: AlignmentIndex,
        ): AppResult<Unit> {
            this.index = index
            return AppResult.Success(Unit)
        }
    }
}
