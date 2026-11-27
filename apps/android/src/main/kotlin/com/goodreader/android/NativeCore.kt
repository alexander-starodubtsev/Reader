package com.goodreader.android

/** Thin JNI access to the shared C++ core. */
internal object NativeCore {
    const val STATUS_OK = 0
    const val STATUS_UNSUPPORTED_FORMAT = 2
    const val STATUS_NOT_FOUND = 3

    init {
        System.loadLibrary("goodreader_jni")
    }

    /** Validates a UTF-8 encoded local book path and returns a stable C API status code. */
    @JvmStatic
    external fun validateBookFile(pathUtf8: ByteArray): Int

    @JvmStatic
    external fun openBookDocument(pathUtf8: ByteArray): Long

    @JvmStatic
    external fun closeBookDocument(handle: Long)

    @JvmStatic
    external fun documentTitle(handle: Long): ByteArray

    @JvmStatic
    external fun documentChapterCount(handle: Long): Int

    @JvmStatic
    external fun documentWordCount(handle: Long): Int

    @JvmStatic
    external fun documentChapterTitle(
        handle: Long,
        chapterIndex: Int,
    ): ByteArray

    @JvmStatic
    external fun documentChapterWordCount(
        handle: Long,
        chapterIndex: Int,
    ): Int

    @JvmStatic
    external fun documentTextRange(
        handle: Long,
        chapterIndex: Int,
        firstWord: Int,
        maxWords: Int,
    ): ByteArray

    @JvmStatic
    external fun positionForGlobalWord(
        handle: Long,
        globalWordIndex: Int,
    ): IntArray

    @JvmStatic
    external fun createAlignmentIndex(
        globalWordIndices: LongArray,
        audioTimesMillis: LongArray,
        confidences: DoubleArray,
    ): Long

    @JvmStatic
    external fun closeAlignmentIndex(handle: Long)

    @JvmStatic
    external fun positionAtAudioTime(
        documentHandle: Long,
        alignmentHandle: Long,
        audioTimeMillis: Long,
    ): IntArray

    @JvmStatic
    external fun audioTimeForPosition(
        documentHandle: Long,
        alignmentHandle: Long,
        chapterIndex: Int,
        wordIndex: Int,
    ): Long
}
