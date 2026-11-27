// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "GoodReaderApple",
    defaultLocalization: "en",
    platforms: [
        .iOS(.v17),
        .macOS(.v14),
    ],
    products: [
        .library(name: "GoodReaderApple", targets: ["GoodReaderApple"]),
    ],
    targets: [
        .target(
            name: "GoodReaderZip",
            publicHeadersPath: "include",
            linkerSettings: [.linkedLibrary("z")]
        ),
        .target(
            name: "GoodReaderApple",
            dependencies: ["GoodReaderZip"],
            resources: [.process("Resources")]
        ),
        .testTarget(name: "GoodReaderAppleTests", dependencies: ["GoodReaderApple"]),
    ]
)
