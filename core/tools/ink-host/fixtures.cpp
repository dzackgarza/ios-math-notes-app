// Writes core/tests/fixtures/ink: three input traces and the outline of each
// stock test brush on each trace, computed on the Linux host.
//
// straight_line and spring_shape are google/ink's recorded traces
// (ink/strokes/input/testdata/*.binarypb), decoded and fitted into the same
// bounds as google/ink's LoadIncrementalStrokeInputs
// (ink/strokes/input/recorded_test_inputs.cc). lissajous is google/ink's
// synthetic MakeCompleteLissajousCurveInputs, split into 16-input frames.
//
// Usage: ink_host_fixtures <google-ink-source-dir> <fixture-dir>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "absl/log/absl_check.h"
#include "ink/geometry/affine_transform.h"
#include "ink/geometry/envelope.h"
#include "ink/geometry/rect.h"
#include "ink/storage/proto/incremental_stroke_inputs.pb.h"
#include "ink/storage/stroke_input_batch.h"
#include "ink/strokes/input/synthetic_test_inputs.h"
#include "ink/strokes/stroke.h"
#include "ink/types/duration.h"
#include "trace.h"

namespace {

const ink::Rect kBounds = ink::Rect::FromTwoPoints({0, 0}, {500, 500});

std::vector<ink_test::TraceFrame> DecodeRecorded(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  ABSL_CHECK(file.is_open()) << path;
  std::string bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  ink::proto::IncrementalStrokeInputs proto;
  ABSL_CHECK(proto.ParseFromString(bytes)) << path;

  std::vector<ink_test::TraceFrame> frames;
  ink::Envelope envelope;
  for (const auto &input : proto.inputs()) {
    auto real = ink::DecodeStrokeInputBatch(input.real());
    auto predicted = ink::DecodeStrokeInputBatch(input.predicted());
    ABSL_CHECK_OK(real.status());
    ABSL_CHECK_OK(predicted.status());
    for (ink::StrokeInput in : *real) envelope.Add(in.position);
    for (ink::StrokeInput in : *predicted) envelope.Add(in.position);
    frames.push_back({*real, *predicted});
  }
  auto transform = ink::AffineTransform::Find(*envelope.AsRect(), kBounds);
  ABSL_CHECK(transform.has_value());
  for (auto &frame : frames) {
    frame.real.Transform(*transform);
    frame.predicted.Transform(*transform);
  }
  return frames;
}

std::vector<ink_test::TraceFrame> Lissajous() {
  ink::StrokeInputBatch all =
      ink::MakeCompleteLissajousCurveInputs(ink::Duration32::Seconds(1.5), kBounds);
  std::vector<ink_test::TraceFrame> frames;
  for (size_t i = 0; i < all.Size(); ++i) {
    if (i % 16 == 0) frames.emplace_back();
    ABSL_CHECK_OK(frames.back().real.Append(all.Get(i)));
  }
  return frames;
}

}  // namespace

int main(int argc, char **argv) {
  ABSL_CHECK_EQ(argc, 3) << "usage: ink_host_fixtures <google-ink-source-dir> <fixture-dir>";
  const std::string testdata = std::string(argv[1]) + "/ink/strokes/input/testdata/";
  const std::string out = argv[2];

  ink_test::WriteTrace(out + "/straight_line.trace", DecodeRecorded(testdata + "straight_line.binarypb"));
  ink_test::WriteTrace(out + "/spring_shape.trace", DecodeRecorded(testdata + "spring_shape.binarypb"));
  ink_test::WriteTrace(out + "/lissajous.trace", Lissajous());

  // Goldens come from the traces as written, so both sides read the same floats.
  for (const std::string &trace : ink_test::kTraceNames) {
    ink::StrokeInputBatch inputs = ink_test::RealInputs(ink_test::ReadTrace(out + "/" + trace + ".trace"));
    for (const auto &[name, brush] : ink_test::StockTestBrushes()) {
      ink_test::WriteOutline(out + "/" + trace + "." + name + ".outline",
                             ink_test::Outline(ink::Stroke(brush, inputs)));
    }
  }
  return 0;
}
