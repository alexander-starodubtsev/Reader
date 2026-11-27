package com.goodreader.application

/** Deterministic ASR test double that records requested audio fragments. */
class FakeSpeechRecognizer(
    private val response: AppResult<RecognizedSpeech>,
) : SpeechRecognizer {
    var requestedUri: String? = null
        private set
    var requestedStartMillis: Long? = null
        private set
    var requestedDurationMillis: Long? = null
        private set

    override suspend fun transcribe(
        audioUri: String,
        startMillis: Long,
        durationMillis: Long,
    ): AppResult<RecognizedSpeech> {
        requestedUri = audioUri
        requestedStartMillis = startMillis
        requestedDurationMillis = durationMillis
        return response
    }
}
