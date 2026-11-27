import SwiftUI

/// Sections shared by the iOS tab bar and desktop sidebar.
public enum GoodReaderSection: String, CaseIterable, Identifiable {
    case home
    case library
    case addBook
    case profile

    public var id: String { rawValue }

    var title: String {
        localized("tab.\(rawValue)")
    }

    var symbol: String {
        switch self {
        case .home: "house"
        case .library: "books.vertical"
        case .addBook: "plus.square"
        case .profile: "person.crop.circle"
        }
    }

    private func localized(_ key: String) -> String {
        Bundle.module.localizedString(forKey: key, value: nil, table: "Localizable")
    }
}

/// Native platform navigation with per-window selection restoration.
public struct GoodReaderRootView: View {
    @Environment(\.colorScheme) private var colorScheme
    @SceneStorage("goodreader.selectedSection") private var storedSection = GoodReaderSection.home.rawValue
    @AppStorage("goodreader.lastOpenedBook") private var lastOpenedBookJSON = ""
    @State private var lastOpenedBook: AppleLibraryEntry?
    @State private var readerBook: AppleLibraryEntry?
    private let store: AppleLibraryStore

    public init(store: AppleLibraryStore) {
        self.store = store
    }

    private var selection: Binding<GoodReaderSection> {
        Binding(
            get: { GoodReaderSection(rawValue: storedSection) ?? .home },
            set: { storedSection = $0.rawValue }
        )
    }

    public var body: some View {
        Group {
            if let readerBook {
                BookReaderView(book: readerBook, onClose: { self.readerBook = nil })
            } else {
                #if os(iOS)
                    TabView(selection: selection) {
                        HomeTab(lastOpenedBook: lastOpenedBook, onOpenBook: openBook)
                            .tabItem {
                                Label(GoodReaderSection.home.title,
                                      systemImage: GoodReaderSection.home.symbol)
                            }
                            .tag(GoodReaderSection.home)
                        LibraryView(store: store, onOpenBook: openBook)
                            .tabItem {
                                Label(GoodReaderSection.library.title,
                                      systemImage: GoodReaderSection.library.symbol)
                            }
                            .tag(GoodReaderSection.library)
                        AddBookTab(
                            store: store,
                            isActive: selection.wrappedValue == .addBook,
                            onOpenBook: openBook
                        )
                            .tabItem {
                                Label(GoodReaderSection.addBook.title,
                                      systemImage: GoodReaderSection.addBook.symbol)
                            }
                            .tag(GoodReaderSection.addBook)
                        ProfileTab()
                            .tabItem {
                                Label(GoodReaderSection.profile.title,
                                      systemImage: GoodReaderSection.profile.symbol)
                            }
                            .tag(GoodReaderSection.profile)
                    }
                    .tint(GoodReaderDesign.accent(for: colorScheme))
                #else
                    NavigationSplitView {
                        List(selection: selection) {
                            ForEach(GoodReaderSection.allCases) { section in
                                Label(section.title, systemImage: section.symbol)
                                    .tag(section)
                            }
                        }
                        .listStyle(.sidebar)
                        .navigationTitle(GoodReaderAppleMetadata.applicationName)
                    } detail: {
                        desktopContent
                    }
                    .tint(GoodReaderDesign.accent(for: colorScheme))
                #endif
            }
        }
        .task {
            restoreLastOpenedBook()
        }
    }

