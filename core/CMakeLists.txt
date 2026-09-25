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
  src/strokes/outline.cpp
  src/editor/editor.cpp)
target_include_directories(ink PUBLIC include src)
target_compile_definitions(ink PRIVATE INK_VERSION="${PROJECT_VERSION}")
target_link_libraries(ink PUBLIC Skia::skia inkcore pugixml::pugixml FastFloat::fast_float immer
  nlohmann_json::nlohmann_json)

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
  target_sources(ink PRIVATE src/render/metal_frame.mm)
  set_source_files_properties(src/render/metal_frame.mm PROPERTIES COMPILE_OPTIONS -fobjc-arc)
endif()

if(INK_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
