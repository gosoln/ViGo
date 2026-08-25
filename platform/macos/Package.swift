// swift-tools-version: 6.2

import PackageDescription

let package = Package(
    name: "NAKLMac",
    platforms: [.macOS(.v13)],
    products: [
        .executable(name: "NAKL", targets: ["NAKL"]),
    ],
    targets: [
        .executableTarget(name: "NAKL"),
        .testTarget(name: "NAKLTests", dependencies: ["NAKL"]),
    ]
)
