import GoodReaderApple
import XCTest

final class GoodReaderAppTests: XCTestCase {
    func testNativeAppUsesTheSharedLibraryView() {
        XCTAssertEqual(GoodReaderAppleMetadata.applicationName, "GoodReader")
    }
}
