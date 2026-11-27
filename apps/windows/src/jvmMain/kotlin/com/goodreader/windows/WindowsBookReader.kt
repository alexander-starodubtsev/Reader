package com.goodreader.windows

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
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.goodreader.application.AppResult
import com.goodreader.application.AudioTrackRepository
import com.goodreader.domain.AudioTrack
import com.goodreader.domain.Book
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.apache.pdfbox.Loader
import org.apache.pdfbox.text.PDFTextStripper
import org.w3c.dom.Element
import org.xml.sax.InputSource
import java.io.File
import java.io.StringReader
import java.net.URI
import java.nio.file.Paths
import java.util.prefs.Preferences
import java.util.zip.ZipFile
import javax.xml.XMLConstants
import javax.xml.parsers.DocumentBuilderFactory

private const val READER_PARAGRAPHS_PER_PAGE = 10

internal data class WindowsBookTextDocument(
    val title: String,
    val author: String?,
    val paragraphs: List<String>,
) {
    companion object {
        fun open(file: File): WindowsBookTextDocument =
            when (file.extension.lowercase()) {
                "pdf" -> openPdf(file)
                "fb2" -> openFb2(file)
                "epub" -> openEpub(file)
                else -> error("The selected book format is not supported.")
            }

        private fun openPdf(file: File): WindowsBookTextDocument {
            Loader.loadPDF(file).use { pdf ->
                val paragraphs =
                    PDFTextStripper()
                        .getText(pdf)
                        .lineSequence()
                        .map(String::trim)
                        .filter(String::isNotEmpty)
                        .toList()
                require(paragraphs.isNotEmpty()) {
                    "This PDF has no selectable text. Scanned PDFs cannot be displayed yet."
                }
                return WindowsBookTextDocument(file.nameWithoutExtension, null, paragraphs)
            }
        }

        private fun openFb2(file: File): WindowsBookTextDocument {
            val root = parseXml(file.readText())
            val title =
                root
                    .elements("book-title")
                    .firstOrNull()
                    ?.textContent
                    ?.trim()
                    ?.takeIf(String::isNotEmpty) ?: file.nameWithoutExtension
            val author =
                root.elements("author").firstOrNull()?.let { author ->
                    listOf("first-name", "middle-name", "last-name")
                        .mapNotNull { name ->
                            author
                                .elements(name)
                                .firstOrNull()
                                ?.textContent
                                ?.trim()
                        }.filter(String::isNotEmpty)
                        .joinToString(" ")
                        .takeIf(String::isNotEmpty)
                }
            val paragraphs = root.elements("p").mapNotNull { it.textContent?.trim()?.takeIf(String::isNotEmpty) }
            require(paragraphs.isNotEmpty()) { "This FB2 file contains no readable text." }
            return WindowsBookTextDocument(title, author, paragraphs)
        }

        private fun openEpub(file: File): WindowsBookTextDocument =
            ZipFile(file).use { zip ->
                val container =
                    zip.getEntry("META-INF/container.xml")
                        ?: error("This EPUB is missing its package manifest.")
                val packagePath =
                    zip
                        .getInputStream(container)
                        .bufferedReader()
                        .use { it.readText() }
                        .let(::parseXml)
                        .elements("rootfile")
                        .firstOrNull()
                        ?.getAttribute("full-path")
                        ?.takeIf(String::isNotEmpty)
                        ?: error("This EPUB has an invalid package manifest.")
                val packageDocument =
                    zip.getEntry(packagePath)?.let { entry ->
                        zip
                            .getInputStream(entry)
                            .bufferedReader()
                            .use { it.readText() }
                            .let(::parseXml)
                    } ?: error("This EPUB package document is missing.")
                val manifest =
                    packageDocument.elements("item").associate { item ->
                        item.getAttribute("id") to item.getAttribute("href")
                    }
                val base = Paths.get(packagePath).parent
                val paragraphs = mutableListOf<String>()
                packageDocument.elements("itemref").forEach { itemRef ->
                    val href = manifest[itemRef.getAttribute("idref")] ?: return@forEach
                    val chapterPath =
                        base
                            ?.resolve(href)
                            ?.normalize()
                            ?.toString()
                            ?.replace('\\', '/') ?: href
                    val entry = zip.getEntry(chapterPath) ?: return@forEach
                    val chapter =
                        zip
                            .getInputStream(entry)
                            .bufferedReader()
                            .use { it.readText() }
                            .let(::parseXml)
                    paragraphs += chapter.readableParagraphs()
                }
                require(paragraphs.isNotEmpty()) { "This EPUB contains no readable text." }
                val metadata = packageDocument.elements("metadata").firstOrNull()
                val title =
                    metadata
                        ?.elements("title")
                        ?.firstOrNull()
                        ?.textContent
                        ?.trim()
                        ?.takeIf(String::isNotEmpty) ?: file.nameWithoutExtension
                val author =
                    metadata
                        ?.elements("creator")
                        ?.firstOrNull()
                        ?.textContent
                        ?.trim()
                        ?.takeIf(String::isNotEmpty)
                WindowsBookTextDocument(title, author, paragraphs)
            }

        private fun parseXml(source: String) =
            DocumentBuilderFactory
                .newInstance()
                .apply {
                    isNamespaceAware = true
                    setFeature("http://xml.org/sax/features/external-general-entities", false)
                    setFeature("http://xml.org/sax/features/external-parameter-entities", false)
                    setFeature("http://apache.org/xml/features/nonvalidating/load-external-dtd", false)
                    setAttribute(XMLConstants.ACCESS_EXTERNAL_DTD, "")
                    setAttribute(XMLConstants.ACCESS_EXTERNAL_SCHEMA, "")
                    setXIncludeAware(false)
                    isExpandEntityReferences = false
                }.newDocumentBuilder()
                .parse(InputSource(StringReader(source)))
                .documentElement

        private fun Element.elements(localName: String): List<Element> {
            val nodes = getElementsByTagNameNS("*", localName)
            if (nodes.length > 0) return (0 until nodes.length).mapNotNull { nodes.item(it) as? Element }
            val plain = getElementsByTagName(localName)
            return (0 until plain.length).mapNotNull { plain.item(it) as? Element }
        }

        private fun Element.readableParagraphs(): List<String> {
            val readableTags = setOf("h1", "h2", "h3", "p", "li", "blockquote")
            val nodes = getElementsByTagName("*")
            return (0 until nodes.length).mapNotNull { index ->
                val element = nodes.item(index) as? Element ?: return@mapNotNull null
                val name = (element.localName ?: element.tagName.substringAfter(':')).lowercase()
                if (name !in readableTags || element.hasReadableAncestor(readableTags)) {
                    null
                } else {
                    element.textContent?.trim()?.takeIf(String::isNotEmpty)
                }
            }
        }

        private fun Element.hasReadableAncestor(readableTags: Set<String>): Boolean {
            var ancestor = parentNode
            while (ancestor is Element) {
                val name = (ancestor.localName ?: ancestor.tagName.substringAfter(':')).lowercase()
                if (name in readableTags) return true
                ancestor = ancestor.parentNode
            }
            return false
        }
    }
}

