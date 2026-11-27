package com.goodreader.android

import java.io.Closeable
import java.nio.charset.StandardCharsets

/** Parsed book content owned by the C++ core. */
internal class NativeBookDocument private constructor(
    private var handle: Long,
) : Closeable {
    internal val nativeHandle: Long
        get() {
            check(handle != 0L) { "The reader document is closed." }
            return handle
        }

    val title: String
        get() = bytes(NativeCore.documentTitle(handle))

    val chapterCount: Int
        get() = NativeCore.documentChapterCount(handle)

    val wordCount: Int
        get() = NativeCore.documentWordCount(handle)

    fun chapterTitle(chapterIndex: Int): String = bytes(NativeCore.documentChapterTitle(handle, chapterIndex))

    fun chapterWordCount(chapterIndex: Int): Int = NativeCore.documentChapterWordCount(handle, chapterIndex)

    fun textRange(
        chapterIndex: Int,
        firstWord: Int,
        maxWords: Int,
    ): String = bytes(NativeCore.documentTextRange(handle, chapterIndex, firstWord, maxWords))

    fun positionForGlobalWord(globalWordIndex: Int): Pair<Int, Int> {
        val result = NativeCore.positionForGlobalWord(handle, globalWordIndex)
        check(result.size == 2) { "Word position is outside the book." }
        return result[0] to result[1]
    }

    fun createAlignmentIndex(anchors: List<NativeAlignmentAnchor>): NativeAlignmentIndex {
        require(anchors.size >= 2) { "At least two alignment anchors are required." }
        val handle =
            NativeCore.createAlignmentIndex(
                anchors.map { it.globalWordIndex.toLong() }.toLongArray(),
                anchors.map { it.audioTimeMillis }.toLongArray(),
                anchors.map { it.confidence }.toDoubleArray(),
            )
        check(handle != 0L) { "The audio alignment anchors are invalid." }
        return NativeAlignmentIndex(this, handle)
    }

    override fun close() {
        if (handle != 0L) {
            NativeCore.closeBookDocument(handle)
            handle = 0L
        }
    }

    internal data class NativeAlignmentAnchor(
        val globalWordIndex: Int,
        val audioTimeMillis: Long,
        val confidence: Double,
    )

    internal class NativeAlignmentIndex(
        private val document: NativeBookDocument,
        private var handle: Long,
    ) : Closeable {
        fun positionAtAudioTime(audioTimeMillis: Long): Pair<Int, Int> {
            val result =
                NativeCore.positionAtAudioTime(document.nativeHandle, requireOpen(), audioTimeMillis)
            check(result.size == 2) { "The audio position could not be mapped to the book." }
            return result[0] to result[1]
        }

        fun audioTimeForPosition(
            chapterIndex: Int,
            wordIndex: Int,
        ): Long {
            val time =
                NativeCore.audioTimeForPosition(
                    document.nativeHandle,
                    requireOpen(),
                    chapterIndex,
                    wordIndex,
                )
            check(time >= 0) { "The book position could not be mapped to the audio." }
            return time
        }

        override fun close() {
            if (handle != 0L) {
                NativeCore.closeAlignmentIndex(handle)
                handle = 0L
            }
        }

        private fun requireOpen(): Long {
            check(handle != 0L) { "The audio alignment index is closed." }
            return handle
        }
    }

    private fun bytes(value: ByteArray?): String {
        check(handle != 0L) { "The reader document is closed." }
        return requireNotNull(value) { "The reader could not load book content." }
            .toString(StandardCharsets.UTF_8)
    }

    companion object {
        fun open(path: String): NativeBookDocument =
            NativeBookDocument(NativeCore.openBookDocument(path.toByteArray(StandardCharsets.UTF_8)))
    }
}
