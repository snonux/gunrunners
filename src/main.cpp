// Gunrunners - proof of concept.
//
// Runs either in a GPU-accelerated window (SDL2) or fully headless with SDL's
// software renderer, in which case every 1280x720 frame can be streamed as
// raw BGRA pixels to a file or stdout (e.g. into ffmpeg) to record gameplay
// clips without a display.

#include "frontend/game.hpp"
#include "frontend/controls.hpp"

#include <SDL.h>
#include <cairo.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace gr;

namespace
{

struct CliOptions
{
  GameOptions game;
  bool headless = false;
  long maxFrames = -1;
  std::string rawOut;
  std::string audioOut;
  bool noAudio = false;
  std::string screenshotPrefix;
  std::vector<long> screenshotFrames;
  bool fullscreen = false;
  // Headless: scripted button presses, tick -> buttons held for 3 ticks.
  std::vector<std::pair<long, Input>> presses;
};

// "120:pause,140:down+confirm" -> presses. Used to test menus headlessly.
bool parsePresses(const std::string& spec, std::vector<std::pair<long, Input>>& out)
{
  std::stringstream ss(spec);
  std::string item;
  while (std::getline(ss, item, ','))
  {
    const auto colon = item.find(':');
    if (colon == std::string::npos)
      return false;
    Input in;
    std::stringstream bs(item.substr(colon + 1));
    std::string b;
    while (std::getline(bs, b, '+'))
    {
      if (b == "left") in.left = true;
      else if (b == "right") in.right = true;
      else if (b == "up") in.up = true;
      else if (b == "down") in.down = true;
      else if (b == "jump") in.jump = true;
      else if (b == "fire") in.fire = true;
      else if (b == "confirm") in.confirm = true;
      else if (b == "pause") in.pause = true;
      else if (b == "back") in.back = true;
      else if (b == "swap") in.swap = true;
      else return false;
    }
    out.emplace_back(std::atol(item.substr(0, colon).c_str()), in);
  }
  return true;
}

void printUsage()
{
  std::puts(
    "Usage: gunrunners [options]\n"
    "  --theme N            0 = Neon Overdrive, 1 = Lost Temple, 2 = Station Zero\n"
    "  --character N        0 = Dash, 1 = Rocco, 2 = Nova\n"
    "  --level PATH         level file (default: levels/level1.txt)\n"
    "  --skip-menu          start straight in the level\n"
    "  --autoplay           let the bot play (menu + level)\n"
    "  --quit-after-clear   exit after the results screen\n"
    "  --headless           no window; use with --raw-out to record\n"
    "  --raw-out PATH       write raw 1280x720 BGRA frames at 60 fps ('-' = stdout)\n"
    "  --audio-out PATH     headless: write the matching soundtrack as a WAV file\n"
    "  --no-audio           windowed: run without sound\n"
    "  --trace              print the player state every logic frame\n"
    "  --screenshots LIST   headless: save PNGs of these ticks, e.g. 100,250\n"
    "  --screenshot-prefix P  path prefix for those PNGs (default shot_)\n"
    "  --frames N           stop after N ticks\n"
    "  --fullscreen         start in fullscreen\n"
    "  --save-dir PATH      where the 5 savegame slots live\n"
    "                       (default: $XDG_DATA_HOME/gunrunners/saves or\n"
    "                       ~/.local/share/gunrunners/saves)\n"
    "  --press LIST         headless: scripted presses, e.g. 300:pause,320:down\n"
    "                       (left right up down jump fire confirm pause back swap)\n"
    "\n"
    "Keys: arrows/WASD move, Z/Space jump, X/Ctrl fire, Enter confirm,\n"
    "      C switch runner, Esc/P pause menu (save, load, quit), T cycle theme\n"
    "Gamepad: stick/d-pad move, A jump, X/B/RB/RT fire, Y switch runner,\n"
    "      Start pause menu, Back cycle theme");
}

bool fileExists(const std::string& p)
{
  std::ifstream f(p);
  return bool(f);
}

std::string defaultLevelPath()
{
  for (const char* p : {"levels/level1.txt", "../levels/level1.txt"})
    if (fileExists(p))
      return p;
  return std::string(GR_DATA_DIR) + "/levels/level1.txt";
}

bool parseArgs(int argc, char** argv, CliOptions& o)
{
  for (int i = 1; i < argc; ++i)
  {
    const std::string a = argv[i];
    auto next = [&]() -> const char* {
      if (i + 1 >= argc)
      {
        std::fprintf(stderr, "missing value for %s\n", a.c_str());
        std::exit(2);
      }
      return argv[++i];
    };
    if (a == "--theme")
      o.game.theme = std::atoi(next());
    else if (a == "--character")
      o.game.character = std::atoi(next());
    else if (a == "--level")
      o.game.levelPath = next();
    else if (a == "--skip-menu")
      o.game.skipMenu = true;
    else if (a == "--autoplay")
      o.game.autoplay = true;
    else if (a == "--quit-after-clear")
      o.game.quitAfterClear = true;
    else if (a == "--headless")
      o.headless = true;
    else if (a == "--raw-out")
      o.rawOut = next();
    else if (a == "--audio-out")
      o.audioOut = next();
    else if (a == "--no-audio")
      o.noAudio = true;
    else if (a == "--trace")
      o.game.trace = true;
    else if (a == "--frames")
      o.maxFrames = std::atol(next());
    else if (a == "--screenshots")
    {
      std::stringstream ss(next());
      std::string item;
      while (std::getline(ss, item, ','))
        o.screenshotFrames.push_back(std::atol(item.c_str()));
    }
    else if (a == "--screenshot-prefix")
      o.screenshotPrefix = next();
    else if (a == "--fullscreen")
      o.fullscreen = true;
    else if (a == "--save-dir")
      o.game.saveDir = next();
    else if (a == "--press")
    {
      if (!parsePresses(next(), o.presses))
      {
        std::fprintf(stderr, "bad --press list\n");
        std::exit(2);
      }
    }
    else if (a == "--help" || a == "-h")
    {
      printUsage();
      return false;
    }
    else
    {
      std::fprintf(stderr, "unknown option: %s\n", a.c_str());
      printUsage();
      std::exit(2);
    }
  }
  if (o.game.levelPath.empty())
    o.game.levelPath = defaultLevelPath();
  return true;
}

// Minimal 16-bit stereo WAV writer; the sizes are patched in on close.
class WavWriter
{
public:
  explicit WavWriter(const std::string& path) : mFile(std::fopen(path.c_str(), "wb"))
  {
    if (!mFile)
      return;
    const unsigned char header[44] = {};
    std::fwrite(header, 1, sizeof(header), mFile);
  }
  ~WavWriter()
  {
    if (!mFile)
      return;
    const auto put32 = [&](std::uint32_t v) { std::fwrite(&v, 4, 1, mFile); };
    const auto put16 = [&](std::uint16_t v) { std::fwrite(&v, 2, 1, mFile); };
    std::fseek(mFile, 0, SEEK_SET);
    std::fwrite("RIFF", 1, 4, mFile);
    put32(36 + mBytes);
    std::fwrite("WAVEfmt ", 1, 8, mFile);
    put32(16);
    put16(1);
    put16(2);
    put32(kAudioRate);
    put32(kAudioRate * 4);
    put16(4);
    put16(16);
    std::fwrite("data", 1, 4, mFile);
    put32(mBytes);
    std::fclose(mFile);
  }
  void write(const float* stereo, int frames)
  {
    if (!mFile)
      return;
    std::vector<std::int16_t> pcm(std::size_t(frames) * 2);
    for (std::size_t i = 0; i < pcm.size(); ++i)
      pcm[i] = std::int16_t(std::lround(std::max(-1.0f, std::min(1.0f, stereo[i])) * 32767.0f));
    std::fwrite(pcm.data(), 2, pcm.size(), mFile);
    mBytes += std::uint32_t(pcm.size() * 2);
  }

private:
  std::FILE* mFile;
  std::uint32_t mBytes = 0;
};

void savePng(SDL_Surface* surface, const std::string& path)
{
  cairo_surface_t* cs = cairo_image_surface_create_for_data(
    static_cast<unsigned char*>(surface->pixels),
    CAIRO_FORMAT_RGB24,
    surface->w,
    surface->h,
    surface->pitch);
  cairo_surface_write_to_png(cs, path.c_str());
  cairo_surface_destroy(cs);
}

int runHeadless(const CliOptions& o)
{
  SDL_Init(0);
  SDL_Surface* surface =
    SDL_CreateRGBSurfaceWithFormat(0, kScreenW, kScreenH, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_Renderer* sdlRenderer = SDL_CreateSoftwareRenderer(surface);
  if (!surface || !sdlRenderer)
  {
    std::fprintf(stderr, "cannot create software renderer: %s\n", SDL_GetError());
    return 1;
  }
  int result = 0;
  {
    Renderer renderer(sdlRenderer);
    std::unique_ptr<Audio> audio;
    std::unique_ptr<WavWriter> wav;
    if (!o.audioOut.empty())
    {
      audio = std::make_unique<Audio>();
      wav = std::make_unique<WavWriter>(o.audioOut);
    }
    Game game(o.game, renderer, audio.get());
    std::vector<float> audioFrame(std::size_t(kAudioRate / 60) * 2);
    std::FILE* out = nullptr;
    if (o.rawOut == "-")
      out = stdout;
    else if (!o.rawOut.empty())
      out = std::fopen(o.rawOut.c_str(), "wb");
    const std::string prefix = o.screenshotPrefix.empty() ? "shot_" : o.screenshotPrefix;

    long frames = 0;
    while (o.maxFrames < 0 || frames < o.maxFrames)
    {
      Input scripted;
      for (const auto& [at, in] : o.presses)
        if (frames >= at && frames < at + 3)
          scripted = scripted | in;
      if (!game.tick(scripted))
        break;
      ++frames;
      if (audio)
      {
        audio->mix(audioFrame.data(), kAudioRate / 60);
        wav->write(audioFrame.data(), kAudioRate / 60);
      }
      const bool shot = std::find(o.screenshotFrames.begin(), o.screenshotFrames.end(), frames) !=
        o.screenshotFrames.end();
      if (out || shot)
      {
        game.render();
        SDL_RenderPresent(sdlRenderer);
        if (shot)
          savePng(surface, prefix + std::to_string(frames) + ".png");
        if (out)
        {
          for (int y = 0; y < kScreenH; ++y)
            std::fwrite(static_cast<const char*>(surface->pixels) + y * surface->pitch, 4, kScreenW, out);
        }
      }
      if (o.maxFrames < 0 && frames > 60L * 60L * 10L)
      {
        std::fprintf(stderr, "giving up after 10 minutes of game time\n");
        result = 1;
        break;
      }
    }
    if (out && out != stdout)
      std::fclose(out);
    std::fprintf(stderr, "ran %ld frames\n", frames);
  }
  SDL_DestroyRenderer(sdlRenderer);
  SDL_FreeSurface(surface);
  SDL_Quit();
  return result;
}

int runWindowed(const CliOptions& o)
{
  if (SDL_Init(SDL_INIT_VIDEO) != 0)
  {
    std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
  SDL_Window* window = SDL_CreateWindow(
    "Gunrunners PoC",
    SDL_WINDOWPOS_CENTERED,
    SDL_WINDOWPOS_CENTERED,
    kScreenW,
    kScreenH,
    SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | (o.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0));
  SDL_Renderer* sdlRenderer =
    SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!sdlRenderer)
    sdlRenderer = SDL_CreateRenderer(window, -1, 0);
  SDL_RenderSetLogicalSize(sdlRenderer, kScreenW, kScreenH);

  {
  Renderer renderer(sdlRenderer);
  std::unique_ptr<Audio> audio;
  if (!o.noAudio)
  {
    audio = std::make_unique<Audio>();
    if (!audio->openDevice())
      audio.reset();
  }
  Game game(o.game, renderer, audio.get());
  Controls controls;
  const double tickSeconds = 1.0 / 60.0;
  const double freq = double(SDL_GetPerformanceFrequency());
  Uint64 last = SDL_GetPerformanceCounter();
  double accumulator = 0.0;
  long frames = 0;
  bool running = true;

  while (running)
  {
    SDL_Event ev;
    while (SDL_PollEvent(&ev))
    {
      switch (controls.handleEvent(ev))
      {
        case Controls::Action::Quit:
          running = false;
          break;
        case Controls::Action::CycleTheme:
          game.cycleTheme();
          break;
        case Controls::Action::None:
          break;
      }
    }

    const Uint64 now = SDL_GetPerformanceCounter();
    accumulator += double(now - last) / freq;
    last = now;
    accumulator = std::min(accumulator, 0.25);
    while (accumulator >= tickSeconds && running)
    {
      running = game.tick(controls.read());
      accumulator -= tickSeconds;
      ++frames;
      if (o.maxFrames >= 0 && frames >= o.maxFrames)
        running = false;
    }

    game.render();
    SDL_RenderPresent(sdlRenderer);
  }
  }

  SDL_DestroyRenderer(sdlRenderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}

} // namespace

int main(int argc, char** argv)
{
  CliOptions o;
  if (!parseArgs(argc, argv, o))
    return 0;
  try
  {
    return o.headless ? runHeadless(o) : runWindowed(o);
  }
  catch (const std::exception& e)
  {
    std::fprintf(stderr, "error: %s\n", e.what());
    return 1;
  }
}
