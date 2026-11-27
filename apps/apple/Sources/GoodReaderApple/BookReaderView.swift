import AVFoundation
import SwiftUI

struct BookReaderView: View {
    @Environment(\.colorScheme) private var colorScheme
    @StateObject private var viewModel: BookReaderViewModel
    @State private var firstParagraph: Int
    @State private var isListening = false
    @State private var showTools = false
    @State private var positionMayBeInaccurate = false
    @State private var audioStartMillis: Int64 = 0
    @State private var audioCurrentPosition: Int64 = 0
    private let book: AppleLibraryEntry
    private let onClose: () -> Void

    init(book: AppleLibraryEntry, onClose: @escaping () -> Void) {
        self.book = book
        self.onClose = onClose
        _viewModel = StateObject(wrappedValue: BookReaderViewModel(book: book))
        _firstParagraph = State(
            initialValue: UserDefaults.standard.integer(forKey: Self.progressKey(for: book.id))
        )
        _audioCurrentPosition = State(
            initialValue: UserDefaults.standard.object(forKey: "goodreader.reader.\(book.id).audioMillis") as? Int64 ?? 0
        )
    }

    var body: some View {
        VStack(spacing: GoodReaderDesign.smallSpacing) {
            HStack {
                Button(action: onClose) {
                    Label(localized("reader.back"), systemImage: "chevron.left")
                }
                .buttonStyle(.plain)
                Spacer()
                if !book.audioTracks.isEmpty {
                    Button {
                        if isListening {
                            if let document = viewModel.document {
                                firstParagraph =
                                    paragraphAtAudioPosition(
                                        audioCurrentPosition,
                                        book.audioTracks.reduce(Int64(0)) { $0 + $1.durationMillis },
                                        in: document
                                    )
                                positionMayBeInaccurate = true
                                isListening = false
                            }
                        } else if let document = viewModel.document {
                            audioStartMillis = audioPosition(for: firstParagraph, in: document)
                            isListening = true
                            positionMayBeInaccurate = false
                        }
                    } label: {
                        Text(localized(isListening ? "reader.readFromHere" : "reader.listenFromHere"))
                    }
                    .buttonStyle(.plain)
                }
            }
            .frame(height: 40)

            if viewModel.isLoading {
                ProgressView(localized("reader.opening"))
                    .frame(maxWidth: .infinity, maxHeight: .infinity)
            } else if let error = viewModel.error {
                ContentUnavailableView(
                    localized("reader.openError.title"),
                    systemImage: "exclamationmark.book.closed",
                    description: Text(error)
                )
                .frame(maxWidth: .infinity, maxHeight: .infinity)
            } else if let document = viewModel.document {
                if isListening {
                    AppleAudioPlayerView(
                        tracks: book.audioTracks,
                        startPositionMillis: audioStartMillis,
                        onPositionChange: { position in
                            audioCurrentPosition = position
                            UserDefaults.standard.set(position, forKey: "goodreader.reader.\(book.id).audioMillis")
                        },
                        onReadFromHere: { position, duration in
                            firstParagraph = paragraphAtAudioPosition(position, duration, in: document)
                            positionMayBeInaccurate = true
                            isListening = false
                        }
                    )
                } else {
                    readerContent(document)
                }
            }
        }
        .padding(.horizontal, GoodReaderDesign.largeSpacing)
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .background(GoodReaderDesign.background(for: colorScheme))
        .task {
            await viewModel.load()
        }
        .onChange(of: firstParagraph) { _, value in
            UserDefaults.standard.set(value, forKey: Self.progressKey(for: book.id))
        }
        .sheet(isPresented: $showTools) {
            if let document = viewModel.document {
                readerTools(document)
                    .presentationDetents([.medium, .large])
                    .presentationDragIndicator(.visible)
            }
        }
    }

