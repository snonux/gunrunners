# Android build of the native game (included by the top-level CMakeLists.txt
# when Gradle builds it with the NDK). SDL2 is built from source; Cairo and
# pixman are the static libraries android/build-deps.sh cross-compiles.

set(GR_ANDROID_DEPS ${CMAKE_CURRENT_LIST_DIR}/deps)
if(NOT EXISTS ${GR_ANDROID_DEPS}/${ANDROID_ABI}/lib/libcairo.a)
  message(FATAL_ERROR "No Cairo for ${ANDROID_ABI}: run android/build-deps.sh first")
endif()

set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
add_subdirectory(${GR_ANDROID_DEPS}/SDL2 ${CMAKE_BINARY_DIR}/SDL2)

# Same target name as pkg-config's on the desktop, so the rest of the build
# does not care where Cairo came from.
add_library(PkgConfig::CAIRO INTERFACE IMPORTED)
set_target_properties(PkgConfig::CAIRO PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${GR_ANDROID_DEPS}/${ANDROID_ABI}/include/cairo"
  INTERFACE_LINK_LIBRARIES
    "${GR_ANDROID_DEPS}/${ANDROID_ABI}/lib/libcairo.a;${GR_ANDROID_DEPS}/${ANDROID_ABI}/lib/libpixman-1.a;m")

# SDL's Java activity loads libmain.so and calls SDL_main (main.cpp's main,
# renamed by SDL_main.h).
function(gr_add_android_main)
  add_library(main SHARED
    src/main.cpp
    src/frontend/android_data.cpp
    src/frontend/touch_controls.cpp)
  target_link_libraries(main PRIVATE gunrunners_core SDL2::SDL2 SDL2::SDL2main android log)
endfunction()
