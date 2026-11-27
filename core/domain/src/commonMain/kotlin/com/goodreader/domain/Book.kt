package com.goodreader.domain

/** Stable identifier for a book stored in the local library. */
data class BookId(
    val value: String,
) {
    init {
        require(value.isNotBlank()) { "Book ID must not be blank." }
    }
}

/** Supported text file formats. */
enum class BookFormat {
    PDF,
    EPUB,
    FB2,
    ;

    companion object {
        /** Returns the supported format for a file extension, or null when unsupported. */
        fun fromExtension(extension: String): BookFormat? =
            entries.firstOrNull { it.name.equals(extension.removePrefix("."), ignoreCase = true) }
    }
}

/** A locally imported book and its user-visible metadata. */
data class Book(
    val id: BookId,
    val title: String,
    val author: String?,
    val format: BookFormat,
    val fileUri: String,
    val wordCount: Int?,
    val audioTrackCount: Int = 0,
) {
    init {
        require(title.isNotBlank()) { "Book title must not be blank." }
        require(fileUri.isNotBlank()) { "Book file URI must not be blank." }
        require(wordCount == null || wordCount >= 0) { "Word count must not be negative." }
        require(audioTrackCount >= 0) { "Audio track count must not be negative." }
    }
}

/** Stable identifier for an audio file associated with a book. */
data class AudioTrackId(
    val value: String,
) {
    init {
        require(value.isNotBlank()) { "Audio track ID must not be blank." }
    }
}

/** Supported audio file formats. */
enum class AudioFormat {
    MP3,
    M4A,
    M4B,
    OGG,
    WAV,
    ;

    companion object {
        /** Returns the supported format for a file extension, or null when unsupported. */
        fun fromExtension(extension: String): AudioFormat? =
            entries.firstOrNull { it.name.equals(extension.removePrefix("."), ignoreCase = true) }
    }
}

/** An audio file in a book's ordered audiobook, with duration in milliseconds. */
data class AudioTrack(
    val id: AudioTrackId,
    val bookId: BookId,
    val uri: String,
    val format: AudioFormat,
    val durationMillis: Long,
    val chapterIndex: Int,
) {
    init {
        require(uri.isNotBlank()) { "Audio URI must not be blank." }
        require(durationMillis > 0) { "Audio duration must be positive." }
        require(chapterIndex >= 0) { "Chapter index must not be negative." }
    }
}
