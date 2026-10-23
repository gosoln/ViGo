#!/usr/bin/env swift
// Generates the ViGo app icon and menu-bar template images.
// Usage: swift assets/icon/generate-icon.swift <output-directory>
import AppKit
import CoreText

let canvas: CGFloat = 1024

func bitmap(_ pixels: Int, width: Int? = nil, height: Int? = nil, draw: (CGContext) -> Void)
  -> NSBitmapImageRep
{
  let rep = NSBitmapImageRep(
    bitmapDataPlanes: nil, pixelsWide: width ?? pixels, pixelsHigh: height ?? pixels,
    bitsPerSample: 8,
    samplesPerPixel: 4, hasAlpha: true, isPlanar: false, colorSpaceName: .deviceRGB,
    bytesPerRow: 0, bitsPerPixel: 0)!
  let context = NSGraphicsContext(bitmapImageRep: rep)!
  NSGraphicsContext.current = context
  let cg = context.cgContext
  if width == nil { cg.scaleBy(x: CGFloat(pixels) / canvas, y: CGFloat(pixels) / canvas) }
  draw(cg)
  NSGraphicsContext.current = nil
  return rep
}

func color(_ hex: UInt32, _ alpha: CGFloat = 1) -> CGColor {
  CGColor(
    red: CGFloat((hex >> 16) & 0xFF) / 255, green: CGFloat((hex >> 8) & 0xFF) / 255,
    blue: CGFloat(hex & 0xFF) / 255, alpha: alpha)
}

func roundedPath(_ rect: CGRect, _ radius: CGFloat) -> CGPath {
  CGPath(roundedRect: rect, cornerWidth: radius, cornerHeight: radius, transform: nil)
}

func gradient(_ top: UInt32, _ bottom: UInt32) -> CGGradient {
  CGGradient(
    colorsSpace: CGColorSpaceCreateDeviceRGB(), colors: [color(top), color(bottom)] as CFArray,
    locations: [0, 1])!
}

/// Bounds of the glyph "e" for the heavy rounded system font at `size`, plus a drawer.
struct Glyph {
  let line: CTLine
  let bounds: CGRect

  init(size: CGFloat) {
    let base = NSFont.systemFont(ofSize: size, weight: .heavy)
    let font = base.fontDescriptor.withDesign(.rounded).flatMap { NSFont(descriptor: $0, size: size) } ?? base
    let text = NSAttributedString(
      string: "e",
      attributes: [
        .font: font,
        NSAttributedString.Key(kCTForegroundColorFromContextAttributeName as String): true,
      ])
    line = CTLineCreateWithAttributedString(text)
    bounds = CTLineGetBoundsWithOptions(line, .useGlyphPathBounds)
  }

  func draw(in cg: CGContext, centerX: CGFloat, baselineBottom: CGFloat) {
    cg.textPosition = CGPoint(x: centerX - bounds.midX, y: baselineBottom - bounds.minY)
    CTLineDraw(line, cg)
  }
}

/// Draws "ê" (letter plus circumflex) with its visual center at `center`.
/// The caller sets the fill/stroke colors; `hat` is stroked separately.
func drawCircumflexE(
  _ cg: CGContext, center: CGPoint, letterSize: CGFloat, hatWidth: CGFloat,
  hatColor: CGColor
) {
  let glyph = Glyph(size: letterSize)
  let gap = letterSize * 0.025
  let hatHeight = letterSize * 0.15
  let total = glyph.bounds.height + gap + hatHeight + hatWidth
  let bottom = center.y - total / 2
  glyph.draw(in: cg, centerX: center.x, baselineBottom: bottom)

  let hatBase = bottom + glyph.bounds.height + gap + hatWidth / 2
  let halfSpan = glyph.bounds.width * 0.36
  cg.saveGState()
  cg.setStrokeColor(hatColor)
  cg.setLineWidth(hatWidth)
  cg.setLineCap(.round)
  cg.setLineJoin(.round)
  cg.move(to: CGPoint(x: center.x - halfSpan, y: hatBase))
  cg.addLine(to: CGPoint(x: center.x, y: hatBase + hatHeight))
  cg.addLine(to: CGPoint(x: center.x + halfSpan, y: hatBase))
  cg.strokePath()
  cg.restoreGState()
}

