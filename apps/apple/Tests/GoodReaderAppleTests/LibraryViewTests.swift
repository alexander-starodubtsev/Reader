import XCTest
@testable import GoodReaderApple

final class LibraryViewTests: XCTestCase {
    func testLibraryViewUsesTheProductName() {
        _ = LibraryView(store: TestLibraryStore())
        XCTAssertEqual(GoodReaderAppleMetadata.applicationName, "GoodReader")
    }

    func testNavigationHasFourLocalizedSections() {
        XCTAssertEqual(
            GoodReaderSection.allCases.map(\.rawValue),
            ["home", "library", "addBook", "profile"]
        )
        let englishBundlePath = Bundle.module.path(forResource: "en", ofType: "lproj")
        let englishBundle = englishBundlePath.flatMap(Bundle.init(path:))
        let russianBundlePath = Bundle.module.path(forResource: "ru", ofType: "lproj")
        let russianBundle = russianBundlePath.flatMap(Bundle.init(path:))

        XCTAssertEqual(
            englishBundle?.localizedString(forKey: "tab.home", value: nil, table: "Localizable"),
            "Home"
        )
        XCTAssertEqual(
            russianBundle?.localizedString(forKey: "tab.addBook", value: nil, table: "Localizable"),
            "Загрузить книгу"
        )
    }

    @MainActor
    func testViewModelLoadsSearchResultsFromTheStore() async {
        let expected = AppleLibraryEntry(
            id: "book-1",
            title: "Test Book",
            author: "A. Writer",
            format: "EPUB",
            fileUri: "file:///books/test.epub",
            audioTrackCount: 0
        )
        let viewModel = LibraryViewModel(store: TestLibraryStore(books: [expected]))

        viewModel.search("Test")
        for _ in 0..<20 where viewModel.books.isEmpty {
            await Task.yield()
        }

        XCTAssertEqual(viewModel.books, [expected])
    }

    private struct TestLibraryStore: AppleLibraryStore {
        let books: [AppleLibraryEntry]

        init(books: [AppleLibraryEntry] = []) {
            self.books = books
        }

        func searchBooks(_ query: String?) async throws -> [AppleLibraryEntry] {
            guard let query, !query.isEmpty else { return books }
            return books.filter { $0.title.localizedCaseInsensitiveContains(query) }
        }

        func importBook(
            fileUri: String,
            title: String,
            fileExtension: String,
            audioFiles: [AppleAudioInput]
        ) async throws -> AppleLibraryEntry {
            AppleLibraryEntry(
                id: "test",
                title: title,
                author: nil,
                format: fileExtension.uppercased(),
                fileUri: fileUri,
                audioTrackCount: audioFiles.count
            )
        }
    }
}
