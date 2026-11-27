import GoodReaderApple
import XCTest

final class GoodReaderMacAppTests: XCTestCase {
    func testNativeAppUsesTheSharedLibraryView() {
        XCTAssertEqual(GoodReaderAppleMetadata.applicationName, "GoodReader")
    }
}
