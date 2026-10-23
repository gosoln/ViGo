import AppKit

@MainActor
final class ActiveApplicationMonitor: NSObject, ObservableObject {
  @Published private(set) var activeApplication: ExcludedApplication?

  private let ownBundleIdentifier: String?
  private let workspace: NSWorkspace

  init(
    ownBundleIdentifier: String? = Bundle.main.bundleIdentifier,
    workspace: NSWorkspace = .shared,
    observeWorkspace: Bool = true
  ) {
    self.ownBundleIdentifier = ownBundleIdentifier
    self.workspace = workspace
    super.init()

    record(workspace.frontmostApplication)
    if observeWorkspace {
      workspace.notificationCenter.addObserver(
        self,
        selector: #selector(activeApplicationDidChange(_:)),
        name: NSWorkspace.didActivateApplicationNotification,
        object: nil
      )
    }
  }

  func recordActivation(bundleIdentifier: String?, displayName: String?) {
    guard let bundleIdentifier, bundleIdentifier != ownBundleIdentifier else { return }
    activeApplication = ExcludedApplication(
      bundleIdentifier: bundleIdentifier,
      displayName: displayName ?? bundleIdentifier
    )
  }

  @objc private func activeApplicationDidChange(_ notification: Notification) {
    let application =
      notification.userInfo?[NSWorkspace.applicationUserInfoKey] as? NSRunningApplication
      ?? workspace.frontmostApplication
    record(application)
  }

  private func record(_ application: NSRunningApplication?) {
    recordActivation(
      bundleIdentifier: application?.bundleIdentifier,
      displayName: application?.localizedName
    )
  }
}
