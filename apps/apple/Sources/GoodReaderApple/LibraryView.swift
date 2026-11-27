import SwiftUI
import UniformTypeIdentifiers

/// Shared copy and identity used by the native Apple app targets.
public enum GoodReaderAppleMetadata {
    /// Product name displayed by the native applications.
    public static let applicationName = "GoodReader"
}

/// Native SwiftUI library with one import form for the book and optional audio chapters.
public struct LibraryView: View {
    @Environment(\.colorScheme) private var colorScheme
    @StateObject private var viewModel: LibraryViewModel
    private let onOpenBook: (AppleLibraryEntry) -> Void

    /// Creates the GoodReader library view using the supplied persistence adapter.
    public init(
        store: AppleLibraryStore,
        onOpenBook: @escaping (AppleLibraryEntry) -> Void = { _ in }
    ) {
        _viewModel = StateObject(wrappedValue: LibraryViewModel(store: store))
        self.onOpenBook = onOpenBook
    }

    /// Renders the searchable library and combined import flow.
    public var body: some View {
        NavigationStack {
            Group {
                if viewModel.books.isEmpty {
                    ContentUnavailableView(
                        viewModel.query.isEmpty ? "Your library is empty" : "No books found",
                        systemImage: "books.vertical",
                        description: Text(
                            viewModel.query.isEmpty
                                ? "Add a PDF, EPUB, or FB2 book to get started."
                                : "Try another title or author."
                        )
                    )
                } else {
                    List(viewModel.books) { book in
                        Button {
                            onOpenBook(book)
                        } label: {
                            VStack(alignment: .leading, spacing: 4) {
                                Text(book.title).font(.headline)
                                Text([book.author, book.format].compactMap { $0 }.joined(separator: " · "))
                                    .font(.subheadline)
                                    .foregroundStyle(.secondary)
                                if book.audioTrackCount > 0 {
                                    Label(
                                        "\(book.audioTrackCount) audio chapter(s)",
                                        systemImage: "headphones"
                                    )
                                    .font(.caption)
                                    .foregroundStyle(.secondary)
                                }
                            }
                            .frame(maxWidth: .infinity, alignment: .leading)
                            .contentShape(Rectangle())
                        }
                        .buttonStyle(.plain)
                        .padding(.vertical, 4)
                        .listRowBackground(
                            GoodReaderDesign.cardBackground(for: colorScheme)
                        )
                    }
                    .listStyle(.plain)
                    .scrollContentBackground(.hidden)
                    .background(GoodReaderDesign.background(for: colorScheme))
                }
            }
            .navigationTitle("Library")
            .searchable(text: $viewModel.query, prompt: "Search books")
            .onChange(of: viewModel.query) { _, value in
                viewModel.search(value)
            }
            .overlay(alignment: .bottom) {
                if let message = viewModel.message {
                    Text(message)
                        .font(.callout)
                        .padding()
                        .frame(maxWidth: .infinity)
                        .background(.regularMaterial, in: RoundedRectangle(cornerRadius: 12))
                        .padding()
                }
            }
            .task {
                viewModel.loadBooks()
            }
            .sheet(
                isPresented: $viewModel.isImportDialogPresented,
                onDismiss: viewModel.cancelImport
            ) {
                ImportBookForm(viewModel: viewModel)
                .interactiveDismissDisabled(viewModel.isImporting || viewModel.isStagingFiles)
            }
            .background(GoodReaderDesign.background(for: colorScheme))
        }
    }
}

