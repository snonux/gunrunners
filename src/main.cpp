// Gunrunners - proof of concept.
//
// Runs either in a GPU-accelerated window (SDL2) or fully headless with SDL's
// software renderer, in which case every 1280x720 frame can be streamed as
// raw BGRA pixels to a file or stdout (e.g. into ffmpeg) to record gameplay
// clips without a display.

#include "base/frame_pacing.hpp"
#include "frontend/game.hpp"
#include "frontend/controls.hpp"
#include "frontend/touch_controls.hpp"
#include "frontend/window_renderer.hpp"
#include "game/profile.hpp"
#include "game/savegame.hpp"
#include "render/vector.hpp"
#ifdef __ANDROID__
#include "frontend/android_data.hpp"
#endif
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
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
  // Headless: scripted button presses, ticks [from, to) -> buttons held
  // (a single tick holds them for 3 ticks).
  struct Press { long from, to; Input in; };
  std::vector<Press> presses;
  // Headless: scripted taps (tick -> x, y) and drags (tick -> dx, dy).
  std::vector<std::pair<long, SDL_FPoint>> taps, drags;
  // Headless (with --touch): a finger on the on-screen controls at a tick,
  // held for 20 ticks, e.g. 300:900x200.
  std::vector<std::pair<long, SDL_FPoint>> fingers;
};

std::string environment(const char* name)
{
  const char* value = std::getenv(name);
  return value ? value : "";
}

// "120:640x300,200:100x0" -> (tick, x, y) pairs, for --tap and --drag.
bool parsePoints(const std::string& spec, std::vector<std::pair<long, SDL_FPoint>>& out)
{
  std::stringstream ss(spec);
  std::string item;
  while (std::getline(ss, item, ','))
  {
    const auto colon = item.find(':');
    const auto x = item.find('x', colon == std::string::npos ? 0 : colon);
    if (colon == std::string::npos || x == std::string::npos)
      return false;
    out.emplace_back(std::atol(item.substr(0, colon).c_str()),
      SDL_FPoint{float(std::atof(item.substr(colon + 1, x - colon - 1).c_str())),
        float(std::atof(item.substr(x + 1).c_str()))});
  }
  return true;
}

// "120:pause,140:down+confirm,200-260:right" -> presses (a range holds the
// buttons from its first tick up to its last). Used to test menus headlessly
// and to play a level by hand, tick by tick.
bool parsePresses(const std::string& spec, std::vector<CliOptions::Press>& out)
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
      else if (b == "quicksave") in.quickSave = true;
      else if (b == "quickload") in.quickLoad = true;
      else if (b == "map") in.map = true;
      else if (b == "use") in.use = true;
      else if (b == "say") in.say = true;
      else if (b == "random") in.randomRunner = true;
      else if (b == "weapon") in.nextWeapon = true;
      else if (b == "aimleft") in.aimLeft = true;
      else if (b == "aimright") in.aimRight = true;
      else if (b == "upjump") in.up = in.jump = in.upJump = true;
      else if (b.rfind("stick", 0) == 0 && b.size() > 5)
      {
        // The right stick pushed at this angle (degrees, 0 right, 90 up).
        const float a = float(std::atof(b.c_str() + 5)) * 0.0174533f;
        aimStick(in, std::cos(a), -std::sin(a));
      }
      else return false;
    }
    const std::string when = item.substr(0, colon);
    const auto dash = when.find('-');
    const long from = std::atol(when.substr(0, dash).c_str());
    const long to = dash == std::string::npos ? from + 3 : std::atol(when.substr(dash + 1).c_str()) + 1;
    out.push_back({from, to, in});
  }
  return true;
}

