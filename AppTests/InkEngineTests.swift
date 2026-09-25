import InkEngine
import QuartzCore
import XCTest

final class InkEngineTests: XCTestCase {
  func testVersion() {
    XCTAssertEqual(String(cString: ink_version()), "0.1.0")
  }

  func testRendersToAMetalLayerOnlyWhenSomethingChanged() {
    let layer = CAMetalLayer()
    layer.frame = CGRect(x: 0, y: 0, width: 100, height: 100)
    layer.contentsScale = 2
    let canvas = ink_canvas_create(1)
    defer { ink_canvas_destroy(canvas) }
    XCTAssertEqual(ink_canvas_attach_metal(canvas, Unmanaged.passUnretained(layer).toOpaque()), 0)
    // Pixels, not points: bounds × contentsScale.
    ink_canvas_set_surface_size(
      canvas, Int32(layer.bounds.width * layer.contentsScale),
      Int32(layer.bounds.height * layer.contentsScale), Float(layer.contentsScale))
    XCTAssertEqual(ink_render(canvas), 1)
    XCTAssertEqual(ink_render(canvas), 0)
    ink_canvas_set_view(canvas, 2, 0, 0, 2, 0, 0)
    XCTAssertEqual(ink_render(canvas), 1)
  }
}
