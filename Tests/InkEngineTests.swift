import InkEngine
import Metal
import QuartzCore
import XCTest

final class InkEngineTests: XCTestCase {
  func testVersion() {
    XCTAssertEqual(String(cString: ink_version()), "0.1.0")
  }

  func testRendersToAMetalLayerOnlyWhenSomethingChanged() throws {
    let device = try XCTUnwrap(MTLCreateSystemDefaultDevice())
    let queue = try XCTUnwrap(device.makeCommandQueue())
    let layer = CAMetalLayer()
    layer.frame = CGRect(x: 0, y: 0, width: 100, height: 100)
    layer.contentsScale = 2

    var document: OpaquePointer?
    XCTAssertEqual(ink_document_create(1, &document), INK_OK)
    defer { ink_document_free(document) }
    var canvas: OpaquePointer?
    XCTAssertEqual(
      ink_canvas_create_metal(
        document, Unmanaged.passUnretained(device).toOpaque(),
        Unmanaged.passUnretained(queue).toOpaque(), Unmanaged.passUnretained(layer).toOpaque(),
        &canvas), INK_OK)
    defer { ink_canvas_free(canvas) }
    // Pixels, not points: bounds × contentsScale.
    XCTAssertEqual(
      ink_canvas_set_surface_size(
        canvas, Int32(layer.bounds.width * layer.contentsScale),
        Int32(layer.bounds.height * layer.contentsScale), Float(layer.contentsScale)), INK_OK)

    var drew: Int32 = 0
    XCTAssertEqual(ink_render(canvas, &drew), INK_OK)
    XCTAssertEqual(drew, 1)
    XCTAssertEqual(ink_render(canvas, &drew), INK_OK)
    XCTAssertEqual(drew, 0)
    XCTAssertEqual(ink_canvas_set_view(canvas, 2, 0, 0, 2, 0, 0), INK_OK)
    XCTAssertEqual(ink_render(canvas, &drew), INK_OK)
    XCTAssertEqual(drew, 1)
  }

  func testBadPageBytesGiveAParseError() {
    var document: OpaquePointer?
    XCTAssertEqual(ink_document_create(1, &document), INK_OK)
    defer { ink_document_free(document) }
    let bad = Array("<svg><<<".utf8)
    XCTAssertEqual(ink_document_load_page(document, "pages/0001.svg", bad, bad.count), INK_ERROR_PARSE)
    XCTAssertTrue(String(cString: ink_last_error()).hasPrefix("pages/0001.svg: "))
  }
}
