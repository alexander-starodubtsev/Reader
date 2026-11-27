import GoodReaderApple
import SwiftUI

@main
struct GoodReaderiOSApp: App {
    var body: some Scene {
        WindowGroup {
            GoodReaderRootView(store: AppleKotlinLibraryStore())
        }
    }
}