void printUsage()
{
  std::puts(
    "Usage: gunrunners [options]\n"
    "  --theme N            0 = Neon Overdrive, 1 = Lost Temple, 2 = Station Zero\n"
    "  --character N        0 Dash, 1 Rocco, 2 Nova, 3 Jade, 4 Skye, 5 Bolt,\n"
    "                       6 and up your own runners (runners.txt); with Snonux\n"
    "                       unlocked he is 6 and your own runners follow\n"
    "  --snonux             unlock Snonux, the secret runner, for this run\n"
    "  --level PATH         play just this level file (default: the campaign)\n"
    "  --start N            campaign: start at level N (with --skip-menu)\n"
    "  --skip-menu          start straight in the level (after its briefing)\n"
    "  --cheats             the pause menu shows CHEATS without the code\n"
    "  --no-cutscenes       skip the opening, briefings and episode endings\n"
    "  --hard               HARD: more enemies in every level\n"
    "  --cutscene NAME      play cutscenes/NAME.txt, then quit\n"
    "  --data-dir PATH      where levels/ and cutscenes/ are\n"
    "  --autoplay           let the bot play (menu + level)\n"
    "  --quit-after-clear   exit after the results screen\n"
    "  --headless           no window; use with --raw-out to record\n"
    "  --raw-out PATH       write raw 1280x720 BGRA frames at 60 fps ('-' = stdout)\n"
    "  --audio-out PATH     headless: write the matching soundtrack as a WAV file\n"
    "  --no-audio           windowed: run without sound\n"
    "  --trace              print the player state every logic frame\n"
    "  --voice-log          print the runners' lines as they trigger and play\n"
    "  --screenshots LIST   headless: save PNGs of these ticks, e.g. 100,250\n"
    "  --screenshot-prefix P  path prefix for those PNGs (default shot_)\n"
    "  --frames N           stop after N ticks\n"
    "  --fullscreen         start in borderless fullscreen (F11 or Alt+Enter toggles)\n"
    "  --windowed           start in a window even if fullscreen was saved\n"
    "  --touch              show the on-screen gamepad for touch screens\n"
    "  --save-dir PATH      where the 5 savegame slots live\n"
    "                       (default: $XDG_DATA_HOME/gunrunners/saves or\n"
    "                       ~/.local/share/gunrunners/saves)\n"
    "  --s3-endpoint URL    also store savegame slots in an S3-compatible bucket\n"
    "  --s3-bucket NAME     bucket used with --s3-endpoint\n"
    "  --s3-region NAME     signing region (default: us-east-1)\n"
    "  --s3-prefix PATH     optional object-key prefix\n"
    "  --s3-access-key KEY  S3 access key (or AWS_ACCESS_KEY_ID)\n"
    "  --s3-secret-key KEY  S3 secret key (or AWS_SECRET_ACCESS_KEY)\n"
    "  --press LIST         headless: scripted presses, e.g. 300:pause,320:down,\n"
    "                       400-460:right+jump (a range holds them)\n"
    "                       (left right up down jump fire confirm pause back swap\n"
    "                       quicksave quickload use; touch: aimleft aimright upjump;\n"
    "                       the right stick: stickDEG, e.g. stick45 up and right)\n"
    "  --tap LIST           headless: taps on the 1280x720 screen, e.g. 120:640x300\n"
    "  --drag LIST          headless: finger drags by dx x dy, e.g. 200:0x-120\n"
    "  --finger LIST        headless + --touch: a finger on the touch controls,\n"
    "                       held 20 ticks, e.g. 300:900x200 (shoots at the spot)\n"
    "\n"
    "Keys: arrows/WASD move, X/Ctrl jump, Z/Space fire, Enter confirm,\n"
    "      C switch runner, E get in or out of a vehicle, Esc/P pause menu\n"
    "      (save, load, quit), T cycle theme, F5 quick save, F9 quick load,\n"
    "      F11 or Alt+Enter fullscreen\n"
    "Gamepad: stick/d-pad move, A jump, X/B/RB/RT fire, Y switch runner, LB vehicle,\n"
    "      Start pause menu, Back map; the right stick aims at any angle and fires\n"
    "SETTINGS on the title screen or in the pause menu: difficulty, controls\n"
    "(rebind keys and buttons), assists and play options");
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
  touch.setThumbs(game.touchThumbs());
  touch.setMenuNavigation(game.touchNavigationTargets());
  const World* world = game.inLevel() ? game.world() : nullptr;
  touch.setVehicle(world && world->riding(), !world || world->canLeaveVehicle());
  SDL_FRect vehicleBounds{};
  const int vehicle = game.vehicleOnScreen(vehicleBounds);
  touch.setVehicleTarget(vehicleBounds, vehicle);
  float rx = 0.0f, ry = 0.0f;
  const bool known = game.runnerOnScreen(rx, ry);
  touch.setRunner(rx, ry, known);
  if (touch.takeMenuPadChange())
    game.setMenuPad(touch.menuPad());
  else
    touch.setMenuPad(game.menuPad());
}

