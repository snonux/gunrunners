// Moving through a cutscene by button:
//   - a press moves on one beat: the next line of the shot, or the next
//     shot once the shot has said all of its lines;
//   - frames passed over that way fire no sounds or waits, but a line on
//     the frame landed on is said;
//   - a press on a panel waiting for a button moves on, as before;
//   - three quick presses, or one button held, skip the whole cutscene;
//     a button held since before the cutscene began skips nothing;
//   - the credits roll on through a single press.

#include "frontend/cutscene.hpp"

#include <cstdio>
#include <string>

using namespace gr;

namespace
{

int gFailures = 0;

void check(bool ok, const std::string& what)
{
  if (!ok)
  {
    ++gFailures;
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
  }
}

const char* kScript = R"(cutscene test
shot one delay=100 repeat=10
  cue 0 fade=in:400
  cue 2 say=MAX "First line."
  cue 30 say=MAX "Second line."
  cue 50 wait=button
  cue 60 say=MAX "Third line."
shot two delay=100 repeat=0
  cue 0 say=DASH "Panel line."
  cue 0 wait=button
shot three delay=100 repeat=10
  cue 5 say=NOVA "Last line."
)";

struct Driver
{
  CutscenePlayer player;
  Input prev;
  explicit Driver(const char* script) : player(Cutscene::parse(script), nullptr) {}
  bool tick(bool held)
  {
    Input in;
    in.confirm = held;
    const bool going = player.tick(in, prev);
    prev = in;
    return going;
  }
  // One press: down for a few ticks, then up for `gap` ticks.
  void press(int gap = 60)
  {
    for (int i = 0; i < 4; ++i)
      tick(true);
    for (int i = 0; i < gap && !player.done(); ++i)
      tick(false);
  }
  void idle(int ticks)
  {
    for (int i = 0; i < ticks && !player.done(); ++i)
      tick(false);
  }
};

} // namespace

int main()
{
  {
    Driver d(kScript);
    d.idle(30); // frame 2: the first line
    check(d.player.subtitle() == "First line.", "the first line shows (" + d.player.subtitle() + ")");
    d.press();
    check(!d.player.done(), "one press does not end the cutscene");
    check(d.player.subtitle() == "Second line.", "a press moves on to the next line (" + d.player.subtitle() + ")");
    d.press();
    check(!d.player.waiting(), "the wait passed over does not hold");
    check(d.player.subtitle() == "Third line.", "and on to the line after it (" + d.player.subtitle() + ")");
    d.press();
    check(d.player.subtitle() == "Panel line." && d.player.waiting(), "the last line's press moves on to the next shot");
    d.press(10);
    check(!d.player.waiting() && d.player.subtitle().empty() && !d.player.done(), "a press on a waiting panel moves on");
    d.press();
    check(d.player.subtitle() == "Last line.", "a press before a shot's line goes to it");
    d.press();
    check(d.player.done(), "a press after the last line ends the cutscene");
  }
  {
    Driver d(kScript);
    d.idle(30);
    d.press(10);
    d.press(10);
    check(!d.player.done(), "two quick presses do not skip");
    d.press(10);
    check(d.player.done(), "three quick presses skip the cutscene");
  }
  {
    Driver d(kScript);
    d.idle(30);
    d.press(40);
    d.press(40);
    d.press(40);
    check(!d.player.done() && d.player.subtitle() == "Panel line.", "three slow presses move on three beats");
  }
  {
    Driver d(kScript);
    d.idle(30);
    for (int i = 0; i < CutscenePlayer::kSkipHoldTicks + 2 && !d.player.done(); ++i)
      d.tick(true);
    check(d.player.done(), "a held button skips the cutscene");
  }
  {
    Driver d(kScript);
    d.prev.confirm = true; // held over from the level
    for (int i = 0; i < 120 && !d.player.done(); ++i)
      d.tick(true);
    check(!d.player.done(), "a button held since before the cutscene skips nothing");
  }
  {
    Driver d("cutscene credits\ncredits run_highlights\n");
    d.idle(30);
    d.press();
    check(!d.player.done(), "the credits roll on through a single press");
    d.press(5);
    d.press(5);
    d.press(5);
    check(d.player.done(), "quick presses skip the credits");
  }
  return gFailures == 0 ? 0 : 1;
}
