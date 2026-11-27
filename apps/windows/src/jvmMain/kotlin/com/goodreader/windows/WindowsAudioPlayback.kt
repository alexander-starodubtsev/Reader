package com.goodreader.windows

import com.goodreader.domain.AudioTrack
import java.io.File
import java.net.URI
import javax.sound.sampled.AudioSystem
import javax.sound.sampled.Clip
import javax.sound.sampled.LineEvent

/** Plays ordered local tracks through the audio providers available to the JVM. */
internal class WindowsAudioPlayback {
    private var tracks: List<AudioTrack> = emptyList()
    private var activeTrack = 0
    private var clip: Clip? = null
    private var elapsedBeforeTrack = 0L

    val durationMillis: Long
        get() = tracks.sumOf(AudioTrack::durationMillis)

    val positionMillis: Long
        get() =
            (elapsedBeforeTrack + (clip?.microsecondPosition ?: 0L) / 1000)
                .coerceIn(0L, durationMillis)

    val isPlaying: Boolean
        get() = clip?.isRunning == true

    fun start(
        audioTracks: List<AudioTrack>,
        positionMillis: Long,
    ) {
        require(audioTracks.isNotEmpty())
        tracks = audioTracks.sortedBy(AudioTrack::chapterIndex)
        seek(positionMillis, resume = true)
    }

    fun togglePlayback() {
        val active = clip ?: return
        if (active.isRunning) active.stop() else active.start()
    }

    fun pause() {
        clip?.let { active ->
            if (active.isRunning) active.stop()
        }
    }

    fun seek(
        positionMillis: Long,
        resume: Boolean = isPlaying,
    ) {
        if (tracks.isEmpty()) return
        var remaining = positionMillis.coerceIn(0L, durationMillis)
        var trackIndex = 0
        while (trackIndex < tracks.lastIndex && remaining >= tracks[trackIndex].durationMillis) {
            remaining -= tracks[trackIndex].durationMillis
            trackIndex += 1
        }
        openTrack(trackIndex, remaining, resume)
    }

    fun close() {
        clip?.close()
        clip = null
    }

    private fun openTrack(
        trackIndex: Int,
        startMillis: Long,
        resume: Boolean,
    ) {
        clip?.close()
        activeTrack = trackIndex
        elapsedBeforeTrack = tracks.take(trackIndex).sumOf(AudioTrack::durationMillis)
        val uri = URI.create(tracks[trackIndex].uri)
        val source = if (uri.scheme == null) File(tracks[trackIndex].uri) else File(uri)
        val stream = AudioSystem.getAudioInputStream(source)
        val nextClip = AudioSystem.getClip()
        nextClip.open(stream)
        stream.close()
        clip = nextClip
        nextClip.addLineListener { event ->
            if (event.type == LineEvent.Type.STOP &&
                nextClip.framePosition >= nextClip.frameLength &&
                activeTrack < tracks.lastIndex
            ) {
                openTrack(activeTrack + 1, 0, resume = true)
            }
        }
        nextClip.microsecondPosition = startMillis * 1000
        if (resume) nextClip.start()
    }
}
