import InkEngine
import QuartzCore
import XCTest

final class InkEngineTests: XCTestCase {
  func testVersion() {
    XCTAssertEqual(String(cString: ink_version()), "0.1.0")
  }

  func testPresentsOneMetalFrame() {
    let layer = CAMetalLayer()
    layer.frame = CGRect(x: 0, y: 0, width: 100, height: 100)
    layer.contentsScale = 2
    // Pixels, not points: bounds × contentsScale.
    layer.drawableSize = CGSize(
      width: layer.bounds.width * layer.contentsScale,
      height: layer.bounds.height * layer.contentsScale)
    XCTAssertEqual(ink_metal_draw_test_frame(Unmanaged.passUnretained(layer).toOpaque()), 0)
  }
}