@Composable
@OptIn(ExperimentalMaterial3Api::class)
@Suppress("FunctionNaming", "ktlint:standard:function-naming")
internal fun WindowsBookReaderScreen(
    book: Book,
    audioTrackRepository: AudioTrackRepository? = null,
    onClose: () -> Unit,
) {
    val preferences = remember { Preferences.userRoot().node("com/goodreader/reader") }
    val key = "position-${book.id.value}"
    var firstParagraph by remember(book.id.value) { mutableIntStateOf(preferences.getInt(key, 0)) }
    var document by remember(book.id.value) { mutableStateOf<WindowsBookTextDocument?>(null) }
    var error by remember(book.id.value) { mutableStateOf<String?>(null) }
    var loading by remember(book.id.value) { mutableStateOf(true) }
    var audioTracks by remember(book.id.value) { mutableStateOf<List<AudioTrack>>(emptyList()) }
    var audioError by remember(book.id.value) { mutableStateOf<String?>(null) }
    var isListening by remember(book.id.value) { mutableStateOf(false) }
    var showTools by remember(book.id.value) { mutableStateOf(false) }
    var positionMayBeInaccurate by remember(book.id.value) { mutableStateOf(false) }
    var audioPosition by remember(book.id.value) {
        mutableLongStateOf(preferences.getLong("audio-position-${book.id.value}", 0L))
    }
    val audioPlayer = remember(book.id.value) { WindowsAudioPlayback() }
    val coroutineScope = rememberCoroutineScope()

    fun useAudioPositionForReading() {
        val loaded = document ?: return
        audioPosition = audioPlayer.positionMillis
        audioPlayer.pause()
        firstParagraph =
            paragraphAtAudioPosition(
                audioPosition,
                audioPlayer.durationMillis.coerceAtLeast(1L),
                loaded,
            )
        preferences.putLong("audio-position-${book.id.value}", audioPosition)
        isListening = false
        positionMayBeInaccurate = true
    }

    LaunchedEffect(book.id.value, book.fileUri) {
        loading = true
        error = null
        try {
            val file = File(URI.create(book.fileUri))
            document = withContext(Dispatchers.IO) { WindowsBookTextDocument.open(file) }
        } catch (failure: Exception) {
            error = failure.message ?: "The book could not be displayed."
        } finally {
            loading = false
        }
    }
    LaunchedEffect(book.id.value, audioTrackRepository) {
        if (audioTrackRepository == null) return@LaunchedEffect
        when (val result = withContext(Dispatchers.IO) { audioTrackRepository.listForBook(book.id) }) {
            is AppResult.Success -> audioTracks = result.value.sortedBy(AudioTrack::chapterIndex)
            is AppResult.Failure -> audioError = text("reader.audioError")
        }
    }
    LaunchedEffect(book.id.value, firstParagraph) {
        preferences.putInt(key, firstParagraph)
    }
    LaunchedEffect(book.id.value, isListening) {
        while (isListening) {
            audioPosition = audioPlayer.positionMillis
            preferences.putLong("audio-position-${book.id.value}", audioPosition)
            delay(500)
        }
    }
    DisposableEffect(audioPlayer) {
        onDispose { audioPlayer.close() }
    }

    Surface(Modifier.fillMaxSize(), color = MaterialTheme.colorScheme.background) {
        Column(
            Modifier.fillMaxSize().padding(horizontal = 24.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            Row(
                Modifier.fillMaxWidth().padding(vertical = 8.dp),
                horizontalArrangement = Arrangement.SpaceBetween,
            ) {
                TextButton(
                    onClick = {
                        if (isListening) useAudioPositionForReading()
                        onClose()
                    },
                ) { Text(text("reader.back")) }
                if (audioTracks.isNotEmpty()) {
                    TextButton(
                        onClick = {
                            val loaded = document ?: return@TextButton
                            if (isListening) {
                                useAudioPositionForReading()
                            } else {
                                val start = audioTimeForParagraph(firstParagraph, loaded, audioTracks)
                                coroutineScope.launch {
                                    try {
                                        withContext(Dispatchers.IO) { audioPlayer.start(audioTracks, start) }
                                        audioError = null
                                        isListening = true
                                    } catch (failure: Exception) {
                                        audioError = failure.message ?: text("reader.audioError")
                                    }
                                }
                            }
                        },
                    ) {
                        Text(text(if (isListening) "reader.readFromHere" else "reader.listenFromHere"))
                    }
                }
            }
            when {
                loading -> Text(text("reader.opening"))
                error != null -> Text(error.orEmpty(), color = MaterialTheme.colorScheme.error)
                document != null -> {
                    val loaded = requireNotNull(document)
                    if (isListening && audioTracks.isNotEmpty()) {
                        WindowsAudioPlayerControls(
                            modifier = Modifier.weight(1f).fillMaxWidth(),
                            tracks = audioTracks,
                            player = audioPlayer,
                            positionMillis = audioPosition,
                            audioError = audioError,
                            onPositionChange = { position ->
                                audioPosition = position
                                coroutineScope.launch {
                                    runCatching { withContext(Dispatchers.IO) { audioPlayer.seek(position) } }
                                        .onFailure { audioError = it.message ?: text("reader.audioError") }
                                }
                            },
                            onTogglePlayback = {
                                coroutineScope.launch {
                                    runCatching { withContext(Dispatchers.IO) { audioPlayer.togglePlayback() } }
                                        .onFailure { audioError = it.message ?: text("reader.audioError") }
                                }
                            },
                        )
                    } else {
                        val safeStart =
                            firstParagraph.coerceIn(0, loaded.paragraphs.lastIndex.coerceAtLeast(0))
                        val end = (safeStart + READER_PARAGRAPHS_PER_PAGE).coerceAtMost(loaded.paragraphs.size)
                        androidx.compose.foundation.layout.Box(
                            Modifier.weight(1f).fillMaxWidth().clickable { showTools = true },
                        ) {
                            Column(
                                Modifier
                                    .fillMaxSize()
                                    .verticalScroll(rememberScrollState())
                                    .padding(vertical = 24.dp),
                                verticalArrangement = Arrangement.spacedBy(12.dp),
                            ) {
                                loaded.paragraphs.subList(safeStart, end).forEach { paragraph ->
                                    Text(paragraph, style = MaterialTheme.typography.bodyLarge)
                                }
                            }
                            if (positionMayBeInaccurate) {
                                Text(
                                    text("reader.positionInaccurate"),
                                    modifier = Modifier.align(Alignment.TopCenter).padding(top = 8.dp),
                                    style = MaterialTheme.typography.labelSmall,
                                    color = MaterialTheme.colorScheme.error,
                                )
                            }
                            if (audioError != null) {
                                Text(
                                    audioError.orEmpty(),
                                    modifier = Modifier.align(Alignment.BottomCenter).padding(8.dp),
                                    color = MaterialTheme.colorScheme.error,
                                )
                            }
                        }
                    }
                }
            }
        }
    }
    if (showTools && document != null) {
        val loaded = requireNotNull(document)
        val lastParagraph = loaded.paragraphs.lastIndex.coerceAtLeast(0)
        ModalBottomSheet(onDismissRequest = { showTools = false }) {
            Column(
                Modifier.fillMaxWidth().padding(24.dp),
                verticalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                Text(loaded.title, style = MaterialTheme.typography.titleMedium)
                Text(
                    "${firstParagraph * 100 / loaded.paragraphs.size.coerceAtLeast(1)}%",
                    style = MaterialTheme.typography.bodySmall,
                )
                Slider(
                    value = firstParagraph.toFloat(),
                    onValueChange = {
                        firstParagraph = it.toInt()
                        positionMayBeInaccurate = false
                    },
                    valueRange = 0f..lastParagraph.toFloat().coerceAtLeast(1f),
                )
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                    TextButton(
                        onClick = {
                            firstParagraph = (firstParagraph - READER_PARAGRAPHS_PER_PAGE).coerceAtLeast(0)
                            positionMayBeInaccurate = false
                        },
                    ) { Text(text("reader.previous")) }
                    Button(
                        onClick = {
                            firstParagraph =
                                (firstParagraph + READER_PARAGRAPHS_PER_PAGE).coerceAtMost(lastParagraph)
                            positionMayBeInaccurate = false
                        },
                    ) { Text(text("reader.next")) }
                }
            }
        }
    }
}