    @ViewBuilder
    private func readerContent(_ document: BookTextDocument) -> some View {
        let safeStart = min(firstParagraph, document.paragraphs.count - 1)
        let end = min(safeStart + 10, document.paragraphs.count)
        let pageText = document.paragraphs[safeStart..<end].joined(separator: "\n\n")

        ZStack(alignment: .top) {
            ScrollView {
                Text(pageText)
                    .font(.body)
                    .lineSpacing(6)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .textSelection(.enabled)
                    .padding(GoodReaderDesign.largeSpacing)
            }
            .background(
                GoodReaderDesign.cardBackground(for: colorScheme),
                in: RoundedRectangle(cornerRadius: GoodReaderDesign.cardCornerRadius)
            )
            .contentShape(Rectangle())
            .onTapGesture { showTools = true }

            if positionMayBeInaccurate {
                Text(localized("reader.positionInaccurate"))
                    .font(.caption)
                    .padding(.horizontal, GoodReaderDesign.mediumSpacing)
                    .padding(.vertical, GoodReaderDesign.smallSpacing)
                    .background(.regularMaterial, in: Capsule())
                    .padding(.top, GoodReaderDesign.smallSpacing)
                    .accessibilityAddTraits(.updatesFrequently)
            }
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }

    private func readerTools(_ document: BookTextDocument) -> some View {
        let lastParagraph = max(0, document.paragraphs.count - 1)
        return VStack(alignment: .leading, spacing: GoodReaderDesign.mediumSpacing) {
            Text(document.title)
                .font(.headline)
            Text("\(Int(Double(firstParagraph) / Double(max(document.paragraphs.count, 1)) * 100))%")
                .font(.caption)
                .foregroundStyle(.secondary)
            Slider(
                value: Binding(
                    get: { Double(firstParagraph) },
                    set: { firstParagraph = Int($0) }
                ),
                in: 0...Double(lastParagraph)
            )
            HStack {
                Button(localized("reader.previous")) {
                    firstParagraph = max(0, firstParagraph - 10)
                    positionMayBeInaccurate = false
                }
                .disabled(firstParagraph == 0)
                Spacer()
                Button(localized("reader.next")) {
                    firstParagraph = min(lastParagraph, firstParagraph + 10)
                    positionMayBeInaccurate = false
                }
                .disabled(firstParagraph >= lastParagraph)
            }
            Spacer(minLength: 0)
        }
        .padding(GoodReaderDesign.largeSpacing)
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
        .background(GoodReaderDesign.background(for: colorScheme))
    }

    private func audioPosition(for paragraph: Int, in document: BookTextDocument) -> Int64 {
        let wordIndex = document.paragraphs.prefix(paragraph).reduce(0) { $0 + wordCount($1) }
        let totalWords = document.paragraphs.reduce(0) { $0 + wordCount($1) }
        let totalDuration = book.audioTracks.reduce(Int64(0)) { $0 + $1.durationMillis }
        guard totalWords > 1 else { return 0 }
        return Int64(Double(wordIndex) / Double(totalWords - 1) * Double(totalDuration))
    }

    private func paragraphAtAudioPosition(
        _ position: Int64,
        _ duration: Int64,
        in document: BookTextDocument
    ) -> Int {
        let totalWords = document.paragraphs.reduce(0) { $0 + wordCount($1) }
        let targetWord = Int(Double(position) / Double(max(duration, 1)) * Double(max(totalWords - 1, 0)))
        var wordsSeen = 0
        for (index, paragraph) in document.paragraphs.enumerated() {
            wordsSeen += wordCount(paragraph)
            if wordsSeen > targetWord { return index }
        }
        return max(0, document.paragraphs.count - 1)
    }

    private func wordCount(_ text: String) -> Int {
        text.split(whereSeparator: \.isWhitespace).count
    }

    private func localized(_ key: String) -> String {
        Bundle.module.localizedString(forKey: key, value: nil, table: "Localizable")
    }

    private static func progressKey(for bookID: String) -> String {
        "goodreader.reader.\(bookID).paragraph"
    }
}

private struct AppleAudioPlayerView: View {
    @StateObject private var model = AppleAudioPlayerModel()
    let tracks: [AppleLibraryAudioTrack]
    let startPositionMillis: Int64
    let onPositionChange: (Int64) -> Void
    let onReadFromHere: (Int64, Int64) -> Void

    var body: some View {
        VStack(spacing: GoodReaderDesign.largeSpacing) {
            Spacer()
            Image(systemName: "headphones")
                .font(.system(size: 44))
                .foregroundStyle(.secondary)
            Text(localized("reader.audiobook"))
                .font(.title2.weight(.semibold))
            if let error = model.error {
                Text(error)
                    .font(.footnote)
                    .foregroundStyle(.red)
            }
            Slider(
                value: Binding(
                    get: { Double(model.positionMillis) },
                    set: { model.seek(to: Int64($0)) }
                ),
                in: 0...Double(max(model.durationMillis, 1))
            )
            HStack {
                Text(timeString(model.positionMillis))
                Spacer()
                Text(timeString(model.durationMillis))
            }
            .font(.caption)
            HStack(spacing: GoodReaderDesign.largeSpacing) {
                Button {
                    model.seek(to: max(0, model.positionMillis - 15_000))
                } label: {
                    Image(systemName: "gobackward.15")
                }
                .accessibilityLabel(localized("reader.skipBack"))
                Button {
                    model.togglePlayback()
                } label: {
                    Image(systemName: model.isPlaying ? "pause.fill" : "play.fill")
                        .font(.title)
                }
                .accessibilityLabel(localized(model.isPlaying ? "reader.pause" : "reader.play"))
                Button {
                    model.seek(to: min(model.durationMillis, model.positionMillis + 15_000))
                } label: {
                    Image(systemName: "goforward.15")
                }
                .accessibilityLabel(localized("reader.skipForward"))
            }
            Spacer()
            Button(localized("reader.readFromHere")) {
                onReadFromHere(model.positionMillis, model.durationMillis)
            }
            .buttonStyle(.borderedProminent)
        }
        .padding(GoodReaderDesign.largeSpacing)
        .task {
            model.start(tracks: tracks, at: startPositionMillis)
        }
        .onReceive(model.$positionMillis) { onPositionChange($0) }
        .onDisappear {
            model.stop()
        }
    }

