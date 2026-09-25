# Linux host tool that writes the google/ink test fixtures and goldens under
# core/tests/fixtures/ink. It decodes google/ink's recorded traces with
# google/ink's own storage code, so it also builds protobuf and ink/storage.
cmake_minimum_required(VERSION 3.25)
project(ink_host_fixtures LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(../../third_party/ink.cmake)
find_package(protobuf CONFIG REQUIRED)

set(INK_PROTOS
  "${google_ink_SOURCE_DIR}/ink/storage/proto/coded_numeric_run.proto"
  "${google_ink_SOURCE_DIR}/ink/storage/proto/stroke_input_batch.proto"
  "${google_ink_SOURCE_DIR}/ink/storage/proto/incremental_stroke_inputs.proto")
add_library(ink_protos STATIC)
target_link_libraries(ink_protos PUBLIC protobuf::libprotobuf)
target_include_directories(ink_protos PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")
protobuf_generate(TARGET ink_protos PROTOS ${INK_PROTOS}
  IMPORT_DIRS "${google_ink_SOURCE_DIR}"
  PROTOC_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}")

add_executable(ink_host_fixtures
  fixtures.cpp
  ../../tests/support/trace.cpp
  "${google_ink_SOURCE_DIR}/ink/storage/input_batch.cc"
  "${google_ink_SOURCE_DIR}/ink/storage/numeric_run.cc"
  "${google_ink_SOURCE_DIR}/ink/storage/stroke_input_batch.cc"
  "${google_ink_SOURCE_DIR}/ink/strokes/input/synthetic_test_inputs.cc")
target_include_directories(ink_host_fixtures PRIVATE ../../tests/support)
target_link_libraries(ink_host_fixtures PRIVATE inkcore ink_protos)
