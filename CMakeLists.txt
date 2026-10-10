cmake_minimum_required(VERSION 3.16)
project(Gunrunners VERSION 0.1.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if(ANDROID)
  include(android/android.cmake)
else()
  find_package(SDL2 REQUIRED)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(CAIRO REQUIRED IMPORTED_TARGET cairo)
endif()

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
  src/audio/music.cpp
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
  src/game/world_maglev.cpp
  src/game/world_chopper.cpp
  src/game/world_jungle.cpp
  src/game/world_temple.cpp
  src/game/world_light.cpp
  src/game/world_mine.cpp
  src/game/world_lava.cpp
  src/game/world_mine_draw.cpp
  src/game/world_pinball.cpp
  src/game/world_cheats.cpp
  src/game/world_map.cpp
  src/data/weapons.cpp
  src/data/enemies.cpp
  src/game/savegame.cpp
  src/frontend/bot.cpp
  src/frontend/planner.cpp
  src/frontend/bindings.cpp
  src/frontend/controls.cpp
  src/frontend/game.cpp
  src/frontend/menus.cpp
  src/frontend/controls_menu.cpp
  src/frontend/map_view.cpp
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

if(ANDROID)
  # The APK's libmain.so; no desktop binary, tools or tests.
  gr_add_android_main()
  return()
endif()

add_executable(gunrunners src/main.cpp src/frontend/touch_controls.cpp)
target_link_libraries(gunrunners PRIVATE gunrunners_core SDL2::SDL2)
if(TARGET SDL2::SDL2main)
  target_link_libraries(gunrunners PRIVATE SDL2::SDL2main)
endif()

# Box art for the README (tools/covers.sh); not part of the default build.
add_executable(gunrunners_cover EXCLUDE_FROM_ALL tools/cover/cover.cpp)
target_link_libraries(gunrunners_cover PRIVATE gunrunners_core)

enable_testing()
add_executable(touch_test tests/touch_test.cpp src/frontend/touch_controls.cpp)
target_link_libraries(touch_test PRIVATE gunrunners_core SDL2::SDL2)
add_test(NAME touch_controls COMMAND touch_test)
add_executable(gamepad_test tests/gamepad_test.cpp src/frontend/touch_controls.cpp)
target_link_libraries(gamepad_test PRIVATE gunrunners_core SDL2::SDL2)
add_test(NAME gamepad COMMAND gamepad_test)
add_executable(bindings_test tests/bindings_test.cpp src/frontend/touch_controls.cpp)
target_link_libraries(bindings_test PRIVATE gunrunners_core SDL2::SDL2)
add_test(NAME bindings COMMAND bindings_test WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
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
# On the train past the first gantries, Arc Caster in hand.
add_test(NAME savegame_maglev_express COMMAND savegame_test ${CMAKE_SOURCE_DIR}/levels/06_maglev_express.txt 900)
# Across the rooftops under the hunter's light, Lock-On Rockets in hand.
add_test(NAME savegame_chopper_down COMMAND savegame_test ${CMAKE_SOURCE_DIR}/levels/07_chopper_down.txt 500)
# Over the stream on the first vine, the Boomerang in hand.
add_test(NAME savegame_canopy_road COMMAND savegame_test ${CMAKE_SOURCE_DIR}/levels/08_canopy_road.txt 600)
# A stone key in hand, the dud plate pressed, a Guardian down.
add_test(NAME savegame_hall_of_traps COMMAND savegame_test ${CMAKE_SOURCE_DIR}/levels/09_hall_of_traps.txt 400)
add_test(NAME savegame_sun_mirrors COMMAND savegame_test ${CMAKE_SOURCE_DIR}/levels/10_sun_mirrors.txt 400)
add_test(NAME savegame_idol_mines COMMAND savegame_test ${CMAKE_SOURCE_DIR}/levels/11_idol_mines.txt 400)
# Over the stone chains past the first toads, Serpent Spears in hand.
add_test(NAME savegame_lava_heart COMMAND savegame_test ${CMAKE_SOURCE_DIR}/levels/12_lava_heart.txt 400)
