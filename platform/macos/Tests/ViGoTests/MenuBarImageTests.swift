import Testing

@testable import ViGo

@Test("Menu bar icon follows the selected typing method and turns off when inactive")
func menuBarImageFollowsMethod() {
  #expect(MenuBarImage.name(method: .telex, isActive: true) == "ViGoMenuBarTelex")
  #expect(MenuBarImage.name(method: .vni, isActive: true) == "ViGoMenuBarVNI")
  #expect(MenuBarImage.name(method: .vni, isActive: false) == "ViGoMenuBarOff")
}