    @ViewBuilder
    private var desktopContent: some View {
        ZStack {
            HomeTab(lastOpenedBook: lastOpenedBook, onOpenBook: openBook)
                .opacity(selection.wrappedValue == .home ? 1 : 0)
                .allowsHitTesting(selection.wrappedValue == .home)
                .accessibilityHidden(selection.wrappedValue != .home)
            LibraryView(store: store, onOpenBook: openBook)
                .opacity(selection.wrappedValue == .library ? 1 : 0)
                .allowsHitTesting(selection.wrappedValue == .library)
                .accessibilityHidden(selection.wrappedValue != .library)
            AddBookTab(
                store: store,
                isActive: selection.wrappedValue == .addBook,
                onOpenBook: openBook
            )
                .opacity(selection.wrappedValue == .addBook ? 1 : 0)
                .allowsHitTesting(selection.wrappedValue == .addBook)
                .accessibilityHidden(selection.wrappedValue != .addBook)
            ProfileTab()
                .opacity(selection.wrappedValue == .profile ? 1 : 0)
                .allowsHitTesting(selection.wrappedValue == .profile)
                .accessibilityHidden(selection.wrappedValue != .profile)
        }
    }

    private func openBook(_ book: AppleLibraryEntry) {
        lastOpenedBook = book
        readerBook = book
        if let data = try? JSONEncoder().encode(book),
           let encoded = String(data: data, encoding: .utf8) {
            lastOpenedBookJSON = encoded
        }
    }

    private func restoreLastOpenedBook() {
        guard let data = lastOpenedBookJSON.data(using: .utf8) else { return }
        lastOpenedBook = try? JSONDecoder().decode(AppleLibraryEntry.self, from: data)
    }

    private func localized(_ key: String) -> String {
        Bundle.module.localizedString(forKey: key, value: nil, table: "Localizable")
    }
}

private struct HomeTab: View {
    @Environment(\.colorScheme) private var colorScheme
    let lastOpenedBook: AppleLibraryEntry?
    let onOpenBook: (AppleLibraryEntry) -> Void

    var body: some View {
        VStack {
            if let lastOpenedBook {
                Button {
                    onOpenBook(lastOpenedBook)
                } label: {
                    VStack(alignment: .leading, spacing: GoodReaderDesign.smallSpacing) {
                        Text(localized("home.lastOpened"))
                            .font(.subheadline)
                            .foregroundStyle(.secondary)
                        Text(lastOpenedBook.title)
                            .font(.title2.weight(.semibold))
                            .foregroundStyle(.primary)
                        if let author = lastOpenedBook.author {
                            Text(author)
                                .font(.body)
                                .foregroundStyle(.secondary)
                        }
                    }
                    .frame(maxWidth: 520, alignment: .leading)
                    .padding(GoodReaderDesign.largeSpacing)
                    .background(
                        GoodReaderDesign.cardBackground(for: colorScheme),
                        in: RoundedRectangle(cornerRadius: GoodReaderDesign.cardCornerRadius)
                    )
                    .contentShape(RoundedRectangle(cornerRadius: GoodReaderDesign.cardCornerRadius))
                }
                .buttonStyle(.plain)
                .accessibilityLabel("\(localized("home.openBook")): \(lastOpenedBook.title)")
            } else {
                Text(localized("home.empty.description"))
                    .font(.body)
                    .foregroundStyle(.secondary)
            }
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .background(GoodReaderDesign.background(for: colorScheme))
    }

    private func localized(_ key: String) -> String {
        Bundle.module.localizedString(forKey: key, value: nil, table: "Localizable")
    }
}

private struct ProfileTab: View {
    @Environment(\.colorScheme) private var colorScheme

    var body: some View {
        ContentUnavailableView(
            Bundle.module.localizedString(forKey: "profile.empty.title", value: nil, table: "Localizable"),
            systemImage: "chart.bar",
            description: Text(
                Bundle.module.localizedString(
                    forKey: "profile.empty.description",
                    value: nil,
                    table: "Localizable"
                )
            )
        )
        .padding(GoodReaderDesign.largeSpacing)
        .frame(maxWidth: 520)
        .background(
            GoodReaderDesign.cardBackground(for: colorScheme),
            in: RoundedRectangle(cornerRadius: GoodReaderDesign.cardCornerRadius)
        )
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .background(GoodReaderDesign.background(for: colorScheme))
    }
}
