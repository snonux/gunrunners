cmake_minimum_required(VERSION 3.16)
project(Gunrunners VERSION 0.2.0 LANGUAGES CXX)

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
  src/assets/art_alien.cpp
  src/assets/art_starfall.cpp
  src/assets/art_crystal.cpp
  src/assets/art_silk.cpp
  src/assets/art_plains.cpp
  src/assets/enemy_art_alien.cpp
  src/assets/enemy_art_station.cpp
  src/assets/enemy_art_cryo.cpp
  src/assets/enemy_art_green.cpp
  src/assets/enemy_art_grav.cpp
  src/assets/enemy_art_reactor.cpp
  src/assets/enemy_art_hull.cpp
  src/assets/enemy_art_hive.cpp
  src/assets/enemy_art_starfall.cpp
  src/assets/enemy_art_crystal.cpp
  src/assets/enemy_art_silk.cpp
  src/assets/enemy_art_plains.cpp
  src/assets/enemy_art_mother.cpp
  src/assets/vehicle_art.cpp
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
  src/game/world_space.cpp
  src/game/world_hive.cpp
  src/game/world_starfall.cpp
  src/game/world_crystal.cpp
  src/game/world_silk.cpp
  src/game/world_plains.cpp
  src/game/world_mother.cpp
  src/game/world_boulder.cpp
  src/game/world_surf.cpp
  src/game/world_sanctum.cpp
  src/game/world_golden.cpp
  src/game/world_station.cpp
  src/game/world_station_draw.cpp
  src/game/world_drift.cpp
  src/game/world_cryo.cpp
  src/game/world_cryo_draw.cpp
  src/game/world_green_draw.cpp
  src/game/world_green.cpp
  src/game/world_grav_draw.cpp
  src/game/world_grav.cpp
  src/game/world_reactor.cpp
  src/game/world_reactor_draw.cpp
  src/game/world_hull_draw.cpp
  src/game/world_hull.cpp
  src/game/world_orbit.cpp
  src/game/world_mine_draw.cpp
  src/game/world_pinball.cpp
  src/game/world_cheats.cpp
  src/game/world_map.cpp
  src/game/world_vehicle.cpp
  src/game/world_vehicle_draw.cpp
  src/game/world_sea.cpp
  src/data/weapons.cpp
  src/data/enemies.cpp
  src/game/savegame.cpp
  src/frontend/bot.cpp
  src/frontend/bot_drive.cpp
  src/frontend/bot_mother.cpp
  src/frontend/planner.cpp
  src/frontend/bindings.cpp
  src/frontend/controls.cpp
  src/frontend/game.cpp
  src/frontend/menus.cpp
  src/frontend/select.cpp
  src/frontend/runner_editor.cpp
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
add_executable(runner_editor_test tests/runner_editor_test.cpp)
target_link_libraries(runner_editor_test PRIVATE gunrunners_core SDL2::SDL2)
add_test(NAME runner_editor COMMAND runner_editor_test)
add_executable(vehicle_test tests/vehicle_test.cpp)
target_link_libraries(vehicle_test PRIVATE gunrunners_core SDL2::SDL2)
add_test(NAME vehicles COMMAND vehicle_test ${CMAKE_SOURCE_DIR}/levels)
# The bot drives the extra levels' vehicles to the exit.
foreach(extra motor_pool deep_dive)
  add_test(NAME bot_extra_${extra} COMMAND gunrunners --headless --autoplay --skip-menu --no-cutscenes
    --quit-after-clear --frames 20000 --level ${CMAKE_SOURCE_DIR}/levels/extra_${extra}.txt)
  set_tests_properties(bot_extra_${extra} PROPERTIES PASS_REGULAR_EXPRESSION "cleared:")
