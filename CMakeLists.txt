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
  src/assets/enemy_art.cpp
  src/data/level.cpp
  src/data/theme.cpp
  src/data/characters.cpp
  src/engine/camera.cpp
  src/audio/audio.cpp
  src/audio/synth.cpp
  src/audio/synth_named.cpp
  src/game/collision.cpp
  src/game/player.cpp
  src/game/world.cpp
  src/game/world_draw.cpp
  src/game/world_save.cpp
  src/game/world_level.cpp
  src/game/world_proto.cpp
  src/game/world_platforms.cpp
  src/game/world_club.cpp
  src/game/world_actors.cpp
  src/game/world_dark.cpp
  src/game/world_sludge.cpp
  src/data/weapons.cpp
  src/data/enemies.cpp
  src/game/savegame.cpp
  src/frontend/bot.cpp
  src/frontend/planner.cpp
  src/frontend/controls.cpp
  src/frontend/game.cpp
  src/frontend/menus.cpp
  src/frontend/cutscene.cpp
  src/frontend/clips.cpp
  src/frontend/flow.cpp
  src/data/campaign.cpp
  src/game/profile.cpp
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

enable_testing()
add_executable(savegame_test tests/savegame_test.cpp)
target_link_libraries(savegame_test PRIVATE gunrunners_core SDL2::SDL2)
add_test(NAME savegame COMMAND savegame_test ${CMAKE_SOURCE_DIR}/levels/level1.txt)
add_test(NAME savegame_rooftop_run COMMAND savegame_test ${CMAKE_SOURCE_DIR}/levels/01_rooftop_run.txt)
# Far enough up the tower for gondolas to have moved and hatches to be open.
add_test(NAME savegame_glass_canyon COMMAND savegame_test ${CMAKE_SOURCE_DIR}/levels/02_glass_canyon.txt 700)
# Past the coat check's drop launch, with the Bass Cannon in hand.
add_test(NAME savegame_club_laserdisc COMMAND savegame_test ${CMAKE_SOURCE_DIR}/levels/03_club_laserdisc.txt 700)
# Sectors A-C relit and a Grid Leech on the cable.
add_test(NAME savegame_blackout COMMAND savegame_test ${CMAKE_SOURCE_DIR}/levels/04_blackout.txt 720)
# Into the low tunnels: the tide mid-cycle, rats out of their pipe.
add_test(NAME savegame_sludge_line COMMAND savegame_test ${CMAKE_SOURCE_DIR}/levels/05_sludge_line.txt 600)
