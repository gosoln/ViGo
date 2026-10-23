// swift-tools-version: 6.2

import Foundation
import PackageDescription

let packageDirectory = URL(fileURLWithPath: #filePath).deletingLastPathComponent()
let repositoryDirectory = packageDirectory.deletingLastPathComponent().deletingLastPathComponent()
let defaultCoreLibrary = repositoryDirectory
    .appendingPathComponent("target/debug/libvigo_core.a").path
let coreLibrary = ProcessInfo.processInfo.environment["VIGO_CORE_LIBRARY"] ?? defaultCoreLibrary

let package = Package(
    name: "ViGoMac",
    platforms: [.macOS(.v13)],
    products: [
        .executable(name: "ViGo", targets: ["ViGo"]),
        .executable(
            name: "ViGoCompatibilityFixture",
            targets: ["ViGoCompatibilityFixture"]
        ),
    ],
    targets: [
        .target(
            name: "ViGoCoreBridge",
            publicHeadersPath: "include"
        ),
        .executableTarget(
            name: "ViGo",
            dependencies: ["ViGoCoreBridge"],
            linkerSettings: [
                .linkedFramework("Carbon"),
                .linkedFramework("InputMethodKit"),
                .unsafeFlags([coreLibrary]),
            ]
        ),
        .executableTarget(name: "ViGoCompatibilityFixture"),
        .testTarget(name: "ViGoTests", dependencies: ["ViGo"]),
    ]
)