endforeach()
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
add_test(NAME savegame_crash_garden COMMAND savegame_test ${CMAKE_SOURCE_DIR}/levels/44_crash_garden.txt 400)
add_test(NAME savegame_hive_gullets COMMAND savegame_test ${CMAKE_SOURCE_DIR}/levels/45_hive_gullets.txt 400)
add_executable(hive_test tests/hive_test.cpp)
target_link_libraries(hive_test PRIVATE gunrunners_core SDL2::SDL2)
add_test(NAME hive_tubes COMMAND hive_test)
# Level 43: in the ship, through the asteroids.
add_test(NAME savegame_starfall COMMAND savegame_test ${CMAKE_SOURCE_DIR}/levels/43_starfall.txt 500)
add_executable(starfall_test tests/starfall_test.cpp)
target_link_libraries(starfall_test PRIVATE gunrunners_core SDL2::SDL2)
add_test(NAME starfall_space COMMAND starfall_test)
add_executable(crystal_test tests/crystal_test.cpp)
target_link_libraries(crystal_test PRIVATE gunrunners_core SDL2::SDL2)
add_test(NAME crystal_swaps COMMAND crystal_test)
add_executable(silk_test tests/silk_test.cpp)
target_link_libraries(silk_test PRIVATE gunrunners_core SDL2::SDL2)
add_test(NAME silk_lines COMMAND silk_test)
add_executable(plains_test tests/plains_test.cpp)
target_link_libraries(plains_test PRIVATE gunrunners_core SDL2::SDL2)
add_test(NAME plains_bounders COMMAND plains_test)
add_executable(mother_test tests/mother_test.cpp)
target_link_libraries(mother_test PRIVATE gunrunners_core SDL2::SDL2)
add_test(NAME mother_boss COMMAND mother_test)
add_test(NAME savegame_crystal_drift COMMAND savegame_test ${CMAKE_SOURCE_DIR}/levels/46_crystal_drift.txt 500)
add_test(NAME savegame_silk_canyon COMMAND savegame_test ${CMAKE_SOURCE_DIR}/levels/47_silk_canyon.txt 500)
# Level 48: on the first Bounder, over the thorns.
add_test(NAME savegame_bounder_plains COMMAND savegame_test ${CMAKE_SOURCE_DIR}/levels/48_bounder_plains.txt 400)
add_test(NAME savegame_hive_mother COMMAND savegame_test ${CMAKE_SOURCE_DIR}/levels/49_hive_mother.txt 400)
add_test(NAME savegame_boulder_run COMMAND savegame_test ${CMAKE_SOURCE_DIR}/levels/13_boulder_run.txt 420)
add_test(NAME savegame_idol_awakens COMMAND savegame_test ${CMAKE_SOURCE_DIR}/levels/14_idol_awakens.txt 300)
add_test(NAME savegame_hangar_bay COMMAND savegame_test ${CMAKE_SOURCE_DIR}/levels/15_hangar_bay.txt 800)
add_test(NAME savegame_cryo_labs COMMAND savegame_test ${CMAKE_SOURCE_DIR}/levels/16_cryo_labs.txt 1000)
add_executable(station_test tests/station_test.cpp)
target_link_libraries(station_test PRIVATE gunrunners_core SDL2::SDL2)
add_test(NAME station_breach COMMAND station_test ${CMAKE_SOURCE_DIR}/levels/15_hangar_bay.txt
  ${CMAKE_SOURCE_DIR}/levels/15_bonus_asteroid_belt.txt)
add_executable(cryo_test tests/cryo_test.cpp)
target_link_libraries(cryo_test PRIVATE gunrunners_core SDL2::SDL2)
add_test(NAME cryo_labs COMMAND cryo_test ${CMAKE_SOURCE_DIR}/levels/16_bonus_air_hockey.txt)
add_executable(green_test tests/green_test.cpp)
target_link_libraries(green_test PRIVATE gunrunners_core SDL2::SDL2)
add_test(NAME hydroponics COMMAND green_test ${CMAKE_SOURCE_DIR}/levels/17_bonus_growth_spurt.txt)
add_test(NAME savegame_hydroponics COMMAND savegame_test ${CMAKE_SOURCE_DIR}/levels/17_hydroponics.txt 1200)
add_executable(hull_test tests/hull_test.cpp)
target_link_libraries(hull_test PRIVATE gunrunners_core SDL2::SDL2)
add_test(NAME hull_walk COMMAND hull_test ${CMAKE_SOURCE_DIR}/levels/18_bonus_planetoids.txt)
add_test(NAME savegame_hull_walk COMMAND savegame_test ${CMAKE_SOURCE_DIR}/levels/18_hull_walk.txt 1200)
add_test(NAME savegame_gravity_lab COMMAND savegame_test ${CMAKE_SOURCE_DIR}/levels/19_gravity_lab.txt 330)
add_executable(grav_test tests/grav_test.cpp)
target_link_libraries(grav_test PRIVATE gunrunners_core SDL2::SDL2)
add_test(NAME gravity_lab COMMAND grav_test ${CMAKE_SOURCE_DIR}/levels/19_bonus_gun_gravity.txt)
add_test(NAME savegame_reactor_core COMMAND savegame_test ${CMAKE_SOURCE_DIR}/levels/20_reactor_core.txt 330)
add_executable(reactor_test tests/reactor_test.cpp)
target_link_libraries(reactor_test PRIVATE gunrunners_core SDL2::SDL2)
add_test(NAME reactor_core COMMAND reactor_test ${CMAKE_SOURCE_DIR}/levels/20_bonus_stop_motion.txt)
