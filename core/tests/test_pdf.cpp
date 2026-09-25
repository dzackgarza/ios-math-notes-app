// Skia PDF backend: Skia docs/examples/PDF.cpp.
#include <catch2/catch_test_macros.hpp>

#include <fstream>

#include "include/codec/SkCodec.h"
#include "include/codec/SkPngDecoder.h"
#include "include/core/SkBitmap.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkData.h"
#include "include/core/SkDocument.h"
#include "include/core/SkImage.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkStream.h"
#include "include/docs/SkPDFDocument.h"
#include "include/docs/SkPDFJpegHelpers.h"
#include "include/encode/SkPngEncoder.h"
#include "include/utils/SkParsePath.h"

namespace {

constexpr SkScalar kA4Width = 595.276f;   // 210 mm in points
constexpr SkScalar kA4Height = 841.89f;   // 297 mm in points

sk_sp<SkData> encodedPng() {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(16, 16);
  bitmap.eraseColor(SK_ColorBLUE);
  SkDynamicMemoryWStream png;
  REQUIRE(SkPngEncoder::Encode(&png, bitmap.pixmap(), {}));
  return png.detachAsData();
}

sk_sp<SkData> a4Pdf() {
  auto path = SkParsePath::FromSVGString("M100 100 L300 120 L200 300 Z");
  REQUIRE(path.has_value());
  auto codec = SkPngDecoder::Decode(encodedPng(), nullptr);
  REQUIRE(codec);
  auto [image, result] = codec->getImage();
  REQUIRE(result == SkCodec::kSuccess);

  SkDynamicMemoryWStream out;
  auto document = SkPDF::MakeDocument(&out, SkPDF::JPEG::MetadataWithCallbacks());
  REQUIRE(document);
  SkCanvas *canvas = document->beginPage(kA4Width, kA4Height);
  SkPaint paint;
  paint.setColor(SK_ColorBLACK);
  canvas->drawPath(*path, paint);
  canvas->drawImage(image, 400, 400);
  document->endPage();
  document->close();
  return out.detachAsData();
}

}  // namespace

TEST_CASE("Skia PDF writes a deterministic A4 page with a path and a PNG") {
  auto first = a4Pdf();
  auto second = a4Pdf();
  REQUIRE(first->size() > 0);
  REQUIRE(first->equals(second.get()));
  std::ofstream("a4.pdf", std::ios::binary)
      .write(static_cast<const char *>(first->data()), first->size());
}
