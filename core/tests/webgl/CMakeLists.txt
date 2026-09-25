add_executable(webgl_check webgl_check.cpp)
target_link_libraries(webgl_check PRIVATE ink)
target_link_options(webgl_check PRIVATE
  -sENVIRONMENT=web -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2
  -sALLOW_MEMORY_GROWTH=1 -sEXPORTED_RUNTIME_METHODS=ccall,HEAPU8)
configure_file(index.html index.html COPYONLY)
