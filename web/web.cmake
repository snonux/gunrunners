# Browser build of the game (included by the top-level CMakeLists.txt when
# emcmake runs it). SDL2 is Emscripten's port; Cairo, pixman and FreeType
# are the static libraries web/build-deps.sh compiles to WebAssembly.
# Cloud saves need libcurl, which a browser page cannot use: they are left
# out (GR_NO_CURL).

set(GR_WEB_DIR ${CMAKE_CURRENT_LIST_DIR})
set(GR_WEB_DEPS ${GR_WEB_DIR}/deps/prefix)
if(NOT EXISTS ${GR_WEB_DEPS}/lib/libcairo.a)
  message(FATAL_ERROR "No Cairo for the web: run web/build-deps.sh first")
endif()

add_library(SDL2::SDL2 INTERFACE IMPORTED)
set_target_properties(SDL2::SDL2 PROPERTIES
  INTERFACE_COMPILE_OPTIONS "-sUSE_SDL=2"
  INTERFACE_LINK_OPTIONS "-sUSE_SDL=2")

add_library(PkgConfig::CAIRO INTERFACE IMPORTED)
set_target_properties(PkgConfig::CAIRO PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${GR_WEB_DEPS}/include/cairo;${GR_WEB_DEPS}/include/freetype2"
  INTERFACE_COMPILE_DEFINITIONS GR_BUNDLED_FONT
  INTERFACE_LINK_LIBRARIES
    "${GR_WEB_DEPS}/lib/libcairo.a;${GR_WEB_DEPS}/lib/libpixman-1.a;${GR_WEB_DEPS}/lib/libfreetype.a")

add_library(CURL::libcurl INTERFACE IMPORTED)
set_target_properties(CURL::libcurl PROPERTIES INTERFACE_COMPILE_DEFINITIONS GR_NO_CURL)

# Some loaders parse numbers with try/catch, so exceptions stay on.
add_compile_options(-fexceptions)
add_link_options(-fexceptions)

# gunrunners.html/.js/.wasm plus gunrunners.data: levels, cutscenes, fonts
# and voices, mounted at /data in the page's file system. Saves live in
# IndexedDB (/saves, see src/main.cpp).
function(gr_add_web_main)
  add_executable(gunrunners src/main.cpp src/frontend/touch_controls.cpp)
  target_link_libraries(gunrunners PRIVATE gunrunners_core SDL2::SDL2)
  set_target_properties(gunrunners PROPERTIES SUFFIX ".html")
  target_link_options(gunrunners PRIVATE
    -lidbfs.js
    -sALLOW_MEMORY_GROWTH=1
    -sINITIAL_MEMORY=256MB
    -sMAXIMUM_MEMORY=2GB
    -sSTACK_SIZE=1MB
    -sEXPORTED_RUNTIME_METHODS=FS
    "SHELL:--shell-file ${GR_WEB_DIR}/shell.html"
    "SHELL:--pre-js ${GR_WEB_DIR}/pre.js"
    "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/levels@/data/levels"
    "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/cutscenes@/data/cutscenes"
    "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/fonts@/data/fonts"
    "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/voices@/data/voices")
endfunction()
