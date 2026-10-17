# Remove only files recorded by the last CMake install. Player saves live
# alongside the game data and are deliberately absent from this manifest.
set(manifest "${BUILD_DIR}/install_manifest.txt")
if(NOT EXISTS "${manifest}")
  message(FATAL_ERROR "No install manifest at ${manifest}; install from this build directory first.")
endif()

file(STRINGS "${manifest}" installed_files)
foreach(installed_file IN LISTS installed_files)
  set(path "$ENV{DESTDIR}${installed_file}")
  if(IS_DIRECTORY "${path}" AND NOT IS_SYMLINK "${path}")
    message(FATAL_ERROR "Refusing to remove a directory from the install manifest: ${path}")
  endif()
  message(STATUS "Removing ${path}")
  file(REMOVE "${path}")
  if(EXISTS "${path}" OR IS_SYMLINK "${path}")
    message(FATAL_ERROR "Could not remove ${path}")
  endif()
endforeach()
