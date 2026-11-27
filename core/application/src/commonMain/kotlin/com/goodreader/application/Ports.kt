package com.goodreader.application

import com.goodreader.domain.AlignmentIndex
import com.goodreader.domain.AudioTrack
import com.goodreader.domain.Book
import com.goodreader.domain.BookId
import com.goodreader.domain.ListeningProgress
import com.goodreader.domain.ReadingProgress
import com.goodreader.domain.ReadingSession

/** Persistence contract for locally stored books. */
interface BookRepository {
    /** Finds a book by stable identifier. */
    suspend fun findById(id: BookId): AppResult<Book?>

    /** Returns books matching an optional case-insensitive query. */
    suspend fun search(query: String?): AppResult<List<Book>>

    /** Inserts or updates a book. */
    suspend fun save(book: Book): AppResult<Unit>
}

/** Persistence contract for audiobook tracks. */
interface AudioTrackRepository {
    /** Lists a book's tracks ordered by chapter index. */
    suspend fun listForBook(bookId: BookId): AppResult<List<AudioTrack>>

    /** Inserts or updates an audio track. */
    suspend fun save(track: AudioTrack): AppResult<Unit>
}

/** Persistence contract for reading and listening positions. */
interface ProgressRepository {
    /** Reads the last text position for a book. */
    suspend fun readingProgress(bookId: BookId): AppResult<ReadingProgress?>

    /** Saves the last text position for a book. */
    suspend fun saveReadingProgress(progress: ReadingProgress): AppResult<Unit>

    /** Reads the last audio position for a book. */
    suspend fun listeningProgress(bookId: BookId): AppResult<ListeningProgress?>

    /** Saves the last audio position for a book. */
    suspend fun saveListeningProgress(progress: ListeningProgress): AppResult<Unit>
}

/** Persistence contract for reading activity used by streak calculations. */
interface ReadingSessionRepository {
    /** Lists recorded sessions for a book. */
    suspend fun listForBook(bookId: BookId): AppResult<List<ReadingSession>>

    /** Saves a completed reading session. */
    suspend fun save(session: ReadingSession): AppResult<Unit>
}

/** Asynchronous audio playback boundary implemented by each platform. */
interface AudioPlayer {
    /** Starts playback at an audio-file URI. */
    suspend fun play(uri: String): AppResult<Unit>

    /** Pauses current playback. */
    suspend fun pause(): AppResult<Unit>

    /** Seeks to a non-negative position in milliseconds. */
    suspend fun seek(positionMillis: Long): AppResult<Unit>

    /** Returns the current position in milliseconds. */
    suspend fun positionMillis(): AppResult<Long>
}

/** On-device speech recognition boundary; implementations must not upload user audio. */
interface SpeechRecognizer {
    /** Transcribes a bounded audio fragment asynchronously and supports coroutine cancellation. */
    suspend fun transcribe(
        audioUri: String,
        startMillis: Long,
        durationMillis: Long,
    ): AppResult<RecognizedSpeech>
}

/** Text and timing metadata returned by an ASR engine. */
data class RecognizedSpeech(
    val text: String,
    val startMillis: Long,
    val endMillis: Long,
    val engineVersion: String,
) {
    init {
        require(text.isNotBlank()) { "Recognized text must not be blank." }
        require(startMillis >= 0 && endMillis > startMillis) { "Speech timestamps are invalid." }
        require(engineVersion.isNotBlank()) { "ASR engine version must not be blank." }
    }
}

/** Files and metadata requested for a single book import. */
data class BookImportRequest(
    val bookUri: String,
    val bookExtension: String,
    val audioFiles: List<AudioImportFile> = emptyList(),
)

/** One user-selected audiobook file and its intended chapter order. */
data class AudioImportFile(
    val uri: String,
    val extension: String,
    val chapterIndex: Int,
)

/** Result of importing a book and its optional audio tracks. */
data class ImportedBook(
    val book: Book,
    val audioTracks: List<AudioTrack>,
)

/** Import boundary for user-selected book and audio files. */
interface BookImporter {
    /** Imports the selected book and optional audio files without fetching remote content. */
    suspend fun import(request: BookImportRequest): AppResult<ImportedBook>
}

/** Conversion progress reported by a background converter. */
data class ConversionProgress(
    val completed: Double,
) {
    init {
        require(completed in 0.0..1.0) { "Conversion progress must be between zero and one." }
    }
}

/** Supported output formats for PDF conversion. */
enum class ConversionFormat {
    EPUB,
    FB2,
}

/** Background conversion boundary for supported book formats. */
interface BookConverter {
    /** Converts a PDF into the selected output format. */
    suspend fun convert(
        sourceUri: String,
        outputFormat: ConversionFormat,
    ): AppResult<ConversionProgress>
}

/** Persistence contract for cached text/audio alignment anchors. */
interface AlignmentIndexRepository {
    /** Loads a compatible cached index or returns null when no index is available. */
    suspend fun find(
        bookId: BookId,
        audioFingerprint: String,
    ): AppResult<AlignmentIndex?>

    /** Stores an index for a particular book/audio pair. */
    suspend fun save(
        bookId: BookId,
        audioFingerprint: String,
        index: AlignmentIndex,
    ): AppResult<Unit>
}

/** User identity exposed to the application without authentication UI assumptions. */
data class LocalUser(
    val id: String,
    val isLocalOnly: Boolean,
) {
    init {
        require(id.isNotBlank()) { "User ID must not be blank." }
    }
}

/** Authentication extension point; the MVP implementation returns a local-only user. */
interface AuthService {
    /** Returns the current local or authenticated user. */
    suspend fun currentUser(): AppResult<LocalUser>
}

/** Feature identifiers used by the entitlement boundary. */
enum class Feature {
    BOOK_IMPORT,
    AUDIO_SYNC,
    PDF_CONVERSION,
}

/** Subscription and feature-flag extension point. */
interface EntitlementService {
    /** Indicates whether the current user can use a feature. */
    suspend fun isEnabled(feature: Feature): AppResult<Boolean>
}
