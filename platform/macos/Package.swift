// swift-tools-version: 6.2

import Foundation
import PackageDescription

let packageDirectory = URL(fileURLWithPath: #filePath).deletingLastPathComponent()
let repositoryDirectory = packageDirectory.deletingLastPathComponent().deletingLastPathComponent()
let defaultCoreLibrary = repositoryDirectory
    .appendingPathComponent("target/debug/libnakl_core.a").path
let coreLibrary = ProcessInfo.processInfo.environment["NAKL_CORE_LIBRARY"] ?? defaultCoreLibrary

let package = Package(
    name: "NAKLMac",
    platforms: [.macOS(.v13)],
    products: [
        .executable(name: "NAKL", targets: ["NAKL"]),
    ],
    targets: [
        .target(
            name: "NAKLCoreBridge",
            publicHeadersPath: "include"
        ),
        .executableTarget(
            name: "NAKL",
            dependencies: ["NAKLCoreBridge"],
            linkerSettings: [
                .linkedFramework("InputMethodKit"),
                .unsafeFlags([coreLibrary]),
            ]
        ),
        .testTarget(name: "NAKLTests", dependencies: ["NAKL"]),
    ]
)
