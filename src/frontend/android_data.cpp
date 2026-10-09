#include "frontend/android_data.hpp"

#include <SDL.h>

#include <cstdio>
#include <filesystem>
#include <sstream>
#include <vector>

namespace gr
{

namespace
{

// SDL_RWFromFile with a relative path reads from the APK's assets.
bool readAsset(const std::string& path, std::vector<char>& out)
{
  SDL_RWops* rw = SDL_RWFromFile(path.c_str(), "rb");
  if (!rw)
    return false;
  const Sint64 size = SDL_RWsize(rw);
  out.resize(size > 0 ? std::size_t(size) : 0);
  const bool ok = size >= 0 && SDL_RWread(rw, out.data(), 1, out.size()) == out.size();
  SDL_RWclose(rw);
  return ok;
}

bool writeFile(const std::filesystem::path& path, const std::vector<char>& data)
{
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  std::FILE* f = std::fopen(path.c_str(), "wb");
  if (!f)
    return false;
  const bool ok = std::fwrite(data.data(), 1, data.size(), f) == data.size();
  return std::fclose(f) == 0 && ok;
}

} // namespace

std::string prepareAndroidData()
{
  const char* internal = SDL_AndroidGetInternalStoragePath();
  if (!internal)
  {
    SDL_Log("no internal storage: %s", SDL_GetError());
    return {};
  }
  const std::filesystem::path root = std::filesystem::path(internal) / "data";
  std::vector<char> manifest;
  if (!readAsset("data/manifest.txt", manifest))
  {
    SDL_Log("no data/manifest.txt in the APK: %s", SDL_GetError());
    return {};
  }
  std::istringstream lines(std::string(manifest.begin(), manifest.end()));
  std::string name;
  std::vector<char> data;
  int copied = 0;
  while (std::getline(lines, name))
  {
    if (name.empty())
      continue;
    if (!readAsset("data/" + name, data) || !writeFile(root / name, data))
    {
      SDL_Log("cannot unpack %s", name.c_str());
      return {};
    }
    ++copied;
  }
  SDL_Log("unpacked %d data files to %s", copied, root.c_str());
  return root.string();
}

std::string androidSaveDir()
{
  const char* internal = SDL_AndroidGetInternalStoragePath();
  return internal ? std::string(internal) + "/saves" : std::string("saves");
}

} // namespace gr
