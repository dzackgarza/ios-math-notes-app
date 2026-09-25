cmake_minimum_required(VERSION 3.25)
project(ink_engine VERSION 0.1.0 LANGUAGES C CXX)
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
  enable_language(OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(INK_BUILD_TESTS "Build the Catch2 tests" OFF)

if(EMSCRIPTEN)
  # Must match the Skia prebuilt and the vcpkg triplet.
  add_compile_options(-fwasm-exceptions -sSUPPORT_LONGJMP=wasm)
  add_link_options(-fwasm-exceptions -sSUPPORT_LONGJMP=wasm)
endif()

include(cmake/Skia.cmake)
find_package(pugixml CONFIG REQUIRED)
find_package(FastFloat CONFIG REQUIRED)
find_package(Immer CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
include(third_party/ink.cmake)

add_library(ink STATIC
  src/ink.cpp
  src/geometry/hit_shapes.cpp
  src/format/numbers.cpp
  src/format/page_svg.cpp
  src/format/notebook.cpp
  src/document/pages.cpp
  src/document/templates.cpp
  src/strokes/outline.cpp
  src/editor/editor.cpp
  src/editor/history.cpp
  src/layout/layout.cpp
  src/render/renderer.cpp)
target_include_directories(ink PUBLIC include src)
target_compile_definitions(ink PRIVATE INK_VERSION="${PROJECT_VERSION}")
target_link_libraries(ink PUBLIC Skia::skia inkcore pugixml::pugixml FastFloat::fast_float immer
  nlohmann_json::nlohmann_json)

if(EMSCRIPTEN)
  target_sources(ink PRIVATE src/render/webgl_surface.cpp)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
  target_sources(ink PRIVATE src/render/metal_surface.mm)
  set_source_files_properties(src/render/metal_surface.mm PROPERTIES COMPILE_OPTIONS -fobjc-arc)
endif()

if(INK_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(EMSCRIPTEN)
  # The web engine module (issue #4). Link flags follow the emcc step of
  # MuPDF platform/wasm/tools/build.sh (ArtifexSoftware/mupdf d587982): no
  # main, no file system, an ES6 module factory. --emit-tsd runs tsc, which
  # hosts/web installs: build with hosts/web/node_modules/.bin on PATH
  # (just engine-module).
  set(INK_EXPORTS
    _ink_version _ink_last_error
    _ink_document_create _ink_document_load_notebook _ink_document_load_page
    _ink_document_load_asset _ink_document_dirty_files _ink_document_mark_saved
    _ink_document_free _ink_document_content_size
    _ink_document_page_count _ink_document_insert_page _ink_document_delete_page
    _ink_document_move_page _ink_document_set_page_size _ink_document_set_template
    _ink_builtin_template_count _ink_builtin_template_name _ink_builtin_template_create
    _ink_canvas_create_webgl _ink_canvas_set_view _ink_canvas_set_surface_size
    _ink_canvas_set_tool _ink_canvas_set_utc_offset _ink_canvas_free _ink_canvas_page_at
    _ink_input _ink_input_update _ink_render _ink_undo _ink_redo _ink_document_page_rect _ink_struct_layout
    _malloc _free)
  list(JOIN INK_EXPORTS "," INK_EXPORTS_FLAG)
  set(INK_MODULE_FLAGS
    -sEXPORTED_FUNCTIONS=${INK_EXPORTS_FLAG}
    -sEXPORTED_RUNTIME_METHODS=HEAPU8,HEAPU32,HEAPF32,HEAPF64
    --no-entry -sFILESYSTEM=0 -sMODULARIZE=1 -sEXPORT_ES6=1 -sALLOW_MEMORY_GROWTH=1
    -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2)

  add_executable(engine EXCLUDE_FROM_ALL src/wasm/module.cpp)
  target_link_libraries(engine PRIVATE ink)
  set_target_properties(engine PROPERTIES SUFFIX .mjs RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/web)
  target_link_options(engine PRIVATE ${INK_MODULE_FLAGS} -sENVIRONMENT=web,worker
    --emit-tsd engine.d.mts)

  # The same module for Node, with the sample round-trip exports of
  # tests/wasm_abi, for the wrapper's tests.
  add_executable(engine_test EXCLUDE_FROM_ALL src/wasm/module.cpp tests/wasm_abi/samples.cpp)
  target_link_libraries(engine_test PRIVATE ink)
  set_target_properties(engine_test PROPERTIES SUFFIX .mjs RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/web)
  target_link_options(engine_test PRIVATE ${INK_MODULE_FLAGS} -sENVIRONMENT=node
    --emit-tsd engine_test.d.mts)
endif()