private struct ImportBookForm: View {
    @ObservedObject var viewModel: LibraryViewModel
    @Environment(\.colorScheme) private var colorScheme
    @State private var isBookPickerPresented = false
    @State private var isAudioPickerPresented = false

    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(alignment: .leading, spacing: GoodReaderDesign.mediumSpacing) {
                    Text("Book")
                        .font(.headline)
                    Button {
                        isBookPickerPresented = true
                    } label: {
                        Label(
                            viewModel.selectedBookName ?? "Choose PDF, EPUB, or FB2",
                            systemImage: "book"
                        )
                    }
                    .buttonStyle(.bordered)
                    .disabled(viewModel.isImporting || viewModel.isStagingFiles)

                    VStack(alignment: .leading, spacing: GoodReaderDesign.smallSpacing) {
                        Text("Audio (optional)")
                            .font(.headline)
                        Button {
                            isAudioPickerPresented = true
                        } label: {
                            Label(
                                viewModel.selectedAudioNames.isEmpty
                                    ? "Choose audio chapters"
                                    : "Replace \(viewModel.selectedAudioNames.count) audio file(s)",
                                systemImage: "headphones"
                            )
                        }
                        .buttonStyle(.bordered)
                        .disabled(viewModel.isImporting || viewModel.isStagingFiles)
                        if !viewModel.selectedAudioNames.isEmpty {
                            ForEach(Array(viewModel.selectedAudioNames.enumerated()), id: \.offset) { _, name in
                                Text(name)
                                    .font(.caption)
                                    .foregroundStyle(.secondary)
                            }
                            Button("Remove audio", role: .destructive, action: viewModel.clearAudio)
                                .disabled(viewModel.isImporting || viewModel.isStagingFiles)
                        }
                    }
                    .padding(GoodReaderDesign.mediumSpacing)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .background(
                        GoodReaderDesign.cardBackground(for: colorScheme),
                        in: RoundedRectangle(cornerRadius: GoodReaderDesign.cardCornerRadius)
                    )

                    if let message = viewModel.message {
                        Text(message)
                            .foregroundStyle(.red)
                    }
                    if viewModel.isStagingFiles {
                        ProgressView("Preparing selected files...")
                    }
                    if viewModel.isImporting {
                        ProgressView("Adding book...")
                    }
                    Button(action: viewModel.importSelectedFiles) {
                        if viewModel.isImporting {
                            ProgressView()
                        } else {
                            Text("Add")
                        }
                    }
                    .buttonStyle(.borderedProminent)
                    .disabled(viewModel.selectedBookName == nil ||
                              viewModel.isImporting || viewModel.isStagingFiles)
                }
            }
            .padding(GoodReaderDesign.largeSpacing)
            .frame(maxWidth: .infinity, maxHeight: .infinity)
            .background(GoodReaderDesign.background(for: colorScheme))
            .navigationTitle("Add to Library")
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Cancel") {
                        viewModel.cancelImport()
                        viewModel.isImportDialogPresented = false
                    }
                        .disabled(viewModel.isImporting || viewModel.isStagingFiles)
                }
                ToolbarItem(placement: .confirmationAction) {
                    Button("Add", action: viewModel.importSelectedFiles)
                        .disabled(viewModel.selectedBookName == nil ||
                                  viewModel.isImporting || viewModel.isStagingFiles)
                }
            }
        }
        .frame(minWidth: 360, minHeight: 300)
        .fileImporter(
            isPresented: $isBookPickerPresented,
            allowedContentTypes: Self.bookContentTypes,
            allowsMultipleSelection: false
        ) { result in
            switch result {
            case .success(let urls):
                if let url = urls.first {
                    viewModel.stageBook(url)
                }
            case .failure(let error):
                viewModel.reportPickerError(error)
            }
        }
        .fileImporter(
            isPresented: $isAudioPickerPresented,
            allowedContentTypes: Self.audioContentTypes,
            allowsMultipleSelection: true
        ) { result in
            switch result {
            case .success(let urls):
                viewModel.stageAudio(urls)
            case .failure(let error):
                viewModel.reportPickerError(error)
            }
        }
    }

    private static var bookContentTypes: [UTType] {
        [.pdf, UTType(filenameExtension: "epub") ?? .item, UTType(filenameExtension: "fb2") ?? .item]
    }

    private static var audioContentTypes: [UTType] {
        ["mp3", "m4a", "m4b", "ogg", "wav"].map { UTType(filenameExtension: $0) ?? .audio }
    }
}

/// Dedicated tab for adding a book and its optional audio tracks.
public struct AddBookTab: View {
    @StateObject private var viewModel: LibraryViewModel
    @Environment(\.colorScheme) private var colorScheme
    private let isActive: Bool
    private let onOpenBook: (AppleLibraryEntry) -> Void

    /// Creates the add-book flow using the native library store.
    public init(
        store: AppleLibraryStore,
        isActive: Bool = true,
        onOpenBook: @escaping (AppleLibraryEntry) -> Void = { _ in }
    ) {
        _viewModel = StateObject(wrappedValue: LibraryViewModel(store: store))
        self.isActive = isActive
        self.onOpenBook = onOpenBook
    }

    public var body: some View {
        Group {
            if viewModel.isImportDialogPresented {
                ImportBookForm(viewModel: viewModel)
            } else {
                ContentUnavailableView(
                    "Book added",
                    systemImage: "checkmark.circle",
                    description: Text(viewModel.message ?? "Your book is ready in the library.")
                )
                .overlay(alignment: .bottom) {
                    HStack {
                        if let importedBook = viewModel.lastImportedBook {
                            Button("Open book") {
                                onOpenBook(importedBook)
                            }
                            .buttonStyle(.borderedProminent)
                        }
                        Button("Add another book") {
                            viewModel.isImportDialogPresented = true
                        }
                    }
                    .padding(.bottom, 24)
                }
            }
        }
        .background(GoodReaderDesign.background(for: colorScheme))
        .onAppear {
            if isActive && viewModel.message == nil {
                viewModel.isImportDialogPresented = true
            }

        }
        .onChange(of: isActive) { _, active in
            if active && viewModel.message == nil {
                viewModel.isImportDialogPresented = true
            }
        }
    }
}
