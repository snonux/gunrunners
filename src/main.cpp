// Gunrunners - proof of concept.
//
// Runs either in a GPU-accelerated window (SDL2) or fully headless with SDL's
// software renderer, in which case every 1280x720 frame can be streamed as
// raw BGRA pixels to a file or stdout (e.g. into ffmpeg) to record gameplay
// clips without a display.

#include "frontend/game.hpp"

#include <SDL.h>
#include <cairo.h>

#include <cstdio>
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
  std::string screenshotPrefix;
  std::vector<long> screenshotFrames;
  bool fullscreen = false;
};

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
    "  --screenshots LIST   headless: save PNGs of these ticks, e.g. 100,250\n"
    "  --screenshot-prefix P  path prefix for those PNGs (default shot_)\n"
    "  --frames N           stop after N ticks\n"
    "  --fullscreen         start in fullscreen\n"
    "\n"
    "Keys: arrows/WASD move, Z/Space jump, X/Ctrl fire, Enter confirm,\n"
    "      T cycle theme, Esc quit");
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
    Game game(o.game, renderer);
    std::FILE* out = nullptr;
    if (o.rawOut == "-")
      out = stdout;
    else if (!o.rawOut.empty())
      out = std::fopen(o.rawOut.c_str(), "wb");
    const std::string prefix = o.screenshotPrefix.empty() ? "shot_" : o.screenshotPrefix;

    long frames = 0;
    while (o.maxFrames < 0 || frames < o.maxFrames)
    {
      if (!game.tick(Input{}))
        break;
      ++frames;
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

Input readKeyboard()
{
  const Uint8* k = SDL_GetKeyboardState(nullptr);
  Input in;
  in.left = k[SDL_SCANCODE_LEFT] || k[SDL_SCANCODE_A];
  in.right = k[SDL_SCANCODE_RIGHT] || k[SDL_SCANCODE_D];
  in.up = k[SDL_SCANCODE_UP] || k[SDL_SCANCODE_W];
  in.down = k[SDL_SCANCODE_DOWN] || k[SDL_SCANCODE_S];
  in.jump = k[SDL_SCANCODE_Z] || k[SDL_SCANCODE_SPACE];
  in.fire = k[SDL_SCANCODE_X] || k[SDL_SCANCODE_LCTRL] || k[SDL_SCANCODE_RCTRL];
  in.confirm = k[SDL_SCANCODE_RETURN];
  return in;
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
  Game game(o.game, renderer);
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
      if (ev.type == SDL_QUIT)
        running = false;
      if (ev.type == SDL_KEYDOWN && !ev.key.repeat)
      {
        if (ev.key.keysym.sym == SDLK_ESCAPE)
          running = false;
        if (ev.key.keysym.sym == SDLK_t)
          game.cycleTheme();
      }
    }

    const Uint64 now = SDL_GetPerformanceCounter();
    accumulator += double(now - last) / freq;
    last = now;
    accumulator = std::min(accumulator, 0.25);
    while (accumulator >= tickSeconds && running)
    {
      running = game.tick(readKeyboard());
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
