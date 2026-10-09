// Gunrunners - proof of concept.
//
// Runs either in a GPU-accelerated window (SDL2) or fully headless with SDL's
// software renderer, in which case every 1280x720 frame can be streamed as
// raw BGRA pixels to a file or stdout (e.g. into ffmpeg) to record gameplay
// clips without a display.

#include "frontend/game.hpp"
#include "frontend/controls.hpp"
#include "frontend/touch_controls.hpp"
#include "game/profile.hpp"
#include "game/savegame.hpp"
#include "render/vector.hpp"
#ifdef __ANDROID__
#include "frontend/android_data.hpp"
#endif

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
  bool windowed = false;
  bool touch = false; // on-screen gamepad (always on Android)
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
    "  --level PATH         play just this level file (default: the campaign)\n"
    "  --start N            campaign: start at level N (with --skip-menu)\n"
    "  --skip-menu          start straight in the level (after its briefing)\n"
    "  --cheats             the pause menu shows CHEATS without the code\n"
    "  --no-cutscenes       skip the opening, briefings and episode endings\n"
    "  --cutscene NAME      play cutscenes/NAME.txt, then quit\n"
    "  --data-dir PATH      where levels/ and cutscenes/ are\n"
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
    "  --fullscreen         start in borderless fullscreen (F11 or Alt+Enter toggles)\n"
    "  --windowed           start in a window even if fullscreen was saved\n"
    "  --touch              show the on-screen gamepad for touch screens\n"
    "  --save-dir PATH      where the 5 savegame slots live\n"
    "                       (default: $XDG_DATA_HOME/gunrunners/saves or\n"
    "                       ~/.local/share/gunrunners/saves)\n"
    "  --press LIST         headless: scripted presses, e.g. 300:pause,320:down\n"
    "                       (left right up down jump fire confirm pause back swap)\n"
    "\n"
    "Keys: arrows/WASD move, X/Ctrl jump, Z/Space fire, Enter confirm,\n"
    "      C switch runner, Esc/P pause menu (save, load, quit), T cycle theme,\n"
    "      F11 or Alt+Enter fullscreen\n"
    "Gamepad: stick/d-pad move, X jump, A/B/RB/RT fire, Y switch runner,\n"
    "      Start pause menu, Back cycle theme\n"
    "CONTROLS on the title screen or in the pause menu rebinds keys and buttons");
}

// The on-screen gamepad follows the game: its layout for the moment, its
// size, where the player moved its controls, and the layout editor.
void syncTouch(TouchControls& touch, Game& game)
{
  touch.setEditing(game.touchEditing());
  touch.configure(game.inLevel() || game.touchEditing() ? TouchControls::Layout::Play : TouchControls::Layout::Menu,
    game.touchSize());
  if (touch.takeLayoutChange())
    game.setTouchLayout(touch.layout());
  else
    touch.setLayout(game.touchLayout());
}

// While the CONTROLS menu listens for a new binding, the next key or pad
// button goes to it instead of to the game.
bool captureBinding(Game& game, const SDL_Event& ev)
{
  if (!game.listening())
    return false;
  switch (ev.type)
  {
    case SDL_KEYDOWN:
      if (!ev.key.repeat)
        game.captureKey(ev.key.keysym.scancode);
      return true;
    case SDL_CONTROLLERBUTTONDOWN:
      game.capturePad(ev.cbutton.button);
      return true;
    case SDL_CONTROLLERAXISMOTION:
      if (ev.caxis.value > 20000 && ev.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT)
        game.capturePad(kPadLeftTrigger);
      else if (ev.caxis.value > 20000 && ev.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT)
        game.capturePad(kPadRightTrigger);
      return false;
    default:
      return false;
  }
}

bool fileExists(const std::string& p)
{
  std::ifstream f(p);
  return bool(f);
}