// Taps and drags on the menus go straight to the game (before its tick).
void forwardTouch(TouchControls& touch, Game& game)
{
  for (const SDL_FPoint& p : touch.takeTaps())
    game.tapAt(p.x, p.y);
  const SDL_FPoint d = touch.takeDrag();
  if (d.x != 0.0f || d.y != 0.0f)
    game.dragBy(d.x, d.y);
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

// While the runner editor takes a name, typed text, Backspace and Enter go
// to it, and keys don't reach the bindings (or T, the theme switch).
bool captureText(Game& game, const SDL_Event& ev)
{
  if (!game.wantsText())
    return false;
  switch (ev.type)
  {
    case SDL_TEXTINPUT:
      game.typeText(ev.text.text);
      return true;
    case SDL_KEYDOWN:
      if (ev.key.keysym.scancode == SDL_SCANCODE_BACKSPACE)
        game.textBackspace();
      else if ((ev.key.keysym.scancode == SDL_SCANCODE_RETURN || ev.key.keysym.scancode == SDL_SCANCODE_KP_ENTER) &&
               !ev.key.repeat)
        game.textDone();
      return true;
    case SDL_KEYUP:
      return true;
    default:
      return false;
  }
}

bool fileExists(const std::string& p)
{
  std::ifstream f(p);
  return bool(f);
}

// Levels next to the binary (an unpacked release), in the installed data
// directory, in or above the current directory, else where it was built from.
std::string defaultDataDir()
{
  std::vector<std::string> dirs = {".", ".."};
  if (char* base = SDL_GetBasePath())
  {
    dirs.insert(dirs.begin(), {base, std::string(base) + ".."});
#ifdef GR_INSTALLED_DATA_RELATIVE
    dirs.insert(dirs.begin() + 2, std::string(base) + GR_INSTALLED_DATA_RELATIVE);
#endif
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
    else if (a == "--hard")
      o.game.hard = true;
    else if (a == "--snonux")
      o.game.snonux = true;
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
    else if (a == "--voice-log")
      o.game.voiceLog = true;
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
    else if (a == "--s3-endpoint")
      o.game.s3.endpoint = next();
    else if (a == "--s3-bucket")
      o.game.s3.bucket = next();
    else if (a == "--s3-region")
      o.game.s3.region = next();
    else if (a == "--s3-prefix")
      o.game.s3.prefix = next();
    else if (a == "--s3-access-key")
      o.game.s3.accessKey = next();
    else if (a == "--s3-secret-key")
      o.game.s3.secretKey = next();
    else if (a == "--press")
    {
      if (!parsePresses(next(), o.presses))
      {
        std::fprintf(stderr, "bad --press list\n");
        std::exit(2);
      }
    }
    else if (a == "--tap" || a == "--drag" || a == "--finger")
    {
      if (!parsePoints(next(), a == "--tap" ? o.taps : a == "--drag" ? o.drags : o.fingers))
      {
        std::fprintf(stderr, "bad %s list\n", a.c_str());
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
  if (o.game.s3.endpoint.empty())
    o.game.s3.endpoint = environment("GUNRUNNERS_S3_ENDPOINT");
  if (o.game.s3.bucket.empty())
    o.game.s3.bucket = environment("GUNRUNNERS_S3_BUCKET");
  if (o.game.s3.prefix.empty())
    o.game.s3.prefix = environment("GUNRUNNERS_S3_PREFIX");
  if (o.game.s3.accessKey.empty())
    o.game.s3.accessKey = environment("AWS_ACCESS_KEY_ID");
  if (o.game.s3.secretKey.empty())
    o.game.s3.secretKey = environment("AWS_SECRET_ACCESS_KEY");
  if (const std::string region = environment("AWS_REGION");
      !region.empty() && o.game.s3.region == "us-east-1")
    o.game.s3.region = region;
  if (o.game.s3.enabled())
  {
    std::string error;
    if (!o.game.s3.valid(&error))
    {
      std::fprintf(stderr, "invalid S3 configuration: %s\n", error.c_str());
      std::exit(2);
    }
  }
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
      for (const auto& press : o.presses)
        if (frames >= press.from && frames < press.to)
          scripted = scripted | press.in;
      for (const auto& [at, p] : o.taps)
        if (frames == at)
        {
          game.render(); // what a tap hits is what was drawn
          game.tapAt(p.x, p.y);
        }
      for (const auto& [at, d] : o.drags)
        if (frames == at)
          game.dragBy(d.x, d.y);
      if (touch && !o.fingers.empty())
      {
        const Uint32 ms = Uint32(frames * 1000 / 60);
        syncTouch(*touch, game);
        for (std::size_t i = 0; i < o.fingers.size(); ++i)
        {
          const auto& [at, p] = o.fingers[i];
          if (frames == at)
            touch->touch(SDL_FingerID(i + 1), p.x, p.y, ms);
          else if (frames == at + 20)
            touch->release(SDL_FingerID(i + 1));
        }
        touch->advance(ms);
        scripted = scripted | touch->read();
      }
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

// One windowed game: everything the main loop needs, on the heap, so a
// browser page can run it one frame per animation frame (the stack of
// runWindowed is gone by then) and a desktop loop calls frame() and then
// sleeps for pauseMs(). It owns the window's SDL_Renderer (see
// WindowRenderer, frontend/window_renderer.hpp), not the window.
struct WindowedRun
{
  WindowedRun(const CliOptions& options, SDL_Window* w, bool startFullscreen)
    : o(options)
    , window(w)
    , sdlRenderer(w)
    , renderer(sdlRenderer.sdl())
    , audio(openAudio(options))
    , game(windowOptions(options), renderer, audio.get())
  {
    SDL_ShowCursor(startFullscreen ? SDL_DISABLE : SDL_ENABLE);
    game.showFullscreen(startFullscreen);
    if (o.touch)
      touch = std::make_unique<TouchControls>(renderer, true);
  }

  static std::unique_ptr<Audio> openAudio(const CliOptions& o)
  {
    if (o.noAudio)
      return nullptr;
    auto audio = std::make_unique<Audio>();
    if (!audio->openDevice())
      audio.reset();
    return audio;
  }

  static GameOptions windowOptions(const CliOptions& o)
  {
    GameOptions options = o.game;
    // FULLSCREEN in the menus; an Android app is always fullscreen.
#ifndef __ANDROID__
    options.window = true;
#endif
    return options;
  }

#ifdef __EMSCRIPTEN__
  // In a browser the play page owns full screen (docs/play/index.html):
  // the whole page goes full screen and keeps the game 16:9.
  void applyFullscreen(bool on)
  {
    EM_ASM({ if (Module.setFullscreen) Module.setFullscreen($0); }, on);
  }
  bool isFullscreen() const { return EM_ASM_INT({ return Module.isFullscreen ? Module.isFullscreen() : 0; }) != 0; }
#else
  void applyFullscreen(bool on)
  {
    SDL_SetWindowFullscreen(window, on ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    SDL_ShowCursor(on ? SDL_DISABLE : SDL_ENABLE);
  }
  bool isFullscreen() const { return (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0; }
#endif
  void toggleFullscreen()
  {
    const bool on = !isFullscreen();
    applyFullscreen(on);
    game.setFullscreen(on);
  }

  // Events, the fixed 60 Hz ticks that are due, and one picture. False
  // once the game is over.
  bool frame();
  // One SDL event: render resets, touch, keys, pads and window events.
  void handleEvent(const SDL_Event& ev);
  // A render reset (resetActionOf) answered by WindowRenderer::answerReset,
  // with the game's and the touch controls' textures (windowOwners) and,
  // when the backend does not survive a device reset, a new SDL_Renderer;
  // the run ends if none can be made.
  void answerReset(ResetAction action);
  // Once per frame before the ticks: the touch controls learn the output
  // size, the game's state, and hand over their taps and drags.
  void prepareTouch();
  // The fixed 60 Hz ticks that are due.
  void runDueTicks();

  // Milliseconds to sleep after a frame: until the next tick is due, with
  // or without vsync (see framePauseMs). The time since `last` includes the
  // frame's present, so one that blocked for the vblank shortens the sleep.
  // Only the desktop loop sleeps; a browser page is paced by
  // requestAnimationFrame.
  int pauseMs() const
  {
    const double dueSeconds = accumulator + double(SDL_GetPerformanceCounter() - last) / freq;
    return framePauseMs(tickSeconds, dueSeconds);
  }

  const CliOptions o;
  SDL_Window* window;
  WindowRenderer sdlRenderer; // before every member holding textures
  Renderer renderer;
  std::unique_ptr<Audio> audio;
  Game game;
  Controls controls;
  std::unique_ptr<TouchControls> touch;
  const double tickSeconds = 1.0 / 60.0;
  const double freq = double(SDL_GetPerformanceFrequency());
  Uint64 last = SDL_GetPerformanceCounter();
  double accumulator = 0.0;
  long frames = 0;
  bool running = true;
};

void WindowedRun::handleEvent(const SDL_Event& ev)
{
  // The GPU lost the render targets or every texture (a Direct3D device
  // reset, an Android context loss): what was lost is made again before
  // this frame draws. The SDL_Renderer survives a targets reset and, on
  // Direct3D 11 and 12, a device reset; elsewhere (OpenGL ES on Android) a
  // device reset leaves it dead and it is made again (see RenderReset).
  if (renderResetOf(ev) != RenderReset::None)
  {
    answerReset(resetActionOf(ev, renderer.backend()));
    return;
  }
  if (touch)
  {
    // Use the navigation from the last frame drawn, including a menu that
    // has just opened since the overlay was last synchronized.
    if (ev.type == SDL_FINGERDOWN)
      touch->setMenuNavigation(game.touchNavigationTargets());
    touch->handleEvent(ev, window);
  }
  // Android may close a backgrounded app without warning.
  //
  // Cloud syncs keep running here, on purpose. Going to the background is
  // no quit, and an upload still running had better reach the cloud
  // before Android may kill the process (a kill leaves the sync's temp
  // files to the next start's removeCloudLeftovers). SDL_APP_TERMINATING
  // comes from SDLActivity.onDestroy (nativeSendQuit) right behind an
  // SDL_QUIT, which ends this loop after this very frame, so ~Game cancels
  // them (cancelCloudSync, about 3 s at most) just as early. The onDestroy
  // of SDL 2.32.10 (pinned in android/build-deps.sh) waits for the SDL
  // thread with a plain Thread.join(), no time limit, so that wait is
  // never cut short.
  if (ev.type == SDL_APP_WILLENTERBACKGROUND || ev.type == SDL_APP_TERMINATING)
    game.suspend();
  if (captureBinding(game, ev) || captureText(game, ev))
    return;
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
    case Controls::Action::Pause:
      game.pauseLevel();
      break;
    case Controls::Action::None:
      break;
  }
}

void WindowedRun::answerReset(ResetAction action)
{
  if (sdlRenderer.answerReset(renderer, action, windowOwners(game, touch.get(), renderer)))
    return;
  // Nothing can be drawn any more: better to quit than to sit there black
  // (frame() draws no more once running is false). On Android, where such a
  // reset follows a return from the background, the profile was saved on
  // the way there (Game::suspend). The error is SDL's own from the failed
  // make, kept by the WindowRenderer before anything else could replace it.
  SDL_LogError(SDL_LOG_CATEGORY_RENDER, "no renderer after a device reset: %s", sdlRenderer.error().c_str());
  running = false;
}

void WindowedRun::prepareTouch()
{
  int pw = 0, ph = 0;
  SDL_GetRendererOutputSize(sdlRenderer.sdl(), &pw, &ph);
  touch->setScreen(pw, ph);
  syncTouch(*touch, game);
  forwardTouch(*touch, game);
}

void WindowedRun::runDueTicks()
{
  while (accumulator >= tickSeconds && running)
  {
#ifndef __ANDROID__
    // (On Android the on-screen letters do: a soft keyboard would cover them.)
    if (game.wantsText() && !SDL_IsTextInputActive())
      SDL_StartTextInput();
#endif
    const Input keysAndPads = controls.read(!game.inLevel(), game.wantsText());
    if (touch)
      touch->advance(SDL_GetTicks());
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
}

#ifdef __ANDROID__
// The frame rate in the log every 300 frames or 10 seconds (adb logcat).
void logFps()
{
  static Uint32 fpsStart = SDL_GetTicks(), fpsFrames = 0;
  if (++fpsFrames == 300 || SDL_GetTicks() - fpsStart > 10000)
  {
    SDL_Log("%.1f fps", fpsFrames * 1000.0 / double(SDL_GetTicks() - fpsStart));
    fpsStart = SDL_GetTicks();
    fpsFrames = 0;
  }
}
#endif

bool WindowedRun::frame()
{
#ifdef __EMSCRIPTEN__
  // The page's button (or the browser's Esc) may have changed full screen.
  game.showFullscreen(isFullscreen());
#endif
  SDL_Event ev;
  while (sdlRenderer.pollEvent(ev))
    handleEvent(ev);
  // Quit, or a device reset that left no renderer (answerReset): no ticks,
  // and above all no picture, as there may be no renderer to draw with and
  // the textures are gone.
  if (!running)
    return false;

  const Uint64 now = SDL_GetPerformanceCounter();
  accumulator += double(now - last) / freq;
  last = now;
  accumulator = std::min(accumulator, 0.25);
  if (controls.bindings() != game.bindings())
    controls.setBindings(game.bindings());
  if (touch)
    prepareTouch();
  runDueTicks();

  game.render();
  if (touch)
    touch->draw(renderer);
  SDL_RenderPresent(sdlRenderer.sdl());
#ifdef __ANDROID__
  logFps();
#endif
  return running;
}

// Runs frames until the game is over; the run is gone when this returns.
void runFrames(std::unique_ptr<WindowedRun> run)
{
#ifdef __EMSCRIPTEN__
  // The browser owns the loop: one frame per animation frame, which also
  // paces it. The run lives as long as the page, and nothing after this
  // line is reached.
  emscripten_set_main_loop_arg(
    [](void* run) {
      // The page drops its loading screen once there is a picture.
      static bool first = true;
      if (first)
        EM_ASM({ if (Module.onFirstFrame) Module.onFirstFrame(); });
      first = false;
      // A page has nothing to quit to: back to a fresh title screen.
      if (!static_cast<WindowedRun*>(run)->frame())
      {
        emscripten_cancel_main_loop();
        EM_ASM({ location.reload(); });
      }
    },
    run.release(), 0, true);
#else
  // Sleep until the next tick is due after every frame instead of drawing
  // the same picture over and over at 100% of a core. A vsynced present
  // cannot be trusted to pace the loop (a swap queue with room or a driver
  // ignoring vsync returns at once), and when it does block, that time
  // already counts towards the tick, so the sleep only gets shorter.
  while (run->frame())
  {
    const int pause = run->pauseMs();
    if (pause > 0)
      SDL_Delay(Uint32(pause));
  }
#ifndef __ANDROID__
  // Destroying the Game can wait a few seconds for cloud uploads to finish
  // or abort (Game::cancelCloudSync); the window is gone meanwhile instead of
  // sitting there frozen. (Android shows its activity until it finishes.)
  SDL_HideWindow(run->window);
#endif
  // The audio device outlives the Game (run's members go in reverse order),
  // so without this the music plays on through that wait, for a game the
  // player has already quit. Audio's own calls, as the device ID is Audio's
  // (SDL_PauseAudio would only reach the legacy device 1): music, cutscene
  // loops and the voice stop at once; a sound effect already playing ends
  // on its own.
  if (run->audio)
  {
    run->audio->playMusicNamed("stop");
    run->audio->stopAllNamed();
    run->audio->stopVoice();
  }
  run.reset();
#endif
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
#ifdef __EMSCRIPTEN__
  // A page may only go full screen from a tap or key press (the play page
  // and the FULLSCREEN menu item do it then).
  const bool startFullscreen = false;
#else
  const bool startFullscreen = !o.windowed && (o.fullscreen || Profile::load(saveDir).fullscreen);
#endif
  SDL_Window* window = SDL_CreateWindow(
    "Gunrunners",
    SDL_WINDOWPOS_CENTERED,
    SDL_WINDOWPOS_CENTERED,
    kScreenW,
    kScreenH,
    SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | (startFullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0));
  // The run makes the renderer, owns it and destroys it last of all it holds.
  runFrames(std::make_unique<WindowedRun>(o, window, startFullscreen));

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
  o.game.s3.caBundle = o.game.dataDir + "/cacert.pem";
  o.touch = true;
  o.fullscreen = true;
  loadGameFont((o.game.dataDir + "/fonts/DejaVuSans-Bold.ttf").c_str());
#endif
#ifdef __EMSCRIPTEN__
  // In a browser page: the data is the page's preloaded /data, the saves
  // are in IndexedDB, mounted at /saves by web/pre.js before main runs.
  o.game.dataDir = "/data";
  o.game.saveDir = "/saves";
  loadGameFont("/data/fonts/DejaVuSans-Bold.ttf");
  // Phones and tablets get the on-screen gamepad, as on Android.
  o.touch = o.touch || EM_ASM_INT({ return navigator.maxTouchPoints > 0 && matchMedia('(pointer: coarse)').matches; });
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