func drawAppIcon(_ cg: CGContext) {
  // macOS icon grid: 824pt body centered in the 1024pt canvas. Three colors only:
  // key gray, letter white, circumflex gold.
  let body = CGRect(x: 100, y: 100, width: 824, height: 824)

  cg.saveGState()
  cg.setShadow(offset: CGSize(width: 0, height: -14), blur: 28, color: color(0x000000, 0.35))
  cg.setFillColor(color(0x3A3A41))
  cg.addPath(roundedPath(body, 185))
  cg.fillPath()
  cg.restoreGState()

  cg.setFillColor(color(0xFFFFFF))
  drawCircumflexE(
    cg, center: CGPoint(x: body.midX, y: body.midY), letterSize: 660, hatWidth: 62,
    hatColor: color(0xFFC83D))
}

func labelLine(_ text: String, size: CGFloat) -> (line: CTLine, bounds: CGRect) {
  let font = NSFont.systemFont(ofSize: size, weight: .bold)
  let line = CTLineCreateWithAttributedString(
    NSAttributedString(
      string: text,
      attributes: [
        .font: font,
        NSAttributedString.Key(kCTForegroundColorFromContextAttributeName as String): true,
      ]))
  return (line, CTLineGetBoundsWithOptions(line, .useGlyphPathBounds))
}

/// Largest size at which every label fits `width`, so all labels share one cap height.
func commonLabelSize(_ labels: [String], width: CGFloat, maxSize: CGFloat) -> CGFloat {
  var size = maxSize
  while size > 4, labels.contains(where: { labelLine($0, size: size).bounds.width > width }) {
    size -= 0.5
  }
  return size
}

/// Draws `text` centered in `rect` using the context fill color.
func drawLabel(_ cg: CGContext, _ text: String, in rect: CGRect, size: CGFloat) {
  let (line, bounds) = labelLine(text, size: size)
  cg.textPosition = CGPoint(x: rect.midX - bounds.midX, y: rect.midY - bounds.midY)
  CTLineDraw(line, cg)
}

/// 52x36 px template image (26x18pt @2x) in the style of macOS input-source icons:
/// a filled key with the label knocked out, or an outlined key when typing is off.
func drawMenuBar(_ cg: CGContext, label: String, size: CGFloat, filled: Bool) {
  let key = CGRect(x: 2, y: 3, width: 48, height: 30)
  cg.setFillColor(color(0x000000))
  cg.setStrokeColor(color(0x000000))
  if filled {
    cg.addPath(roundedPath(key, 8))
    cg.fillPath()
    cg.setBlendMode(.clear)
    drawLabel(cg, label, in: key.insetBy(dx: 5, dy: 4), size: size)
    cg.setBlendMode(.normal)
  } else {
    cg.setLineWidth(3)
    cg.addPath(roundedPath(key.insetBy(dx: 1.5, dy: 1.5), 7))
    cg.strokePath()
    drawLabel(cg, label, in: key.insetBy(dx: 5, dy: 4), size: size)
  }
}

func write(_ rep: NSBitmapImageRep, to url: URL) throws {
  try rep.representation(using: .png, properties: [:])!.write(to: url)
}

guard CommandLine.arguments.count == 2 else {
  FileHandle.standardError.write(Data("usage: generate-icon.swift <output-directory>\n".utf8))
  exit(2)
}
let output = URL(fileURLWithPath: CommandLine.arguments[1], isDirectory: true)
try FileManager.default.createDirectory(at: output, withIntermediateDirectories: true)

let iconset = output.appendingPathComponent("ViGo.iconset", isDirectory: true)
try? FileManager.default.removeItem(at: iconset)
try FileManager.default.createDirectory(at: iconset, withIntermediateDirectories: true)
for (points, scale) in [(16, 1), (16, 2), (32, 1), (32, 2), (128, 1), (128, 2), (256, 1), (256, 2), (512, 1), (512, 2)] {
  let name = scale == 1 ? "icon_\(points)x\(points).png" : "icon_\(points)x\(points)@2x.png"
  try write(bitmap(points * scale, draw: drawAppIcon), to: iconset.appendingPathComponent(name))
}
try write(bitmap(1024, draw: drawAppIcon), to: output.appendingPathComponent("ViGo-1024.png"))
let menuBarLabels = [
  ("ViGoMenuBarTelex", "VITX", false), ("ViGoMenuBarVNI", "VNI", false),
  ("ViGoMenuBarOff", "A", false),
]
let menuBarLabelSize = commonLabelSize(menuBarLabels.map { $0.1 }, width: 36, maxSize: 20)
for (name, label, filled) in menuBarLabels {
  try write(
    bitmap(0, width: 52, height: 36) {
      drawMenuBar($0, label: label, size: menuBarLabelSize, filled: filled)
    },
    to: output.appendingPathComponent("\(name).png"))
}
