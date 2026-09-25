# google/ink core modules (brush, color, geometry, strokes, types) at a pinned
# commit, built with our own CMake. The source list is the non-test .cc files
# of those modules. It follows Chromium third_party/ink/BUILD.gn:13-205, which
# also leaves out storage (protobuf), rendering, jni and kmp.
include(FetchContent)
FetchContent_Declare(google_ink
  URL https://github.com/google/ink/archive/1b220eee5a05e9b67be9f20f49ae2d574c8667a7.tar.gz
  URL_HASH SHA256=d728f4773153fd1d5d9659b8445653773d22deabc709b571a967106220c1f627
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(google_ink)

find_package(absl CONFIG REQUIRED)
find_package(unofficial-libtess2 CONFIG REQUIRED)

set(INK_CORE_SOURCES
  ink/brush/brush_behavior.cc
  ink/brush/brush.cc
  ink/brush/brush_coat.cc
  ink/brush/brush_family.cc
  ink/brush/brush_paint.cc
  ink/brush/brush_tip.cc
  ink/brush/color_function.cc
  ink/brush/easing_function.cc
  ink/brush/stock_brushes.cc
  ink/color/color.cc
  ink/color/color_space.cc
  ink/geometry/affine_transform.cc
  ink/geometry/angle.cc
  ink/geometry/distance.cc
  ink/geometry/envelope.cc
  ink/geometry/internal/algorithms.cc
  ink/geometry/internal/circle.cc
  ink/geometry/internal/intersects_internal.cc
  ink/geometry/internal/legacy_segment_intersection.cc
  ink/geometry/internal/legacy_triangle_contains.cc
  ink/geometry/internal/legacy_vector_utils.cc
  ink/geometry/internal/lerp.cc
  ink/geometry/internal/mesh_packing.cc
  ink/geometry/internal/modulo.cc
  ink/geometry/internal/outline_processing.cc
  ink/geometry/internal/polyline_processing.cc
  ink/geometry/internal/static_rtree.cc
  ink/geometry/intersects.cc
  ink/geometry/mesh.cc
  ink/geometry/mesh_format.cc
  ink/geometry/mesh_packing_types.cc
  ink/geometry/mutable_mesh.cc
  ink/geometry/partitioned_mesh.cc
  ink/geometry/point.cc
  ink/geometry/quad.cc
  ink/geometry/rect.cc
  ink/geometry/segment.cc
  ink/geometry/tessellator.cc
  ink/geometry/triangle.cc
  ink/geometry/vec.cc
  ink/strokes/in_progress_stroke.cc
  ink/strokes/input/internal/stroke_input_validation_helpers.cc
  ink/strokes/input/stroke_input_batch.cc
  ink/strokes/input/stroke_input.cc
  ink/strokes/internal/brush_tip_extruder.cc
  ink/strokes/internal/brush_tip_extruder/derivative_calculator.cc
  ink/strokes/internal/brush_tip_extruder/derivative_calculator_helpers.cc
  ink/strokes/internal/brush_tip_extruder/directed_partial_outline.cc
  ink/strokes/internal/brush_tip_extruder/extruded_vertex.cc
  ink/strokes/internal/brush_tip_extruder/find_clockwise_winding_segment.cc
  ink/strokes/internal/brush_tip_extruder/geometry.cc
  ink/strokes/internal/brush_tip_extruder/mutable_mesh_view.cc
  ink/strokes/internal/brush_tip_extruder/simplify.cc
  ink/strokes/internal/brush_tip_extrusion.cc
  ink/strokes/internal/brush_tip_modeler.cc
  ink/strokes/internal/brush_tip_modeler_helpers.cc
  ink/strokes/internal/brush_tip_shape.cc
  ink/strokes/internal/brush_tip_state.cc
  ink/strokes/internal/circular_extrusion_helpers.cc
  ink/strokes/internal/constrain_brush_tip_extrusion.cc
  ink/strokes/internal/easing_implementation.cc
  ink/strokes/internal/modeled_stroke_input.cc
  ink/strokes/internal/mutable_multi_mesh.cc
  ink/strokes/internal/noise_generator.cc
  ink/strokes/internal/rounded_polygon.cc
  ink/strokes/internal/stroke_input_modeler.cc
  ink/strokes/internal/stroke_input_modeler/passthrough_input_modeler.cc
  ink/strokes/internal/stroke_input_modeler/sliding_window_input_modeler.cc
  ink/strokes/internal/stroke_outline.cc
  ink/strokes/internal/stroke_segmentation.cc
  ink/strokes/internal/stroke_shape_builder.cc
  ink/strokes/internal/stroke_shape_update.cc
  ink/strokes/internal/stroke_subtraction.cc
  ink/strokes/internal/stroke_vertex.cc
  ink/strokes/stroke.cc
  ink/types/duration.cc
  ink/types/physical_distance.cc
)
list(TRANSFORM INK_CORE_SOURCES PREPEND "${google_ink_SOURCE_DIR}/")

add_library(inkcore STATIC ${INK_CORE_SOURCES})
target_include_directories(inkcore PUBLIC "${google_ink_SOURCE_DIR}")
target_link_libraries(inkcore PUBLIC
  unofficial::libtess2::libtess2
  absl::algorithm_container absl::check absl::cleanup absl::flat_hash_map
  absl::flat_hash_set absl::function_ref absl::hash absl::inlined_vector
  absl::log absl::status absl::status_builder absl::status_macros absl::statusor
  absl::str_format absl::strings
  absl::span absl::time absl::synchronization absl::any_invocable absl::btree
  absl::core_headers absl::base absl::bits absl::fixed_array)
