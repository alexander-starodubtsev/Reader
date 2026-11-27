package com.goodreader.android

import android.content.Context
import android.media.MediaPlayer
import android.net.Uri
import com.goodreader.domain.AudioTrack

/** Ordered local-track playback used by the built-in reader/player. */
internal class AndroidAudioPlayback(
    private val context: Context,
) {
    private val player = MediaPlayer()
    private var tracks: List<AudioTrack> = emptyList()
    private var activeTrack = 0
    private var elapsedBeforeTrack = 0L
    private var prepared = false
    private var onError: ((String) -> Unit)? = null

    val durationMillis: Long
        get() = tracks.sumOf(AudioTrack::durationMillis)

    val positionMillis: Long
        get() = if (prepared) (elapsedBeforeTrack + player.currentPosition).coerceIn(0L, durationMillis) else 0L

    val isPlaying: Boolean
        get() = prepared && player.isPlaying

    fun start(
        audioTracks: List<AudioTrack>,
        positionMillis: Long,
        onError: (String) -> Unit,
    ) {
        require(audioTracks.isNotEmpty())
        tracks = audioTracks.sortedBy(AudioTrack::chapterIndex)
        this.onError = onError
        seek(positionMillis)
    }

    fun pause() {
        if (player.isPlaying) player.pause()
    }

    fun togglePlayback() {
        if (!prepared) return
        if (player.isPlaying) player.pause() else player.start()
    }

    fun seek(positionMillis: Long) {
        if (tracks.isEmpty()) return
        var remaining = positionMillis.coerceIn(0L, durationMillis)
        var trackIndex = 0
        while (trackIndex < tracks.lastIndex && remaining >= tracks[trackIndex].durationMillis) {
            remaining -= tracks[trackIndex].durationMillis
            trackIndex += 1
        }
        prepareTrack(trackIndex, remaining)
    }

    fun release() {
        player.release()
    }

    private fun prepareTrack(
        trackIndex: Int,
        startMillis: Long,
    ) {
        activeTrack = trackIndex
        elapsedBeforeTrack = tracks.take(trackIndex).sumOf(AudioTrack::durationMillis)
        prepared = false
        try {
            player.reset()
            player.setOnCompletionListener {
                if (activeTrack < tracks.lastIndex) {
                    prepareTrack(activeTrack + 1, 0)
                }
            }
            player.setOnErrorListener { _, _, _ ->
                onError?.invoke("This audio file could not be played.")
                true
            }
            player.setDataSource(context, Uri.parse(tracks[trackIndex].uri))
            player.setOnPreparedListener { mediaPlayer ->
                mediaPlayer.seekTo(startMillis.coerceAtMost(tracks[trackIndex].durationMillis).toInt())
                prepared = true
                mediaPlayer.start()
            }
            player.prepareAsync()
        } catch (failure: Exception) {
            onError?.invoke(failure.localizedMessage ?: "This audio file could not be played.")
        }
    }
}
