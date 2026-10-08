cmake_minimum_required(VERSION 3.16)
project(TurboDudes VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(SDL2 REQUIRED)

add_library(turbodudes_core STATIC
  src/gfx/canvas.cpp
  src/gfx/font.cpp
  src/assets/art.cpp
  src/data/level.cpp
  src/data/theme.cpp
  src/data/characters.cpp
  src/engine/camera.cpp
  src/game/world.cpp
  src/frontend/bot.cpp
  src/frontend/game.cpp
)
target_include_directories(turbodudes_core PUBLIC src)
target_compile_definitions(turbodudes_core PUBLIC TD_DATA_DIR="${CMAKE_SOURCE_DIR}")
if(NOT MSVC)
  target_compile_options(turbodudes_core PRIVATE -Wall -Wextra)
endif()

add_executable(turbodudes src/main.cpp)
target_link_libraries(turbodudes PRIVATE turbodudes_core SDL2::SDL2)
if(TARGET SDL2::SDL2main)
  target_link_libraries(turbodudes PRIVATE SDL2::SDL2main)
endif()