@Composable
@Suppress("FunctionNaming", "ktlint:standard:function-naming")
private fun WindowsAudioPlayerControls(
    modifier: Modifier,
    tracks: List<AudioTrack>,
    player: WindowsAudioPlayback,
    positionMillis: Long,
    audioError: String?,
    onPositionChange: (Long) -> Unit,
    onTogglePlayback: () -> Unit,
) {
    val duration = tracks.sumOf(AudioTrack::durationMillis).coerceAtLeast(1L)
    Column(modifier.padding(24.dp), verticalArrangement = Arrangement.Center) {
        Text(tracks.firstOrNull()?.uri?.substringAfterLast('/') ?: "", style = MaterialTheme.typography.titleLarge)
        if (audioError != null) Text(audioError, color = MaterialTheme.colorScheme.error)
        Slider(
            value = positionMillis.toFloat().coerceIn(0f, duration.toFloat()),
            onValueChange = { onPositionChange(it.toLong()) },
            valueRange = 0f..duration.toFloat(),
        )
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
            TextButton(onClick = { onPositionChange((positionMillis - 15_000).coerceAtLeast(0)) }) {
                Text("-15s")
            }
            Button(onClick = onTogglePlayback) {
                Text(text(if (player.isPlaying) "reader.pause" else "reader.play"))
            }
            TextButton(onClick = { onPositionChange((positionMillis + 15_000).coerceAtMost(duration)) }) {
                Text("+15s")
            }
        }
    }
}

private fun audioTimeForParagraph(
    paragraphIndex: Int,
    document: WindowsBookTextDocument,
    tracks: List<AudioTrack>,
): Long {
    val totalWords = document.paragraphs.sumOf { it.split(Regex("\\s+")).size }
    val wordsBefore = document.paragraphs.take(paragraphIndex).sumOf { it.split(Regex("\\s+")).size }
    val duration = tracks.sumOf(AudioTrack::durationMillis)
    return if (totalWords <= 1) 0 else (wordsBefore.toDouble() / (totalWords - 1) * duration).toLong()
}

private fun paragraphAtAudioPosition(
    positionMillis: Long,
    durationMillis: Long,
    document: WindowsBookTextDocument,
): Int {
    val totalWords = document.paragraphs.sumOf { it.split(Regex("\\s+")).size }
    val target = (positionMillis.toDouble() / durationMillis * (totalWords - 1).coerceAtLeast(0)).toInt()
    var wordsSeen = 0
    document.paragraphs.forEachIndexed { index, paragraph ->
        wordsSeen += paragraph.split(Regex("\\s+")).size
        if (wordsSeen > target) return index
    }
    return document.paragraphs.lastIndex.coerceAtLeast(0)
}
