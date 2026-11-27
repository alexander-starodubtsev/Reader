import GoodReaderApple
import SwiftUI

@main
struct GoodReadermacOSApp: App {
    var body: some Scene {
        WindowGroup {
            GoodReaderRootView(store: AppleKotlinLibraryStore())
        }
    }
}
