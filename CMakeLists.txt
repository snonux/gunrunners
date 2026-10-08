cmake_minimum_required(VERSION 3.16)
project(Gunrunners VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(SDL2 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(CAIRO REQUIRED IMPORTED_TARGET cairo)

add_library(gunrunners_core STATIC
  src/render/color.cpp
  src/render/renderer.cpp
  src/render/vector.cpp
  src/assets/art.cpp
  src/data/level.cpp
  src/data/theme.cpp
  src/data/characters.cpp
  src/engine/camera.cpp
  src/audio/audio.cpp
  src/audio/synth.cpp
  src/game/collision.cpp
  src/game/player.cpp
  src/game/world.cpp
  src/game/world_draw.cpp
  src/frontend/bot.cpp
  src/frontend/game.cpp
)
target_include_directories(gunrunners_core PUBLIC src)
target_link_libraries(gunrunners_core PUBLIC SDL2::SDL2 PkgConfig::CAIRO)
target_compile_definitions(gunrunners_core PUBLIC GR_DATA_DIR="${CMAKE_SOURCE_DIR}")
if(NOT MSVC)
  target_compile_options(gunrunners_core PRIVATE -Wall -Wextra)
endif()

add_executable(gunrunners src/main.cpp)
target_link_libraries(gunrunners PRIVATE gunrunners_core SDL2::SDL2)
if(TARGET SDL2::SDL2main)
  target_link_libraries(gunrunners PRIVATE SDL2::SDL2main)
endif()
