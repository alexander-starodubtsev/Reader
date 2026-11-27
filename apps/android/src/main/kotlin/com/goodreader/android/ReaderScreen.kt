package com.goodreader.android

import android.content.Context
import android.net.Uri
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.ModalBottomSheet
import androidx.compose.material3.Slider
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableLongStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import com.goodreader.application.AppResult
import com.goodreader.application.AudioTrackRepository
import com.goodreader.domain.AudioTrack
import com.goodreader.domain.Book
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.withContext
import java.io.File
import kotlin.math.roundToInt

private const val READER_PAGE_WORDS = 500

/** Reflowable reader rendering text and stable word positions supplied by the C++ core. */
@Composable
@OptIn(ExperimentalMaterial3Api::class)
@Suppress("FunctionNaming", "ktlint:standard:function-naming")
internal fun ReaderScreen(
    book: Book,
    onClose: () -> Unit,
    audioTrackRepository: AudioTrackRepository? = null,
) {
    val context = LocalContext.current
    val readingPreferences =
        remember(book.id.value) {
            context.getSharedPreferences("goodreader.reader", Context.MODE_PRIVATE)
        }
    var document by remember(book.id.value) { mutableStateOf<NativeBookDocument?>(null) }
    var error by remember(book.id.value) { mutableStateOf<String?>(null) }
    var audioError by remember(book.id.value) { mutableStateOf<String?>(null) }
    var isLoading by remember(book.id.value) { mutableStateOf(true) }
    var currentGlobalWord by rememberSaveable(book.id.value) {
        mutableIntStateOf(readingPreferences.getInt("position-${book.id.value}", 0))
    }
    var currentChapter by remember(book.id.value) { mutableIntStateOf(0) }
    var currentText by remember(book.id.value) { mutableStateOf("") }
    var audioTracks by remember(book.id.value) { mutableStateOf<List<AudioTrack>>(emptyList()) }
    var audioPosition by rememberSaveable(book.id.value) {
        mutableLongStateOf(readingPreferences.getLong("audio-position-${book.id.value}", 0L))
    }
    var isListening by rememberSaveable(book.id.value) { mutableStateOf(false) }
    var showTools by remember { mutableStateOf(false) }
    var positionMayBeInaccurate by remember { mutableStateOf(false) }
    val audioPlayer = remember(book.id.value) { AndroidAudioPlayback(context) }

    fun useAudioPositionForReading() {
        val loaded = document ?: return
        audioPlayer.pause()
        audioPosition = audioPlayer.positionMillis
        val lastWord = (loaded.wordCount - 1).coerceAtLeast(0)
        currentGlobalWord =
            (
                audioPosition.toDouble() /
                    audioPlayer.durationMillis.coerceAtLeast(1) * lastWord
            ).roundToInt()
                .coerceIn(0, lastWord)
        isListening = false
        positionMayBeInaccurate = true
    }

    LaunchedEffect(book.id.value, book.fileUri) {
        isLoading = true
        error = null
        try {
            document = openNativeDocument(context, book)
            isLoading = false
        } catch (failure: Exception) {
            error = failure.message ?: context.getString(R.string.reader_open_error)
            isLoading = false
        }
    }

    LaunchedEffect(book.id.value, audioTrackRepository) {
        if (audioTrackRepository == null) return@LaunchedEffect
        when (val result = withContext(Dispatchers.IO) { audioTrackRepository.listForBook(book.id) }) {
            is AppResult.Success -> audioTracks = result.value.sortedBy(AudioTrack::chapterIndex)
            is AppResult.Failure -> audioError = context.getString(R.string.reader_audio_error)
        }
    }

    LaunchedEffect(book.id.value, currentGlobalWord) {
        readingPreferences.edit().putInt("position-${book.id.value}", currentGlobalWord).apply()
    }

    LaunchedEffect(isListening) {
        while (isListening) {
            audioPosition = audioPlayer.positionMillis
            readingPreferences.edit().putLong("audio-position-${book.id.value}", audioPosition).apply()
            delay(500)
        }
    }

    DisposableEffect(document) {
        onDispose { document?.close() }
    }

    DisposableEffect(audioPlayer) {
        onDispose { audioPlayer.release() }
    }

    LaunchedEffect(document, currentGlobalWord) {
        val loaded = document ?: return@LaunchedEffect
        if (loaded.wordCount == 0) {
            error = context.getString(R.string.reader_no_text)
            return@LaunchedEffect
        }
        val safeWord = currentGlobalWord.coerceIn(0, loaded.wordCount - 1)
        currentGlobalWord = safeWord
        val position =
            withContext(Dispatchers.IO) {
                val location = loaded.positionForGlobalWord(safeWord)
                location to loaded.textRange(location.first, location.second, READER_PAGE_WORDS)
            }
        currentChapter = position.first.first
        currentText = position.second
    }

    Surface(
        modifier = Modifier.fillMaxSize(),
        color = MaterialTheme.colorScheme.background,
    ) {
        Column(
            modifier = Modifier.fillMaxSize().padding(horizontal = GoodReaderSpacing.medium),
            verticalArrangement = Arrangement.spacedBy(GoodReaderSpacing.small),
        ) {
            Row(
                modifier = Modifier.fillMaxWidth().padding(vertical = GoodReaderSpacing.small),
                horizontalArrangement = Arrangement.SpaceBetween,
            ) {
                TextButton(
                    onClick = {
                        if (isListening) useAudioPositionForReading()
                        onClose()
                    },
                ) { Text(stringResource(R.string.reader_close)) }
                if (audioTracks.isNotEmpty()) {
                    TextButton(
                        onClick = {
                            val loaded = document ?: return@TextButton
                            if (isListening) {
                                useAudioPositionForReading()
                            } else {
                                val lastWord = (loaded.wordCount - 1).coerceAtLeast(1)
                                val start =
                                    audioPosition.takeIf { it > 0L }
                                        ?: (currentGlobalWord.toDouble() / lastWord * audioPlayer.durationMillis).toLong()
                                audioPlayer.start(audioTracks, start) { message ->
                                    audioError = message.ifBlank { context.getString(R.string.reader_audio_error) }
                                }
                                isListening = true
                            }
                        },
                    ) {
                        Text(
                            stringResource(
                                if (isListening) R.string.reader_read_from_here else R.string.reader_listen_from_here,
                            ),
                        )
                    }
                }
            }

            val loaded = document
            if (isLoading) {
                Text(stringResource(R.string.reader_loading))
            } else if (error != null) {
                Text(error.orEmpty(), color = MaterialTheme.colorScheme.error)
            } else if (loaded != null && loaded.chapterCount > 0) {
                if (isListening && audioTracks.isNotEmpty()) {
                    AudioPlayerControls(
                        modifier = Modifier.weight(1f).fillMaxWidth(),
                        tracks = audioTracks,
                        player = audioPlayer,
                        positionMillis = audioPosition,
                        audioError = audioError,
                        onPositionChange = { position ->
                            audioPosition = position
                            audioPlayer.seek(position)
                        },
                    )
                } else {
                    androidx.compose.foundation.layout.Box(
                        modifier =
                            Modifier
                                .weight(1f)
                                .fillMaxWidth()
                                .clickable { showTools = true },
                    ) {
                        Column(
                            modifier =
                                Modifier
                                    .fillMaxSize()
                                    .verticalScroll(rememberScrollState())
                                    .padding(vertical = GoodReaderSpacing.large),
                            verticalArrangement = Arrangement.spacedBy(GoodReaderSpacing.medium),
                        ) {
                            Text(
                                currentText,
                                modifier = Modifier.fillMaxWidth(),
                                style = MaterialTheme.typography.bodyLarge,
                            )
                        }
                        if (positionMayBeInaccurate) {
                            Text(
                                stringResource(R.string.reader_position_inaccurate),
                                modifier = Modifier.align(Alignment.TopCenter).padding(top = GoodReaderSpacing.small),
                                style = MaterialTheme.typography.labelSmall,
                                color = MaterialTheme.colorScheme.error,
                            )
                        }
                        if (audioError != null) {
                            Text(
                                audioError.orEmpty(),
                                modifier = Modifier.align(Alignment.BottomCenter).padding(GoodReaderSpacing.small),
                                style = MaterialTheme.typography.labelSmall,
                                color = MaterialTheme.colorScheme.error,
                            )
                        }
                    }
                }
            }
        }
    }

    if (showTools && document != null) {
        val loaded = requireNotNull(document)
        val lastWord = (loaded.wordCount - 1).coerceAtLeast(0)
        ModalBottomSheet(onDismissRequest = { showTools = false }) {
            Column(
                modifier = Modifier.fillMaxWidth().padding(GoodReaderSpacing.medium),
                verticalArrangement = Arrangement.spacedBy(GoodReaderSpacing.small),
            ) {
                Text(loaded.chapterTitle(currentChapter), style = MaterialTheme.typography.titleMedium)
                val progress =
                    if (lastWord == 0) 100 else currentGlobalWord * 100 / lastWord
                Text(
                    stringResource(R.string.reader_progress, progress.coerceIn(0, 100)),
                    style = MaterialTheme.typography.bodySmall,
                )
                Slider(
                    value = currentGlobalWord.toFloat().coerceIn(0f, lastWord.coerceAtLeast(1).toFloat()),
                    onValueChange = {
                        currentGlobalWord = it.toInt().coerceIn(0, lastWord)
                        positionMayBeInaccurate = false
                    },
                    valueRange = 0f..lastWord.coerceAtLeast(1).toFloat(),
                    enabled = loaded.wordCount > 1,
                )
                repeat(loaded.chapterCount) { index ->
                    TextButton(
                        onClick = {
                            currentGlobalWord = loaded.globalWordStart(index)
                            currentChapter = index
                            positionMayBeInaccurate = false
                            showTools = false
                        },
                    ) {
                        Text(loaded.chapterTitle(index))
                    }
                }
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                ) {
                    TextButton(
                        onClick = {
                            currentGlobalWord = (currentGlobalWord - READER_PAGE_WORDS).coerceAtLeast(0)
                            positionMayBeInaccurate = false
                        },
                        enabled = currentGlobalWord > 0,
                    ) { Text(stringResource(R.string.reader_previous)) }
                    Button(
                        onClick = {
                            currentGlobalWord = (currentGlobalWord + READER_PAGE_WORDS).coerceAtMost(lastWord)
                            positionMayBeInaccurate = false
                        },
                        enabled = currentGlobalWord < lastWord,
                    ) { Text(stringResource(R.string.reader_next)) }
                }
            }
        }
    }
}

