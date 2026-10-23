// Only the SDL file API used by android_data.cpp. The host regression test
// supplies Android's internal-storage-first lookup without needing a device.
#pragma once

#include <cstddef>
#include <cstdio>

using Sint64 = long long;
struct SDL_RWops
{
  std::FILE* file;
};

const char* SDL_AndroidGetInternalStoragePath();
SDL_RWops* SDL_RWFromFile(const char* path, const char* mode);
Sint64 SDL_RWsize(SDL_RWops* rw);
std::size_t SDL_RWread(SDL_RWops* rw, void* data, std::size_t size, std::size_t count);
int SDL_RWclose(SDL_RWops* rw);
const char* SDL_GetError();
void SDL_Log(const char* format, ...);
