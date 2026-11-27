package com.goodreader.application

import com.goodreader.alignment.AlignmentSearchConfig
import com.goodreader.alignment.TextNormalizer
import com.goodreader.domain.BookId
import com.goodreader.domain.WordOffset
import java.io.File
import javax.sound.sampled.AudioSystem
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertIs
import kotlin.test.assertTrue

class SyncRoundTripFixtureTest {
    @Test
    fun testDataAudioToTextAndBackReusesTheSavedAnchor() =
        runTest {
            val fixtureDirectory = locateFixtureDirectory()
            val textFile = File(fixtureDirectory, "sample.txt")
            val audioFile = File(fixtureDirectory, "sample.wav")
            val bookText = textFile.readText()
            val durationMillis =
                AudioSystem.getAudioInputStream(audioFile).use { audio ->
                    (audio.frameLength * 1000 / audio.format.frameRate).toLong()
                }
            val wordCount = TextNormalizer.tokenize(bookText).size
            val estimatedWord = wordCount / 2
            val speech =
                TextNormalizer.tokenize(bookText)
                    .subList(estimatedWord - 5, estimatedWord + 5)
                    .joinToString(" ")
            val audioPosition = durationMillis / 2
            val asr =
                FakeSpeechRecognizer(
                    AppResult.Success(
                        RecognizedSpeech(
                            speech,
                            audioPosition - 20_000,
                            audioPosition,
                            "fixture-fake-asr",
                        ),
                    ),
                )
            val repository = FakeAlignmentIndexRepository()
            val sync =
                SynchronizeAudioTextUseCase(
                    repository,
                    FindTextMatchUseCase(asr),
                )
            val bookId = BookId("test-data-book")
            val fingerprint = audioFile.absolutePath
            val config = AlignmentSearchConfig(fragmentDurationMillis = 20_000)

            val textPosition =
                sync.listenToRead(
                    bookId,
                    fingerprint,
                    audioFile.toURI().toString(),
                    audioPosition,
                    durationMillis,
                    bookText,
                    config,
                )
            val returnedAudioPosition =
                sync.readToListen(
                    bookId,
                    fingerprint,
                    requireSuccess(textPosition).wordOffset,
                    wordCount,
                    durationMillis,
                )

            assertTrue(textFile.length() > 0)
            assertTrue(audioFile.length() > 0)
            assertTrue(requireSuccess(textPosition).confidence > 0.9)
            assertEquals(AppResult.Success(audioPosition), returnedAudioPosition)
            assertEquals(audioPosition, asr.requestedStartMillis?.plus(asr.requestedDurationMillis!!))
        }

    @Test
    fun missingIntroAudioMatchesLaterTextAndCanBeMappedBackToItsTimestamp() =
        runTest {
            val fixtureDirectory = locateFixtureDirectory()
            val textFile = File(fixtureDirectory, "sample.txt")
            val audioFile = File(fixtureDirectory, "sample_missing_intro.wav")
            val bookText = textFile.readText()
            val durationMillis = durationOf(audioFile)
            val words = TextNormalizer.tokenize(bookText)
            val targetWord = words.size * 2 / 3
            val speech = words.subList(targetWord - 5, targetWord + 5).joinToString(" ")
            val audioPosition = durationMillis / 2
            val asr =
                FakeSpeechRecognizer(
                    AppResult.Success(
                        RecognizedSpeech(
                            speech,
                            audioPosition - 20_000,
                            audioPosition,
                            "fixture-fake-asr",
                        ),
                    ),
                )
            val repository = FakeAlignmentIndexRepository()
            val sync =
                SynchronizeAudioTextUseCase(
                    repository,
                    FindTextMatchUseCase(asr),
                )

            val textPosition =
                sync.listenToRead(
                    BookId("missing-intro-book"),
                    audioFile.absolutePath,
                    audioFile.toURI().toString(),
                    audioPosition,
                    durationMillis,
                    bookText,
                    AlignmentSearchConfig(initialWindowFraction = 0.15, fragmentDurationMillis = 20_000),
                )
            val found = requireSuccess(textPosition)
            val mappedAudioPosition =
                sync.readToListen(
                    BookId("missing-intro-book"),
                    audioFile.absolutePath,
                    found.wordOffset,
                    words.size,
                    durationMillis,
                )

            assertTrue(audioFile.isFile)
            assertTrue(found.wordOffset.value > words.size / 2)
            assertEquals(AppResult.Success(audioPosition), mappedAudioPosition)
        }

    private fun durationOf(audioFile: File): Long =
        AudioSystem.getAudioInputStream(audioFile).use { audio ->
            (audio.frameLength * 1000 / audio.format.frameRate).toLong()
        }

    private fun locateFixtureDirectory(): File {
        val candidates =
            listOf(
                File(System.getProperty("user.dir"), "../../test-data"),
                File(System.getProperty("user.dir"), "test-data"),
            )
        return candidates.firstOrNull { File(it, "sample.txt").isFile && File(it, "sample.wav").isFile }
            ?: error("The test-data/sample.txt and test-data/sample.wav fixtures are required.")
    }

    private fun requireSuccess(result: AppResult<AudioTextPosition>): AudioTextPosition {
        assertIs<AppResult.Success<AudioTextPosition>>(result)
        return result.value
    }

    private class FakeAlignmentIndexRepository(
        var index: com.goodreader.domain.AlignmentIndex? = null,
    ) : AlignmentIndexRepository {
        override suspend fun find(
            bookId: BookId,
            audioFingerprint: String,
        ): AppResult<com.goodreader.domain.AlignmentIndex?> = AppResult.Success(index)

        override suspend fun save(
            bookId: BookId,
            audioFingerprint: String,
            index: com.goodreader.domain.AlignmentIndex,
        ): AppResult<Unit> {
            this.index = index
            return AppResult.Success(Unit)
        }
    }
}