// Levels next to the binary (an unpacked release), in or above the current
// directory (the source tree), else where it was built from.
std::string defaultDataDir()
{
  std::vector<std::string> dirs = {".", ".."};
  if (char* base = SDL_GetBasePath())
  {
    dirs.insert(dirs.begin(), {base, std::string(base) + ".."});
    SDL_free(base);
  }
  for (const std::string& d : dirs)
    if (fileExists(d + "/levels/01_rooftop_run.txt"))
      return d;
  return GR_DATA_DIR;
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
    else if (a == "--start")
      o.game.startLevel = std::atoi(next());
    else if (a == "--no-cutscenes")
      o.game.noCutscenes = true;
    else if (a == "--cutscene")
      o.game.cutscene = next();
    else if (a == "--data-dir")
      o.game.dataDir = next();
    else if (a == "--skip-menu")
      o.game.skipMenu = true;
    else if (a == "--cheats")
      o.game.cheats = true;
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
    else if (a == "--windowed")
      o.windowed = true;
    else if (a == "--touch")
      o.touch = true;
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
  if (o.game.dataDir.empty())
    o.game.dataDir = defaultDataDir();
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
#ifdef CAIRO_HAS_PNG_FUNCTIONS // not in the minimal Cairo of the Android build
  cairo_surface_write_to_png(cs, path.c_str());
#else
  (void)path;
#endif
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
    std::unique_ptr<TouchControls> touch;
    if (o.touch)
      touch = std::make_unique<TouchControls>(renderer, true);
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
        if (touch)
        {
          syncTouch(*touch, game);
          touch->draw(renderer);
        }
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
#ifdef __ANDROID__
  // Landscape only, and the back key reaches the game (pause / back out of
  // a menu) instead of closing the app.
  SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
  SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
#endif
  // Borderless fullscreen at the desktop resolution: no mode switch, no
  // window decorations, and the logical size keeps the 1280x720 picture
  // letterboxed. The saved setting applies unless a flag overrides it. The
  // window is created fullscreen so it also works without a window manager.
  const std::string saveDir = o.game.saveDir.empty() ? defaultSaveDir() : o.game.saveDir;
  const bool startFullscreen = !o.windowed && (o.fullscreen || Profile::load(saveDir).fullscreen);
  SDL_Window* window = SDL_CreateWindow(
    "Gunrunners",
    SDL_WINDOWPOS_CENTERED,
    SDL_WINDOWPOS_CENTERED,
    kScreenW,
    kScreenH,
    SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | (startFullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0));
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
  GameOptions gameOptions = o.game;
  // FULLSCREEN in the menus; an Android app is always fullscreen.
#ifndef __ANDROID__
  gameOptions.window = true;
#endif
  Game game(gameOptions, renderer, audio.get());

  const auto applyFullscreen = [&](bool on) {
    SDL_SetWindowFullscreen(window, on ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    SDL_ShowCursor(on ? SDL_DISABLE : SDL_ENABLE);
  };
  const auto isFullscreen = [&] {
    return (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;
  };
  const auto toggleFullscreen = [&] {
    const bool on = !isFullscreen();
    applyFullscreen(on);
    game.setFullscreen(on);
  };
  SDL_ShowCursor(startFullscreen ? SDL_DISABLE : SDL_ENABLE);
  game.showFullscreen(startFullscreen);

  Controls controls;
  std::unique_ptr<TouchControls> touch;
  if (o.touch)
    touch = std::make_unique<TouchControls>(renderer, true);
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
      if (touch)
        touch->handleEvent(ev, window);
      // Android may close a backgrounded app without warning.
      if (ev.type == SDL_APP_WILLENTERBACKGROUND || ev.type == SDL_APP_TERMINATING)
        game.suspend();
      if (captureBinding(game, ev))
        continue;
      switch (controls.handleEvent(ev))
      {
        case Controls::Action::Quit:
          running = false;
          break;
        case Controls::Action::CycleTheme:
          game.cycleTheme();
          break;
        case Controls::Action::ToggleFullscreen:
          toggleFullscreen();
          break;
        case Controls::Action::None:
          break;
      }
    }

    const Uint64 now = SDL_GetPerformanceCounter();
    accumulator += double(now - last) / freq;
    last = now;
    accumulator = std::min(accumulator, 0.25);
    if (controls.bindings() != game.bindings())
      controls.setBindings(game.bindings());
    if (touch)
    {
      int pw = 0, ph = 0;
      SDL_GetRendererOutputSize(sdlRenderer, &pw, &ph);
      touch->setScreen(pw, ph);
      syncTouch(*touch, game);
    }
    while (accumulator >= tickSeconds && running)
    {
      const Input keysAndPads = controls.read(!game.inLevel());
      running = game.tick(touch ? keysAndPads | touch->read() : keysAndPads);
      if (touch)
        syncTouch(*touch, game);
      if (game.takeFullscreenToggle())
        toggleFullscreen();
      accumulator -= tickSeconds;
      ++frames;
      if (o.maxFrames >= 0 && frames >= o.maxFrames)
        running = false;
    }

    game.render();
    if (touch)
      touch->draw(renderer);
    SDL_RenderPresent(sdlRenderer);
#ifdef __ANDROID__
    static Uint32 fpsStart = SDL_GetTicks(), fpsFrames = 0;
    if (++fpsFrames == 300 || SDL_GetTicks() - fpsStart > 10000)
    {
      SDL_Log("%.1f fps", fpsFrames * 1000.0 / double(SDL_GetTicks() - fpsStart));
      fpsStart = SDL_GetTicks();
      fpsFrames = 0;
    }
#endif
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
#ifdef __ANDROID__
  // No command line on Android: the game data is unpacked from the APK,
  // saves go to private storage, and the screen is the gamepad.
  o.game.dataDir = prepareAndroidData();
  if (o.game.dataDir.empty())
  {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Gunrunners", "Could not unpack the game data.", nullptr);
    return 1;
  }
  o.game.saveDir = androidSaveDir();
  o.touch = true;
  o.fullscreen = true;
  loadGameFont((o.game.dataDir + "/fonts/DejaVuSans-Bold.ttf").c_str());
#endif
  o.game.touch = o.touch;
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
