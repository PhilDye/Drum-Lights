// swift-tools-version: 5.10

import PackageDescription

let package = Package(
    name: "DrumLightsKit",
    platforms: [.iOS(.v17), .macOS(.v14), .watchOS(.v10)],
    products: [
        .library(name: "DrumLightsKit", targets: ["DrumLightsKit"])
    ],
    targets: [
        .target(
            name: "DrumLightsKit",
            resources: [.copy("Resources/modes.json")]
        ),
        .testTarget(
            name: "DrumLightsKitTests",
            dependencies: ["DrumLightsKit"]
        )
    ]
)