@Composable
@Suppress("FunctionNaming", "ktlint:standard:function-naming")
private fun AudioPlayerControls(
    modifier: Modifier,
    tracks: List<AudioTrack>,
    player: AndroidAudioPlayback,
    positionMillis: Long,
    audioError: String?,
    onPositionChange: (Long) -> Unit,
) {
    val duration = tracks.sumOf(AudioTrack::durationMillis).coerceAtLeast(1L)
    var isPlaying by remember { mutableStateOf(true) }
    Column(
        modifier = modifier.padding(GoodReaderSpacing.large),
        verticalArrangement = Arrangement.Center,
    ) {
        Text(tracks.firstOrNull()?.uri?.substringAfterLast('/') ?: "", style = MaterialTheme.typography.titleLarge)
        if (audioError != null) {
            Text(audioError, color = MaterialTheme.colorScheme.error)
        }
        Slider(
            value = positionMillis.toFloat().coerceIn(0f, duration.toFloat()),
            onValueChange = { onPositionChange(it.toLong()) },
            valueRange = 0f..duration.toFloat(),
        )
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
        ) {
            TextButton(onClick = { onPositionChange((positionMillis - 15_000).coerceAtLeast(0)) }) {
                Text("-15s")
            }
            Button(
                onClick = {
                    player.togglePlayback()
                    isPlaying = player.isPlaying
                },
            ) {
                Text(stringResource(if (isPlaying) R.string.reader_pause_audio else R.string.reader_play_audio))
            }
            TextButton(onClick = { onPositionChange((positionMillis + 15_000).coerceAtMost(duration)) }) {
                Text("+15s")
            }
        }
    }
}

private fun NativeBookDocument.globalWordStart(chapterIndex: Int): Int = (0 until chapterIndex).sumOf(::chapterWordCount)

private suspend fun openNativeDocument(
    context: Context,
    book: Book,
): NativeBookDocument =
    withContext(Dispatchers.IO) {
        val uri = Uri.parse(book.fileUri)
        if (uri.scheme == null || uri.scheme == "file") {
            val path = uri.path ?: book.fileUri
            return@withContext NativeBookDocument.open(path)
        }

        val stagedFile =
            File.createTempFile(
                "goodreader-reader-",
                ".${book.format.name.lowercase()}",
                context.cacheDir,
            )
        try {
            val input =
                context.contentResolver.openInputStream(uri)
                    ?: throw IllegalStateException(context.getString(R.string.reader_open_error))
            input.use { source -> stagedFile.outputStream().use { target -> source.copyTo(target) } }
            NativeBookDocument.open(stagedFile.absolutePath)
        } finally {
            stagedFile.delete()
        }
    }
