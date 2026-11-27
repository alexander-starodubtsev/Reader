import Combine
import Foundation

@MainActor
public final class LibraryViewModel: ObservableObject {
    @Published public var query = ""
    @Published public var books: [AppleLibraryEntry] = []
    @Published public var isImportDialogPresented = false
    @Published public private(set) var selectedBookName: String?
    @Published public private(set) var selectedAudioNames: [String] = []
    @Published public private(set) var isImporting = false
    @Published public private(set) var isStagingFiles = false
    @Published public private(set) var message: String?
    @Published public private(set) var lastImportedBook: AppleLibraryEntry?

    private let store: AppleLibraryStore
    private let fileStore = AppleSelectedFileStore()
    private var selectedBook: StagedBook?
    private var selectedAudio: [StagedAudio] = []
    private var searchTask: Task<Void, Never>?
    private var searchRevision = 0

    /// Creates a view model backed by an injected library store.
    public init(store: AppleLibraryStore) {
        self.store = store
    }

    /// Loads books using the current search query.
    public func loadBooks() {
        search(query)
    }

    /// Displays errors returned from the native file picker.
    public func reportPickerError(_ error: Error) {
        let code = (error as NSError).code
        guard code != NSUserCancelledError else { return }
        message = error.localizedDescription
    }

    /// Searches with cancellation and guards against stale query results.
    public func search(_ value: String) {
        query = value
        message = nil
        searchRevision += 1
        let revision = searchRevision
        searchTask?.cancel()
        searchTask = Task {
            do {
                let result = try await store.searchBooks(value)
                guard revision == searchRevision else { return }
                books = result
            } catch {
                guard revision == searchRevision else { return }
                message = error.localizedDescription
            }
        }
    }

    /// Stages a user-selected book into app-owned storage for durable access.
    public func stageBook(_ source: URL) {
        guard ["pdf", "epub", "fb2"].contains(source.pathExtension.lowercased()) else {
            message = "Choose a PDF, EPUB, or FB2 book file."
            return
        }
        isStagingFiles = true
        Task {
            var staged: URL?
            do {
                let copiedFile = try await fileStore.stage(source)
                staged = copiedFile
                if let previous = selectedBook {
                    try await fileStore.remove([previous.url])
                }
                selectedBook = StagedBook(
                    url: copiedFile,
                    displayName: source.deletingPathExtension().lastPathComponent
                )
                selectedBookName = selectedBook?.displayName
                message = nil
                isStagingFiles = false
            } catch {
                isStagingFiles = false
                if let staged {
                    do {
                        try await fileStore.remove([staged])
                    } catch {
                        message = "Could not stage the selected book or clean up its temporary copy."
                        return
                    }
                }
                message = error.localizedDescription
            }
        }
    }

    /// Stages optional audio files and extracts their durations off the UI thread.
    public func stageAudio(_ sources: [URL]) {
        guard sources.allSatisfy({ ["mp3", "m4a", "m4b", "ogg", "wav"].contains($0.pathExtension.lowercased()) }) else {
            message = "Choose MP3, M4A, M4B, OGG, or WAV audio files."
            return
        }
        isStagingFiles = true
        Task {
            var staged: [StagedAudio] = []
            do {
                let sortedSources = sources.sorted {
                    $0.lastPathComponent.localizedStandardCompare($1.lastPathComponent) == .orderedAscending
                }
                for source in sortedSources {
                    let url = try await fileStore.stage(source)
                    do {
                        let duration = try await AppleSelectedFileStore.durationMillis(for: url)
                        staged.append(
                            StagedAudio(
                                url: url,
                                displayName: source.lastPathComponent,
                                durationMillis: duration
                            )
                        )
                    } catch {
                        try await fileStore.remove([url])
                        throw error
                    }
                }
                try await fileStore.remove(selectedAudio.map(\.url))
                selectedAudio = staged
                selectedAudioNames = staged.map(\.displayName)
                message = nil
                isStagingFiles = false
            } catch {
                isStagingFiles = false
                do {
                    try await fileStore.remove(staged.map(\.url))
                    message = error.localizedDescription
                } catch {
                    message = "Audio selection failed and temporary files could not be removed."
                }
            }
        }
    }

    /// Removes optional audio from the pending pairing.
    public func clearAudio() {
        let previous = selectedAudio.map(\.url)
        selectedAudio = []
        selectedAudioNames = []
        Task {
            do {
                try await fileStore.remove(previous)
            } catch {
                message = error.localizedDescription
            }
        }
    }

    /// Imports the staged book and optional audio files through the SQLDelight facade.
    public func importSelectedFiles() {
        guard let book = selectedBook else {
            message = "Choose a PDF, EPUB, or FB2 book file."
            return
        }
        isImporting = true
        message = nil
        Task {
            do {
                let audio = selectedAudio.enumerated().map { index, file in
                    AppleAudioInput(
                        uri: file.url.absoluteString,
                        fileExtension: file.url.pathExtension.lowercased(),
                        durationMillis: file.durationMillis,
                        chapterIndex: index
                    )
                }
                let importedBook = try await store.importBook(
                    fileUri: book.url.absoluteString,
                    title: book.displayName,
                    fileExtension: book.url.pathExtension.lowercased(),
                    audioFiles: audio
                )
                lastImportedBook = importedBook
                selectedBook = nil
                selectedAudio = []
                selectedBookName = nil
                selectedAudioNames = []
                isImporting = false
                isImportDialogPresented = false
                message = "Added \(book.displayName) to your library."
                loadBooks()
            } catch {
                isImporting = false
                message = error.localizedDescription
            }
        }
    }

    /// Discards staged selections when the import form is cancelled.
    public func cancelImport() {
        guard selectedBook != nil || !selectedAudio.isEmpty else { return }
        let previous = [selectedBook?.url].compactMap { $0 } + selectedAudio.map(\.url)
        selectedBook = nil
        selectedAudio = []
        selectedBookName = nil
        selectedAudioNames = []
        Task {
            do {
                try await fileStore.remove(previous)
            } catch {
                message = error.localizedDescription
            }
        }
    }
}

struct StagedBook {
    let url: URL
    let displayName: String
}

struct StagedAudio {
    let url: URL
    let displayName: String
    let durationMillis: Int64
}