    private func timeString(_ millis: Int64) -> String {
        let seconds = max(0, millis / 1000)
        return String(format: "%d:%02d", seconds / 60, seconds % 60)
    }

    private func localized(_ key: String) -> String {
        Bundle.module.localizedString(forKey: key, value: nil, table: "Localizable")
    }
}

@MainActor
private final class AppleAudioPlayerModel: ObservableObject {
    @Published private(set) var positionMillis: Int64 = 0
    @Published private(set) var durationMillis: Int64 = 0
    @Published private(set) var isPlaying = false
    @Published private(set) var error: String?

    private var player: AVQueuePlayer?
    private var items: [AVPlayerItem] = []
    private var tracks: [AppleLibraryAudioTrack] = []
    private var timeObserver: Any?

    func start(tracks: [AppleLibraryAudioTrack], at position: Int64) {
        guard player == nil else { return }
        self.tracks = tracks.sorted { $0.chapterIndex < $1.chapterIndex }
        durationMillis = self.tracks.reduce(0) { $0 + $1.durationMillis }
        items = self.tracks.compactMap { track in
            guard let url = URL(string: track.uri), url.isFileURL else { return nil }
            return AVPlayerItem(url: url)
        }
        guard items.count == self.tracks.count, !items.isEmpty else {
            error = "An audio chapter is not available as a local file."
            return
        }
        player = AVQueuePlayer(items: items)
        timeObserver = player?.addPeriodicTimeObserver(
            forInterval: CMTime(seconds: 0.25, preferredTimescale: 600),
            queue: .main
        ) { [weak self] _ in
            Task { @MainActor in self?.updatePosition() }
        }
        seek(to: position)
    }

    func togglePlayback() {
        guard let player else { return }
        if isPlaying {
            player.pause()
            isPlaying = false
        } else {
            player.play()
            isPlaying = true
        }
    }

    func seek(to position: Int64) {
        guard let player, !items.isEmpty else { return }
        let target = min(max(position, 0), durationMillis)
        var remaining = target
        var index = 0
        while index < tracks.count - 1 && remaining >= tracks[index].durationMillis {
            remaining -= tracks[index].durationMillis
            index += 1
        }
        player.pause()
        player.removeAllItems()
        for item in items.dropFirst(index) {
            player.insert(item, after: nil)
        }
        player.seek(to: CMTime(milliseconds: remaining))
        player.play()
        isPlaying = true
        positionMillis = target
    }

    func stop() {
        player?.pause()
        if let timeObserver, let player {
            player.removeTimeObserver(timeObserver)
        }
        timeObserver = nil
        player = nil
        items = []
        isPlaying = false
    }

    private func updatePosition() {
        guard let player, let current = player.currentItem,
              let index = items.firstIndex(where: { $0 === current }) else {
            isPlaying = false
            return
        }
        let priorDuration = tracks.prefix(index).reduce(Int64(0)) { $0 + $1.durationMillis }
        let localTime = player.currentTime().seconds
        guard localTime.isFinite else { return }
        positionMillis = min(durationMillis, priorDuration + Int64(localTime * 1000))
        isPlaying = player.rate > 0
    }
}

private extension CMTime {
    init(milliseconds: Int64) {
        self.init(value: milliseconds, timescale: 1000)
    }
}

@MainActor
private final class BookReaderViewModel: ObservableObject {
    @Published private(set) var document: BookTextDocument?
    @Published private(set) var error: String?
    @Published private(set) var isLoading = true

    private let book: AppleLibraryEntry

    init(book: AppleLibraryEntry) {
        self.book = book
    }

    func load() async {
        guard isLoading else { return }
        do {
            guard let url = URL(string: book.fileUri), url.isFileURL else {
                throw ReaderError.corruptBook("The saved book location is not a local file URL.")
            }
            guard FileManager.default.fileExists(atPath: url.path) else {
                throw ReaderError.corruptBook("The book file is missing at \(url.path).")
            }
            let loaded = try await Task.detached(priority: .userInitiated) {
                try BookTextDocument.open(url)
            }.value
            document = loaded
        } catch {
            self.error = error.localizedDescription
        }
        isLoading = false
    }
}
