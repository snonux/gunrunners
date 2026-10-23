// Upgrade an already installed Android game. SDL2 opens a relative path in
// internal storage first, falling back to the APK only if it is absent.
// The APK and extracted files must therefore use different source paths.
#include "frontend/android_data.hpp"

#include <SDL.h>

#include <cstdarg>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace fs = std::filesystem;

namespace
{
std::string internal;
fs::path apk;
int failures = 0;

void write(const fs::path& path, const std::string& data)
{
  fs::create_directories(path.parent_path());
  std::ofstream(path, std::ios::binary) << data;
}

std::string read(const fs::path& path)
{
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

void check(bool ok, const char* what)
{
  std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
  if (!ok)
    ++failures;
}
} // namespace

const char* SDL_AndroidGetInternalStoragePath() { return internal.c_str(); }

SDL_RWops* SDL_RWFromFile(const char* path, const char* mode)
{
  // Same lookup order as native SDL2 src/file/SDL_rwops.c on Android.
  std::FILE* file = std::fopen((fs::path(internal) / path).c_str(), mode);
  if (!file)
    file = std::fopen((apk / path).c_str(), mode);
  return file ? new SDL_RWops{file} : nullptr;
}

Sint64 SDL_RWsize(SDL_RWops* rw)
{
  const long pos = std::ftell(rw->file);
  std::fseek(rw->file, 0, SEEK_END);
  const long size = std::ftell(rw->file);
  std::fseek(rw->file, pos, SEEK_SET);
  return size;
}

std::size_t SDL_RWread(SDL_RWops* rw, void* data, std::size_t size, std::size_t count)
{
  return std::fread(data, size, count, rw->file);
}

int SDL_RWclose(SDL_RWops* rw)
{
  const int result = std::fclose(rw->file);
  delete rw;
  return result;
}

const char* SDL_GetError() { return "file unavailable"; }
void SDL_Log(const char* format, ...)
{
  va_list args;
  va_start(args, format);
  std::vprintf(format, args);
  std::puts("");
  va_end(args);
}

int main(int argc, char** argv)
{
  if (argc != 3)
    return 2;
  const fs::path root = argv[1];
  fs::remove_all(root);
  internal = (root / "internal").string();
  apk = root / "apk";
  const std::string level = read(fs::path(argv[2]) / "01_rooftop_run.txt");
  check(!level.empty(), "test reads the shipped Rooftop Run level");
  const std::string manifest = "levels/01_rooftop_run.txt\nfonts/test.ttf\ncacert.pem\n";
  // Include the legacy APK layout too: reverting to it must reproduce the
  // stale-file bug rather than merely fail because a manifest is absent.
  for (const char* prefix : {"data", "bundled-data"})
  {
    write(apk / prefix / "manifest.txt", manifest);
    write(apk / prefix / "levels/01_rooftop_run.txt", level);
    write(apk / prefix / "fonts/test.ttf", "new font");
    write(apk / prefix / "cacert.pem", "new certificate");
  }
  write(fs::path(internal) / "data/levels/01_rooftop_run.txt", "old level without recovery ladder");
  write(fs::path(internal) / "data/cacert.pem", "old certificate");
  write(fs::path(internal) / "saves/profile.txt", "keep my progress");
  write(fs::path(internal) / "saves/slot1.txt", "keep my save");

  const std::string out = gr::prepareAndroidData();
  check(out == (fs::path(internal) / "data").string(), "upgrade keeps the existing data directory");
  check(read(fs::path(out) / "levels/01_rooftop_run.txt") == level,
    "upgrade replaces the old level with the APK's level including the recovery ladder");
  check(read(fs::path(out) / "cacert.pem") == "new certificate", "upgrade replaces other stale data");
  check(read(fs::path(out) / "fonts/test.ttf") == "new font", "upgrade extracts new files");
  check(read(fs::path(internal) / "saves/profile.txt") == "keep my progress" &&
      read(fs::path(internal) / "saves/slot1.txt") == "keep my save",
    "upgrade preserves the profile and saves");

  write(apk / "bundled-data/levels/01_rooftop_run.txt", level + "; next update\n");
  check(gr::prepareAndroidData() == out &&
      read(fs::path(out) / "levels/01_rooftop_run.txt") == level + "; next update\n",
    "a later update refreshes the extracted level again");
  fs::remove(apk / "bundled-data/cacert.pem");
  check(gr::prepareAndroidData().empty(), "missing APK data fails instead of reusing the extracted copy");
  fs::remove_all(root);
  return failures == 0 ? 0 : 1;
}
