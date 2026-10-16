// The background renderers behind Audio::preloadMusic (src/audio/render_queue.hpp)
// and Audio::play (src/audio/sound_bank.hpp), with fake renders that can be
// held at a gate instead of real synthesis.
// Checks that:
//   - a request renders once on the worker and take() hands it over once;
//   - a repeated request (queued, rendering or finished) is not rendered again;
//   - one worker renders one thing at a time, oldest request first;
//   - markStale() empties the queue, so a level's music requested right
//     after it renders next, ahead of the last level's leftovers and of
//     its briefing's tracks, and survives a skipped briefing;
//   - take() on a request still queued withdraws it (the caller renders it),
//     and on one rendering blocks until it is done;
//   - finished results past the cap are thrown away, stale ones first, then
//     the oldest, never one a take() is blocked on;
//   - drop() cancels the queue, frees finished results and throws away the
//     render in progress, unless it is asked for again (request or take),
//     and never frees what a blocked take() is waiting for;
//   - markStale()/dropStale() free only what nobody asked for again;
//   - without threads nothing renders, take() hands back nothing;
//   - stop()/the destructor cancel the queue, wait for the render in
//     progress and let a blocked take() collect it;
//   - a render that throws leaves no result (a blocked take() gets nullptr)
//     and the worker keeps going;
//   - requestFirst() puts a new or queued id at the front (after the render
//     in progress), and adds nothing for one rendering or finished;
//   - takeReady() hands over only a finished result: it never waits for a
//     render in progress and leaves a queued request queued;
//   - demote() moves a queued id behind the rest (a cancelled music switch),
//     and leaves one rendering or finished alone;
//   - takeFailed() reports a render that threw, once; a new request or
//     drop() forgets it;
//   - PendingMusic (src/audio/pending_music.hpp), the music switch
//     Audio::playMusicNamed makes late: a track without a seek starts at
//     0, one with a seek starts that far in plus the frames played since,
//     wrapped to its length, and the last seek or want() counts;
//   - Audio's music switch itself (its test constructor: fake tracks held at
//     a gate, renders deferred as with a device, the test mixing): the last
//     request wins and an older one never starts, even when its render ends
//     first; asking for the track playing or "stop" calls off the pending
//     one, whose render then never starts; asking for the pending track
//     again keeps its seek; a seek goes to the pending track, not the one
//     filling the gap; pollMusic renews a request that markPreloadsStale,
//     dropStalePreloads or dropPreloads cancelled; a pending track whose
//     render fails is given up (not retried every tick); a cancelled switch
//     no longer delays the preloads queued behind it;
//   - Audio's playMusic maps Music to the named tracks (menu,
//     theme_synthwave, victory; None stops), asking for the one playing
//     goes on with it, and the track cache evicts the least recently used;
//   - SoundBank (src/audio/sound_bank.hpp), the sound effects Audio::play
//     plays, rendered on their own worker from openDevice on so startup
//     doesn't wait 3.5 s for them: prerender() renders in the order given,
//     each once; tryGet() never waits for another thread (the one the
//     worker renders right now comes back as nothing, a queued or idle one
//     is rendered by the caller, so an effect played before its turn still
//     plays); get() renders a queued or idle one on the caller's thread
//     (headless recording) and waits only for the worker's render of the
//     same one, rendering it itself should that throw; a rendered one keeps
//     its address; without threads both render on the spot; stop() cancels
//     the queue; a render that throws leaves nothing and is rendered again
//     when asked again; the worker's start hook (Audio lowers the thread's
//     priority there) runs once, on the worker;
//   - Audio::play puts an effect into the next mix on its first play,
//     headless (get) and with renders deferred (tryGet), and nothing plays
//     without it;
//   - Audio::sfxRenderOrder() lists every effect once, the menu's first.
//
// No check depends on a pause being long enough: a take() is known to be
// blocked once RenderQueue::waiting() counts it (a get() once
// SoundBank::waiting() does), and only then is the gate opened. waitUntil
// only bounds how long a broken queue may hang the test.

#include "audio/audio.hpp"
#include "audio/pending_music.hpp"
#include "audio/render_queue.hpp"
#include "audio/sound_bank.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <functional>
#include <memory>
#include <mutex>
#include <numeric>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace gr;

namespace
{

using Queue = RenderQueue<std::string>;
using Log = std::vector<std::string>;

int gFailures = 0;

void check(bool ok, const std::string& what)
{
  std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
  if (!ok)
    ++gFailures;
}

// Polls for up to 5 s, so a broken queue fails the test instead of hanging it.
bool waitUntil(const std::function<bool()>& done)
{
  const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (!done())
  {
    if (std::chrono::steady_clock::now() > end)
      return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return true;
}

// A render that records what it was asked for and, while the gate is
// closed, holds the worker inside it. Ids starting with "boom" throw.
// onStart, if set (before the first request), runs on the worker as a
// render starts, outside every lock.
struct FakeRender
{
  std::mutex m;
  std::condition_variable cv;
  bool open = true;
  Log started; // in order
  int active = 0;
  int maxActive = 0;
  int finished = 0;
  std::function<void(const std::string&)> onStart;

  std::unique_ptr<std::string> render(const std::string& id)
  {
    if (onStart)
      onStart(id);
    std::unique_lock<std::mutex> lock(m);
    started.push_back(id);
    maxActive = std::max(maxActive, ++active);
    cv.wait(lock, [&] { return open; });
    --active;
    ++finished;
    if (id.rfind("boom", 0) == 0)
      throw std::runtime_error("boom");
    return std::make_unique<std::string>("pcm:" + id);
  }
  Queue::Render fn()
  {
    return [this](const std::string& id) { return render(id); };
  }
  void close()
  {
    std::lock_guard<std::mutex> lock(m);
    open = false;
  }
  void release()
  {
    {
      std::lock_guard<std::mutex> lock(m);
      open = true;
    }
    cv.notify_all();
  }
  Log log()
  {
    std::lock_guard<std::mutex> lock(m);
    return started;
  }
  int count(int FakeRender::*field)
  {
    std::lock_guard<std::mutex> lock(m);
    return this->*field;
  }
  // Whether the worker got to `id` (and, with the gate closed, is held there).
  bool reached(const std::string& id)
  {
    return waitUntil([&] {
      const auto l = log();
      return std::find(l.begin(), l.end(), id) != l.end();
    });
  }
};

// Calls take(id) on its own thread, so the test can hold the render it
// waits for. blockedIn() tells when it is inside the wait.
struct Taker
{
  std::unique_ptr<std::string> got;
  std::thread thread;

  Taker(Queue& q, const std::string& id) : thread([this, &q, id] { got = q.take(id); }) {}
  // Joins, then hands over what take() returned.
  std::unique_ptr<std::string> result()
  {
    thread.join();
    return std::move(got);
  }
};

bool blockedIn(const Queue& q, const std::string& id)
{
  return waitUntil([&] { return q.waiting(id) == 1; });
}

// A queue whose owner raises a flag as its destruction starts: the body
// runs before the member's ~RenderQueue, so once the flag is up the queue
// is (about to be) destroyed, which another thread can watch safely.
struct Owned
{
  std::atomic<bool>& dying;
  Queue q;

  Owned(std::atomic<bool>& flag, FakeRender& fake) : dying(flag), q(fake.fn(), 8) {}
  ~Owned() { dying = true; }
};

void testRequestAndTake()
{
  FakeRender fake;
  Queue q(fake.fn(), 4);
  check(q.request("a"), "a new id is queued");
  check(waitUntil([&] { return q.ready("a"); }), "the worker renders it");
  check(!q.request("a"), "a finished id is not queued again");
  const auto out = q.take("a");
  check(out && *out == "pcm:a", "take() hands the result over");
  check(!q.take("a"), "only once");
  check(q.readyCount() == 0, "nothing is left behind");
  check(fake.log() == Log{"a"}, "it rendered exactly once");
}

void testOneWorkerInOrder()
{
  FakeRender fake;
  fake.close();
  Queue q(fake.fn(), 8);
  q.request("a");
  check(fake.reached("a"), "the worker starts on the first request");
  q.request("b");
  q.request("c");
  check(!q.request("a"), "an id rendering now is not queued again");
  check(!q.request("b"), "a queued id is not queued twice");
  check(q.queuedCount() == 2, "the rest wait in the queue");
  fake.release();
  check(waitUntil([&] { return q.readyCount() == 3; }), "all three finish");
  check(fake.log() == Log{"a", "b", "c"}, "oldest request first, each once");
  check(fake.count(&FakeRender::maxActive) == 1, "one render at a time");
}

// What Game::beginCampaignLevel and playBriefing do, the queue's part of
// it: the level start's markStale() cancels the last level's queued
// preloads, so the level's music, asked for before the scenes' tracks, is
// the next to render. That it is asked for first is the caller's doing
// (playBriefing), not the queue's: requests render in order. The opening's
// track is taken at once, and the briefing is skipped before any cue plays
// (startLevel: markStale, take, dropStale).
void testLevelTrackFirst()
{
  FakeRender fake;
  // Holds the worker again as the level's track starts, so the test can
  // see what it rendered right after the last level's.
  fake.onStart = [&fake](const std::string& id) {
    if (id == "level")
      fake.close();
  };
  fake.close();
  Queue q(fake.fn(), 8);
  q.request("old");
  check(fake.reached("old"), "the worker is held on the last level's preload");
  q.request("oldvariant");
  q.markStale();
  check(q.queuedCount() == 0, "the level start cancels the last level's queue");
  q.request("level");
  q.request("opening");
  q.request("cue");
  check(!q.take("opening"), "the opening's track, still queued, is rendered by the caller");
  fake.release();
  check(fake.reached("level"), "the worker gets to the level's track");
  check(fake.log() == Log{"old", "level"}, "right after the render in progress, before anything else");
  q.markStale();
  Taker taker(q, "level");
  check(blockedIn(q, "level"), "the skip's level start waits for its track in progress");
  fake.release();
  const auto level = taker.result();
  check(level && *level == "pcm:level", "and gets it, not cancelled by markStale()");
  q.dropStale();
  check(q.queuedCount() == 0 && q.readyCount() == 0, "the skipped cue was cancelled, the last level's track freed");
  check(fake.log() == Log{"old", "level"}, "neither the cancelled variant nor the cue rendered");
}

void testTakeQueuedAndRendering()
{
  FakeRender fake;
  fake.close();
  Queue q(fake.fn(), 8);
  q.request("a");
  check(fake.reached("a"), "the worker is held on a");
  q.request("b");
  check(!q.take("b"), "take() of a queued id hands back nothing (the caller renders it)");
  check(q.queuedCount() == 0, "and withdraws the request");
  Taker taker(q, "a");
  check(blockedIn(q, "a"), "take() of the id rendering blocks on it");
  fake.release();
  const auto a = taker.result();
  check(a && *a == "pcm:a", "and hands it over once it is done");
  check(q.waiting("a") == 0, "then no longer counts as waiting");
  q.request("z");
  check(waitUntil([&] { return q.ready("z"); }), "the worker keeps going");
  check(fake.log() == Log{"a", "z"}, "the withdrawn request never rendered");
}

void testCap()
{
  FakeRender fake;
  Queue q(fake.fn(), 2);
  for (const char* id : {"a", "b", "c"})
    q.request(id);
  check(waitUntil([&] { return q.ready("c"); }), "every request finishes");
  check(q.readyCount() == 2, "finished results stop at the cap");
  check(!q.ready("a") && q.ready("b") && q.ready("c"), "the oldest went first");
  check(!q.take("a"), "an evicted id hands back nothing (the caller renders it)");
  check(q.request("a"), "and can be requested again");
  check(waitUntil([&] { return q.ready("a"); }), "which renders it again");
  check(q.readyCount() == 2 && !q.ready("b"), "evicting the next oldest");
  check(!q.request("c"), "asking for a finished id again (c, a -> a, c)");
  q.request("d");
  check(waitUntil([&] { return q.ready("d"); }), "the next one finishes");
  check(q.ready("c") && !q.ready("a"), "and evicts the oldest asked for, not the oldest rendered");
}

void testCapEvictsStaleFirst()
{
  FakeRender fake;
  Queue q(fake.fn(), 2);
  q.request("fresh");
  check(waitUntil([&] { return q.ready("fresh"); }), "a finished preload");
  fake.close();
  q.request("stale");
  check(fake.reached("stale"), "the worker is held on another");
  q.markStale();
  check(!q.request("fresh"), "the older one is asked for again");
  fake.release();
  check(waitUntil([&] { return q.ready("stale"); }), "the stale render ends and is kept for now");
  q.request("new");
  check(waitUntil([&] { return q.ready("new"); }), "a third one goes past the cap");
  check(q.ready("fresh") && !q.ready("stale"), "a stale result goes before an older fresh one");
}

void testCapKeepsWaitedResult()
{
  FakeRender fake;
  Queue q(fake.fn(), 1);
  q.request("x");
  check(waitUntil([&] { return q.ready("x"); }), "a finished result fills the cap");
  fake.close();
  q.request("a");
  check(fake.reached("a"), "the worker is held on a");
  Taker taker(q, "a");
  check(blockedIn(q, "a"), "a take() blocks on a");
  q.request("b");
  q.request("c");
  fake.release();
  // b and c may finish before the taker has the lock back: then the cap
  // must evict them, not a.
  const auto a = taker.result();
  check(a && *a == "pcm:a", "the cap never evicts the result a take() waits for");
  check(!q.ready("x"), "the unclaimed result made room for it");
  check(q.readyCount() <= 1, "the cap holds again once it is taken");
}

void testDrop()
{
  FakeRender fake;
  Queue q(fake.fn(), 8);
  q.request("old");
  check(waitUntil([&] { return q.ready("old"); }), "a finished preload");
  fake.close();
  q.request("a");
  check(fake.reached("a"), "the worker is held on a");
  q.request("b");
  q.drop();
  check(q.readyCount() == 0 && !q.ready("old"), "drop() frees finished results");
  check(q.queuedCount() == 0, "and cancels the queue");
  fake.release();
  q.request("z");
  check(waitUntil([&] { return q.ready("z"); }), "the worker keeps going");
  check(!q.ready("a"), "the render that was in progress is thrown away");
  check(fake.log() == Log{"old", "a", "z"}, "the cancelled request never rendered");
}

void testDropThenAskAgain()
{
  FakeRender fake;
  fake.close();
  Queue q(fake.fn(), 8);
  q.request("a");
  check(fake.reached("a"), "the worker is held on a");
  q.drop();
  check(!q.request("a"), "a request after drop() for the id rendering adds nothing to the queue");
  fake.release();
  check(waitUntil([&] { return q.ready("a"); }), "but keeps its result");

  fake.close();
  q.request("b");
  check(fake.reached("b"), "the worker is held on b");
  q.drop();
  Taker taker(q, "b");
  check(blockedIn(q, "b"), "take() after drop() blocks on the render in progress");
  fake.release();
  const auto b = taker.result();
  check(b && *b == "pcm:b", "and still gets it");
  check(fake.log() == Log{"a", "b"}, "neither rendered twice");
}

// drop() and dropStale() while a take() is blocked: before the render
// ends, and in the window after the worker stored it but before the
// take() has the lock back (the next render drops everything as it starts).
void testDropWhileTaking()
{
  FakeRender fake;
  fake.close();
  Queue q(fake.fn(), 8);
  q.request("a");
  check(fake.reached("a"), "the worker is held on a");
  Taker taker(q, "a");
  check(blockedIn(q, "a"), "a take() blocks on a");
  q.drop();
  q.markStale();
  q.dropStale();
  fake.release();
  const auto a = taker.result();
  check(a && *a == "pcm:a", "drop() and dropStale() leave a blocked take() its render");

  FakeRender fake2;
  Queue q2(fake2.fn(), 8);
  fake2.onStart = [&](const std::string& id) {
    if (id == "b")
      q2.drop();
  };
  fake2.close();
  q2.request("a");
  check(fake2.reached("a"), "the worker is held on a again");
  Taker taker2(q2, "a");
  check(blockedIn(q2, "a"), "a take() blocks on it");
  q2.request("b");
  fake2.release();
  const auto a2 = taker2.result();
  check(a2 && *a2 == "pcm:a", "a drop() right after the render was stored keeps it for the take()");
}

void testStale()
{
  FakeRender fake;
  Queue q(fake.fn(), 8);
  q.request("shared");
  q.request("old");
  check(waitUntil([&] { return q.ready("shared") && q.ready("old"); }), "two finished preloads");
  fake.close();
  q.request("a");
  check(fake.reached("a"), "the worker is held on a");
  q.request("queued");
  q.markStale();
  check(q.queuedCount() == 0, "markStale() cancels the queue");
  check(q.ready("shared") && q.ready("old"), "but frees nothing yet");
  check(!q.request("shared"), "a stale result asked for again is not rendered again");
  check(!q.request("a"), "nor is a stale render in progress");
  q.dropStale();
  check(q.ready("shared") && !q.ready("old"), "dropStale() frees only what nobody asked for again");
  fake.release();
  check(waitUntil([&] { return q.ready("a"); }), "the render asked for again is kept");
  check(fake.log() == Log{"shared", "old", "a"}, "nothing rendered twice, the cancelled request never");

  fake.close();
  q.request("b");
  check(fake.reached("b"), "the worker is held on b");
  q.markStale();
  fake.release();
  check(waitUntil([&] { return q.ready("b"); }), "a stale render that ends before dropStale() is kept for now");
  q.dropStale();
  check(q.readyCount() == 0, "and freed by it, with every other stale result");

  fake.close();
  q.request("c");
  check(fake.reached("c"), "the worker is held on c");
  q.markStale();
  q.dropStale();
  fake.release();
  q.request("z");
  check(waitUntil([&] { return q.ready("z"); }), "the worker keeps going");
  check(!q.ready("c"), "a render still stale at dropStale() is thrown away when it ends");
}

void testWithoutThreads()
{
  FakeRender fake;
  Queue q(fake.fn(), 4, false);
  check(q.request("a"), "a request is remembered");
  check(!q.request("a"), "once");
  check(q.queuedCount() == 1, "and waits in the queue");
  check(!q.take("a"), "take() hands back nothing: the caller renders it");
  check(q.queuedCount() == 0, "and withdraws it");
  q.request("b");
  q.drop();
  check(q.queuedCount() == 0, "drop() empties the queue");
  q.request("c");
  q.markStale();
  q.dropStale();
  check(q.queuedCount() == 0, "so does markStale()");
  check(fake.log().empty(), "nothing ever rendered in the background");
}

void testDestructor()
{
  FakeRender fake;
  fake.close();
  std::atomic<bool> dying{false};
  auto owned = std::make_unique<Owned>(dying, fake);
  Queue& q = owned->q;
  q.request("a");
  check(fake.reached("a"), "the worker is held on a");
  q.request("b");
  q.request("c");
  q.stop(); // what ~RenderQueue does first, observable here
  check(q.queuedCount() == 0, "stopping cancels the queue");
  check(q.request("d") && !q.take("d"), "later requests are only remembered (the caller renders them)");
  std::thread killer([&] { owned.reset(); });
  check(waitUntil([&] { return dying.load(); }), "the destruction starts");
  fake.release();
  killer.join();
  check(fake.log() == Log{"a"}, "nothing queued rendered");
  check(fake.count(&FakeRender::finished) == 1 && fake.count(&FakeRender::active) == 0,
    "the destructor waited for the render in progress");
}

void testDestructorWhileTaking()
{
  FakeRender fake;
  fake.close();
  std::atomic<bool> dying{false};
  auto owned = std::make_unique<Owned>(dying, fake);
  owned->q.request("a");
  check(fake.reached("a"), "the worker is held on a");
  Taker taker(owned->q, "a");
  check(blockedIn(owned->q, "a"), "a take() blocks on a");
  // The gate opens only once ~Owned has begun, so the render ends while
  // ~RenderQueue stops or joins the worker. Whether the taker is still
  // listed as waiting when the destructor gets to its final wait is up to
  // the scheduler: often it has collected its render and left by then. So
  // this proves the take() gets its result and nothing hangs; a destructor
  // that forgot to wait fails only when the race goes the other way, which
  // the ASan/TSan builds report (a use-after-free or a race on the freed
  // queue) and a plain build may not notice. A deterministic version would
  // need test-only hooks inside both take() and the destructor.
  std::thread killer([&] { owned.reset(); });
  check(waitUntil([&] { return dying.load(); }), "the destruction starts");
  fake.release();
  killer.join();
  const auto a = taker.result();
  check(a && *a == "pcm:a", "the destructor lets a blocked take() collect the render in progress");
}

void testThrowingRender()
{
  FakeRender fake;
  Queue q(fake.fn(), 4);
  q.request("boom");
  q.request("ok");
  check(waitUntil([&] { return q.ready("ok"); }), "the worker survives a render that throws");
  check(!q.ready("boom") && !q.take("boom"), "which leaves no result");

  fake.close();
  q.request("boom2");
  check(fake.reached("boom2"), "the worker is held on a render that will throw");
  Taker taker(q, "boom2");
  check(blockedIn(q, "boom2"), "a take() blocks on it");
  fake.release();
  check(!taker.result(), "and gets nothing back when it throws (the caller renders it)");
}

// What Audio::playMusicNamed and pollMusic do when a level's layer is
// needed while the worker is busy with the level's other preloads.
void testRequestFirst()
{
  FakeRender fake;
  fake.close();
  Queue q(fake.fn(), 8);
  q.request("a");
  check(fake.reached("a"), "the worker is held on a");
  q.request("b");
  q.request("c");
  check(!q.requestFirst("c"), "a queued id asked for first is not queued twice");
  check(q.queuedCount() == 2, "but moved, not added");
  check(q.requestFirst("d"), "a new id asked for first is queued");
  check(!q.requestFirst("a"), "the id rendering is not queued again");
  fake.release();
  check(waitUntil([&] { return q.readyCount() == 4; }), "everything finishes");
  check(fake.log() == Log{"a", "d", "c", "b"}, "after the render in progress, the last asked for first comes first");
  check(!q.requestFirst("c"), "a finished id is not rendered again");
  check(fake.log().size() == 4, "nothing rendered twice");

  FakeRender fake2;
  Queue q2(fake2.fn(), 4, false);
  check(q2.requestFirst("x") && q2.queuedCount() == 1, "without threads it is only remembered");
  check(!q2.takeReady("x"), "and never ready");
}

void testTakeReady()
{
  FakeRender fake;
  fake.close();
  Queue q(fake.fn(), 8);
  q.request("a");
  check(fake.reached("a"), "the worker is held on a");
  q.request("b");
  // Run on this thread: a takeReady() that waited would hang the test here.
  check(!q.takeReady("a"), "the id rendering hands back nothing, at once");
  check(q.waiting("a") == 0, "without waiting for it");
  check(!q.takeReady("b"), "a queued id hands back nothing");
  check(q.queuedCount() == 1, "and stays queued (unlike take())");
  check(!q.takeReady("never"), "nor does an id never asked for");
  fake.release();
  check(waitUntil([&] { return q.ready("a") && q.ready("b"); }), "both finish");
  const auto a = q.takeReady("a");
  check(a && *a == "pcm:a", "a finished result is handed over");
  check(!q.takeReady("a") && !q.ready("a"), "once");
  check(fake.log() == Log{"a", "b"}, "each rendered once");
}

// What Audio::cancelPending does when a music switch is called off.
void testDemote()
{
  FakeRender fake;
  fake.close();
  Queue q(fake.fn(), 8);
  q.request("a");
  check(fake.reached("a"), "the worker is held on a");
  q.requestFirst("p");
  q.request("c1");
  q.request("c2");
  check(q.demote("p"), "a queued id is demoted");
  check(q.queuedCount() == 3, "moved, not withdrawn");
  check(!q.demote("a"), "the id rendering is left alone");
  check(!q.demote("never"), "an id never asked for is nothing to demote");
  fake.release();
  check(waitUntil([&] { return q.readyCount() == 4; }), "everything finishes");
  check(fake.log() == Log{"a", "c1", "c2", "p"}, "the demoted id came after the requests that were behind it");
  check(!q.demote("p") && q.ready("p"), "a finished id is left alone");
}

// What Audio::pollMusic asks before it asks for a pending track again.
void testTakeFailed()
{
  FakeRender fake;
  Queue q(fake.fn(), 8);
  check(!q.takeFailed("boom"), "nothing failed yet");
  q.request("boom");
  q.request("ok");
  check(waitUntil([&] { return q.ready("ok"); }), "the worker goes on after a render that throws");
  check(!q.takeFailed("ok"), "a render that worked is no failure");
  check(q.takeFailed("boom"), "the one that threw is reported");
  check(!q.takeFailed("boom"), "once");

  q.request("boom2");
  q.request("ok2");
  check(waitUntil([&] { return q.ready("ok2"); }), "another render throws");
  fake.close();
  q.request("gate");
  check(fake.reached("gate"), "the worker is held on another id");
  q.request("boom2");
  check(!q.takeFailed("boom2"), "a new request forgets the earlier failure");
  fake.release();
  q.request("ok3");
  check(waitUntil([&] { return q.ready("ok3"); }), "the new attempt ends");
  check(q.takeFailed("boom2"), "and its failure is reported again");

  q.request("boom3");
  q.request("ok4");
  check(waitUntil([&] { return q.ready("ok4"); }), "a third render throws");
  q.drop();
  check(!q.takeFailed("boom3"), "drop() forgets failures");
}

void testPendingMusic()
{
  PendingMusic p;
  check(!p.active() && p.id().empty(), "nothing pending at first");
  p.want("layer@2");
  check(p.active() && p.id() == "layer@2", "a track not rendered yet is pending");
  check(p.startFrame(5000, 1000) == 0, "without a seek it starts at its start, however late");
  p.seek(300, 1000);
  check(p.startFrame(1000, 10000) == 300, "arriving at once it starts where the seek put it");
  check(p.startFrame(1450, 10000) == 750, "arriving late it starts as far on as the frames played since");
  check(p.startFrame(1000 + 9800, 10000) == 100, "wrapped to the track's length, as seekMusic wraps it");
  check(p.startFrame(900, 10000) == 300, "a count from before the seek never runs it backwards");
  check(p.startFrame(2000, 0) == 0, "an empty track starts at 0");
  p.want("layer@2");
  check(p.startFrame(1000, 10000) == 300, "asking for the same track again keeps its seek");
  p.seek(0, 4000);
  check(p.startFrame(4100, 10000) == 100, "the last seek counts");
  p.want("other");
  check(p.id() == "other" && p.startFrame(9000, 10000) == 0, "another track drops the old one's seek");
  p.cancel();
  check(!p.active() && p.id().empty(), "cancel() leaves nothing pending");
  p.want("other");
  check(p.startFrame(9000, 10000) == 0, "nor a seek from before it");
}

// Stands in for the synthesizer behind Audio's tracks (Audio's test
// constructor): every track is `frames` long and not silent, renders of the
// ids held wait at the gate until released, and ids starting with "boom"
// throw. It must outlive the Audio, with nothing held by then (the Audio's
// destructor waits for the render in progress).
struct FakeTracks
{
  std::mutex m;
  std::condition_variable cv;
  Log held;
  Log started; // in order
  std::size_t frames = std::size_t(4 * kAudioRate);

  std::unique_ptr<Audio::Track> render(const std::string& id)
  {
    std::unique_lock<std::mutex> lock(m);
    started.push_back(id);
    cv.wait(lock, [&] { return std::find(held.begin(), held.end(), id) == held.end(); });
    if (id.rfind("boom", 0) == 0)
      throw std::runtime_error("boom");
    auto track = std::make_unique<Audio::Track>();
    track->left.assign(frames, 0.25f);
    track->right.assign(frames, 0.25f);
    return track;
  }
  Audio::TrackRender fn()
  {
    return [this](const std::string& id) { return render(id); };
  }
  void hold(const std::string& id)
  {
    std::lock_guard<std::mutex> lock(m);
    held.push_back(id);
  }
  void release(const std::string& id)
  {
    {
      std::lock_guard<std::mutex> lock(m);
      held.erase(std::remove(held.begin(), held.end(), id), held.end());
    }
    cv.notify_all();
  }
  Log log()
  {
    std::lock_guard<std::mutex> lock(m);
    return started;
  }
  long count(const std::string& id)
  {
    const Log l = log();
    return long(std::count(l.begin(), l.end(), id));
  }
  // Whether a render of `id` started (and, held, waits at the gate).
  bool reached(const std::string& id)
  {
    return waitUntil([&] { return count(id) > 0; });
  }
};

// Mixes `frames` frames, as the device's callback would.
void mixFrames(Audio& a, int frames)
{
  std::vector<float> buf(std::size_t(frames) * 2);
  a.mix(buf.data(), frames);
}

// Ticks (pollMusic, as Game::tick does) until `id` plays.
bool playsSoon(Audio& a, const std::string& id)
{
  return waitUntil([&] {
    a.pollMusic();
    return a.musicPlaying() == id;
  });
}

// Waits until the music worker is past everything asked for so far: one
// worker renders in order, so a preload asked for now starting means every
// render ahead of it has ended and its result is stored. Each call adds a
// finished preload, so a test calls it only a few times (the cap is 8).
bool settle(Audio& a, FakeTracks& fake)
{
  static int n = 0;
  const std::string id = "settle" + std::to_string(++n);
  a.preloadMusic(id);
  return fake.reached(id);
}

void testAudioLastRequestWins()
{
  FakeTracks fake;
  fake.hold("a");
  fake.hold("b");
  Audio a(fake.fn(), true);
  a.playMusicNamed("a");
  check(fake.reached("a"), "a switch to a track not rendered yet asks for its render");
  check(a.musicPending() == "a" && a.musicPlaying() == "stop", "and waits for it while the silence goes on");
  a.playMusicNamed("b");
  check(a.musicPending() == "b", "a newer request replaces it");
  fake.release("a");
  check(fake.reached("b"), "the older render ends (and is stored) before the newer one starts");
  for (int i = 0; i < 3; ++i)
    a.pollMusic();
  check(a.musicPlaying() == "stop" && a.musicPending() == "b", "the older track never starts");
  fake.release("b");
  check(playsSoon(a, "b"), "the last one asked for starts once rendered");
  check(a.musicPending().empty(), "and nothing is pending any more");
  check(fake.count("a") == 1 && fake.count("b") == 1, "each rendered once");
}

void testAudioCancelPending()
{
  FakeTracks fake;
  Audio a(fake.fn(), true);
  a.playMusicNamed("a");
  check(playsSoon(a, "a"), "a track starts once rendered");
  fake.hold("b");
  a.playMusicNamed("b");
  check(fake.reached("b") && a.musicPending() == "b", "another one is pending");
  a.playMusicNamed("a");
  check(a.musicPending().empty() && a.musicPlaying() == "a", "asking for the track playing calls it off");
  fake.release("b");
  check(settle(a, fake), "its render ends");
  a.pollMusic();
  check(a.musicPlaying() == "a", "and does not start");

  fake.hold("c");
  a.playMusicNamed("c");
  check(fake.reached("c") && a.musicPending() == "c", "a third one is pending");
  a.playMusicNamed("stop");
  check(a.musicPending().empty() && a.musicPlaying() == "stop", "stop calls it off and silences the music");
  fake.release("c");
  check(settle(a, fake), "its render ends");
  a.pollMusic();
  check(a.musicPlaying() == "stop", "stop, then the render becomes ready: it does not start");
}

void testAudioSeek()
{
  FakeTracks fake;
  fake.hold("a");
  Audio a(fake.fn(), true);
  a.playMusicNamed("a");
  check(fake.reached("a"), "a is pending");
  a.seekMusic(1.0);
  mixFrames(a, 100);
  a.playMusicNamed("a");
  check(a.musicPending() == "a", "asking for the pending track again keeps it pending");
  fake.release("a");
  check(settle(a, fake), "its render ends");
  a.pollMusic();
  check(a.musicPlaying() == "a" && a.musicFrame() == std::size_t(kAudioRate) + 100,
    "and keeps its seek: it starts there plus the frames mixed since");

  mixFrames(a, 500);
  const std::size_t at = a.musicFrame();
  fake.hold("b");
  a.playMusicNamed("b");
  check(fake.reached("b"), "b is pending while a plays");
  a.seekMusic(2.0);
  check(a.musicFrame() == at, "a seek meanwhile leaves the track playing alone");
  mixFrames(a, 300);
  check(a.musicFrame() == at + 300, "which plays on");
  fake.release("b");
  check(settle(a, fake), "b's render ends");
  a.pollMusic();
  check(a.musicPlaying() == "b" && a.musicFrame() == std::size_t(2 * kAudioRate) + 300,
    "the seek went to the pending track");
}

// A level start's markPreloadsStale (or the title's dropPreloads) while a
// switch waits: without pollMusic asking again, the track would never come.
void testAudioPollRenews()
{
  FakeTracks fake;
  fake.hold("x");
  Audio a(fake.fn(), true);
  a.preloadMusic("x");
  check(fake.reached("x"), "the worker is held on a preload");
  a.playMusicNamed("b");
  a.markPreloadsStale();
  a.playMusicNamed("c");
  a.dropPreloads();
  check(a.musicPending() == "c", "the switch still waits after its request was cancelled");
  fake.release("x");
  check(playsSoon(a, "c"), "pollMusic asks for it again and it starts");
  check(fake.count("b") == 0, "the track asked for before it never rendered");

  fake.hold("d");
  a.playMusicNamed("d");
  check(fake.reached("d"), "another switch waits for its render in progress");
  a.markPreloadsStale();
  a.dropStalePreloads();
  fake.release("d");
  check(playsSoon(a, "d"), "a render dropped as stale is asked for again and starts");
  check(fake.count("d") <= 2, "rendered at most once more");
}

void testAudioFailedRender()
{
  FakeTracks fake;
  Audio a(fake.fn(), true);
  a.playMusicNamed("a");
  check(playsSoon(a, "a"), "a track plays");
  fake.hold("boom");
  a.playMusicNamed("boom");
  check(fake.reached("boom"), "a switch waits for a render that will fail");
  a.pollMusic();
  check(a.musicPending() == "boom", "still pending while it renders");
  fake.release("boom");
  check(settle(a, fake), "the render fails");
  a.pollMusic();
  check(a.musicPending().empty(), "the next tick gives it up");
  check(a.musicPlaying() == "a", "and the music playing goes on");
  for (int i = 0; i < 5; ++i)
    a.pollMusic();
  check(settle(a, fake), "the worker is idle again");
  check(fake.count("boom") == 1, "the failed render is not retried tick after tick");
  a.playMusicNamed("boom");
  check(a.musicPending() == "boom", "asked for again, it is tried again");
  check(settle(a, fake), "and fails again");
  a.pollMusic();
  check(a.musicPending().empty() && fake.count("boom") == 2, "once");
}

// A switch called off while its request still waits in the queue: the
// preloads asked for before it (a cutscene's cues) render first again.
void testAudioCancelDemotes()
{
  FakeTracks fake;
  fake.hold("x");
  Audio a(fake.fn(), true);
  a.preloadMusic("x");
  check(fake.reached("x"), "the worker is held on a preload");
  a.preloadMusic("cue1");
  a.preloadMusic("cue2");
  a.playMusicNamed("p");
  a.playMusicNamed("stop");
  fake.release("x");
  check(settle(a, fake), "everything asked for renders");
  const Log l = fake.log();
  check(l.size() >= 4 && Log(l.begin(), l.begin() + 4) == Log({"x", "cue1", "cue2", "p"}),
    "the called-off track went behind the cues, still queued");
}

// The built-in tracks go by the named tracks' ids, cache and all.
void testAudioPlayMusic()
{
  FakeTracks fake;
  Audio a(fake.fn(), true);
  a.playMusic(Music::Menu);
  check(playsSoon(a, "menu"), "Music::Menu plays the named track menu");
  mixFrames(a, 100);
  a.playMusic(Music::Menu);
  check(a.musicPlaying() == "menu" && a.musicFrame() == 100 && a.musicPending().empty(),
    "asked for again, it goes on, not restarted");
  a.playMusic(Music::Level);
  check(playsSoon(a, "theme_synthwave"), "Music::Level plays theme_synthwave");
  mixFrames(a, 50);
  a.playMusicNamed("theme_synthwave");
  a.playMusic(Music::Level);
  check(a.musicFrame() == 50, "one track whichever way it is asked for: it goes on");
  a.playMusic(Music::Victory);
  check(playsSoon(a, "victory"), "Music::Victory plays victory");
  a.playMusic(Music::None);
  check(a.musicPlaying() == "stop" && a.musicPending().empty(), "Music::None stops the music");
  a.playMusic(Music::Menu);
  check(a.musicPlaying() == "menu", "a cached track starts at once");
  check(fake.count("menu") == 1 && fake.count("theme_synthwave") == 1 && fake.count("victory") == 1,
    "each rendered once");
}

// Audio keeps the 10 tracks used last: the menu's, asked for again and
// again, outlives tracks played once after it was first rendered.
void testAudioTrackCache()
{
  FakeTracks fake;
  fake.frames = 1000;
  Audio a(fake.fn(), true);
  a.playMusicNamed("menu");
  bool played = playsSoon(a, "menu");
  for (int k = 1; k <= 9; ++k)
  {
    a.playMusicNamed("t" + std::to_string(k));
    played = playsSoon(a, "t" + std::to_string(k)) && played;
  }
  check(played, "ten tracks fill the cache");
  a.playMusicNamed("menu");
  check(a.musicPlaying() == "menu", "the menu's, cached, plays again at once");
  a.playMusicNamed("stop");
  a.playMusicNamed("t10");
  check(playsSoon(a, "t10"), "an eleventh track goes past the cap");
  a.playMusicNamed("menu");
  check(a.musicPlaying() == "menu" && fake.count("menu") == 1, "the menu's, used last but one, is still cached");
  a.playMusicNamed("t1");
  check(a.musicPending() == "t1", "the least recently used track went instead");
  check(playsSoon(a, "t1") && fake.count("t1") == 2, "and is rendered again");
}

// What a headless recording does (no device: Audio::play renders the
// effect on its first play with the real synthesizer, a few ms), and what a
// window does before the effects' worker got to it (tryGet renders it).
void testAudioPlaySfx()
{
  const int frames = 4096;
  const auto loud = [](const std::vector<float>& buf) {
    return std::any_of(buf.begin(), buf.end(), [](float v) { return v != 0.0f; });
  };
  std::vector<float> buf(std::size_t(frames) * 2);
  Audio quiet;
  quiet.mix(buf.data(), frames);
  check(!loud(buf), "nothing played, the mix is silent");
  Audio headless;
  headless.play(Sfx::Jump);
  headless.mix(buf.data(), frames);
  check(loud(buf), "headless, an effect's first play is in the next mix");
  FakeTracks fake;
  Audio deferred(fake.fn(), true);
  deferred.play(Sfx::Jump);
  deferred.mix(buf.data(), frames);
  check(loud(buf), "with renders deferred too: an effect played before its turn is not dropped");
}

// The sound effects' bank (src/audio/sound_bank.hpp, behind Audio::play),
// with a fake render: renders of `hold` wait at the gate while it is
// closed, every other one passes. It records which thread rendered what,
// so a test tells the worker's renders from the caller's.
using Bank = SoundBank<std::string>;
using Order = std::vector<std::size_t>;

struct FakeBank
{
  static constexpr std::size_t kNone = std::size_t(-1);
  std::mutex m;
  std::condition_variable cv;
  bool open = true;
  std::size_t hold = kNone;
  std::size_t boom = kNone; // its next render throws (once)
  std::vector<std::pair<std::size_t, std::thread::id>> started; // in order

  std::string render(std::size_t i)
  {
    std::unique_lock<std::mutex> lock(m);
    started.emplace_back(i, std::this_thread::get_id());
    if (i == hold)
      cv.wait(lock, [&] { return open; });
    if (i == boom)
    {
      boom = kNone;
      throw std::runtime_error("boom");
    }
    return "pcm" + std::to_string(i);
  }
  Bank::Render fn()
  {
    return [this](std::size_t i) { return render(i); };
  }
  // Holds the renders of `i` at the gate (set before the first request).
  void holdAt(std::size_t i)
  {
    std::lock_guard<std::mutex> lock(m);
    hold = i;
    open = false;
  }
  void release()
  {
    {
      std::lock_guard<std::mutex> lock(m);
      open = true;
    }
    cv.notify_all();
  }
  Order log()
  {
    std::lock_guard<std::mutex> lock(m);
    Order out;
    for (const auto& s : started)
      out.push_back(s.first);
    return out;
  }
  // The thread that rendered `i` first (a default id if none did).
  std::thread::id by(std::size_t i)
  {
    std::lock_guard<std::mutex> lock(m);
    for (const auto& s : started)
      if (s.first == i)
        return s.second;
    return std::thread::id();
  }
  // The thread that rendered `i` last (a default id if none did).
  std::thread::id byLast(std::size_t i)
  {
    std::lock_guard<std::mutex> lock(m);
    for (auto s = started.rbegin(); s != started.rend(); ++s)
      if (s->first == i)
        return s->second;
    return std::thread::id();
  }
  bool reached(std::size_t i)
  {
    return waitUntil([&] {
      const auto l = log();
      return std::find(l.begin(), l.end(), i) != l.end();
    });
  }
};

void testBankOrder()
{
  FakeBank fake;
  fake.holdAt(3);
  Bank bank(fake.fn(), 6);
  check(bank.queued().empty() && bank.readyCount() == 0, "a new bank renders nothing");
  bank.prerender({3, 1});
  check(fake.reached(3), "the worker is held on the first one asked for");
  bank.prerender({0, 1, 2, 3, 4, 5, 9});
  check(bank.queued() == Order({1, 0, 2, 4, 5}),
    "the rest queue behind, in order, nothing twice, nothing past the end");
  fake.release();
  check(waitUntil([&] { return bank.readyCount() == 6; }), "the worker renders them all");
  check(fake.log() == Order({3, 1, 0, 2, 4, 5}), "in the order asked for, each once");
  const std::string* two = bank.get(2);
  check(two && *two == "pcm2" && two == bank.tryGet(2), "a rendered item stays where it was handed out");
  check(fake.log().size() == 6, "get() of a rendered one renders nothing");
}

// What Audio::play does in a window: a sound played before its turn (a
// level's warning siren, right after launch) still plays.
void testBankTryGetNeverWaits()
{
  FakeBank fake;
  fake.holdAt(0);
  Bank bank(fake.fn(), 5);
  bank.prerender({0, 1, 2, 3, 4});
  check(fake.reached(0), "the worker is held on 0");
  // Run on this thread: a tryGet() that waited would hang the test here.
  check(!bank.tryGet(0), "the one the worker renders right now hands back nothing, at once");
  check(bank.waiting(0) == 0, "without waiting for it");
  check(bank.queued() == Order({1, 2, 3, 4}), "nor is it queued again");
  const std::string* three = bank.tryGet(3);
  check(three && *three == "pcm3", "a queued one is rendered on the spot, so its first play is not lost");
  check(fake.by(3) == std::this_thread::get_id(), "by the caller, not the worker");
  check(bank.queued() == Order({1, 2, 4}), "and its request withdrawn");
  check(bank.tryGet(3) == three, "asked again, it is handed over, not rendered again");
  check(!bank.tryGet(7), "a number past the end hands back nothing");
  fake.release();
  check(waitUntil([&] { return bank.readyCount() == 5; }), "everything finishes");
  check(fake.log() == Order({0, 3, 1, 2, 4}), "each rendered once");
}

// What Audio::play does headless (and in the browser without threads).
void testBankGet()
{
  FakeBank fake;
  fake.holdAt(0);
  Bank bank(fake.fn(), 5);
  bank.prerender({0, 1, 2, 3, 4});
  check(fake.reached(0), "the worker is held on 0");
  const std::string* three = bank.get(3);
  check(three && *three == "pcm3", "get() of a queued one hands it over");
  check(fake.by(3) == std::this_thread::get_id(), "rendered by the caller, not behind the queue");
  check(bank.queued() == Order({1, 2, 4}), "and its request withdrawn");
  const std::string* zero = nullptr;
  std::thread getter([&] { zero = bank.get(0); });
  check(waitUntil([&] { return bank.waiting(0) == 1; }), "get() of the one rendering waits for it");
  fake.release();
  getter.join();
  check(zero && *zero == "pcm0", "and gets the worker's render");
  check(waitUntil([&] { return bank.readyCount() == 5; }), "the rest finish");
  const Order rendered = fake.log();
  check(std::count(rendered.begin(), rendered.end(), std::size_t(0)) == 1 &&
      std::count(rendered.begin(), rendered.end(), std::size_t(3)) == 1,
    "nothing rendered twice");
}

// A get() waiting for the worker's render, which then throws: it does not
// come back empty-handed, but renders the item itself.
void testBankGetAfterFailedRender()
{
  FakeBank fake;
  fake.holdAt(0);
  fake.boom = 0;
  Bank bank(fake.fn(), 2);
  bank.prerender({0});
  check(fake.reached(0), "the worker is held on a render that will throw");
  const std::string* zero = nullptr;
  std::thread::id getterId;
  std::thread getter([&] {
    getterId = std::this_thread::get_id();
    zero = bank.get(0);
  });
  check(waitUntil([&] { return bank.waiting(0) == 1; }), "get() waits for it");
  fake.release();
  getter.join();
  check(zero && *zero == "pcm0", "and when it throws, gets the item anyway");
  check(fake.log() == Order({0, 0}) && fake.byLast(0) == getterId, "rendered once more, on the caller's thread");
}

// Audio lowers the effects' worker's priority from its start hook.
void testBankWorkerHook()
{
  FakeBank fake;
  std::atomic<int> calls{0};
  std::atomic<bool> onWorker{false};
  const std::thread::id self = std::this_thread::get_id();
  {
    Bank bank(fake.fn(), 1, true, [&] {
      onWorker = std::this_thread::get_id() != self;
      ++calls;
    });
    check(waitUntil([&] { return calls.load() == 1; }), "the worker's start hook runs");
    check(onWorker.load(), "on the worker's thread");
    Bank unthreaded(fake.fn(), 1, false, [&] { ++calls; });
  }
  check(calls.load() == 1, "once, and not without a worker");
}

void testBankWithoutThreads()
{
  FakeBank fake;
  Bank bank(fake.fn(), 4, false);
  bank.prerender({0, 1, 2, 3});
  check(bank.queued().empty(), "without threads prerender() queues nothing");
  const std::string* one = bank.tryGet(1);
  check(one && *one == "pcm1" && fake.by(1) == std::this_thread::get_id(), "tryGet() renders on the spot");
  const std::string* two = bank.get(2);
  check(two && *two == "pcm2" && fake.by(2) == std::this_thread::get_id(), "so does get()");
  check(bank.tryGet(1) == one && bank.get(2) == two, "after which both hand it over");
  check(!bank.get(4) && !bank.tryGet(4), "a number past the end is nothing");
  check(fake.log() == Order({1, 2}), "nothing else rendered");
}

void testBankStopAndFailure()
{
  FakeBank fake;
  fake.holdAt(0);
  fake.boom = 2;
  Bank bank(fake.fn(), 4);
  bank.prerender({0, 1, 2, 3});
  check(fake.reached(0), "the worker is held on 0");
  bank.stop();
  check(bank.queued().empty(), "stop() cancels the queue");
  fake.release();
  check(waitUntil([&] { return bank.ready(0); }), "the render in progress is kept");
  const std::string* one = bank.tryGet(1);
  check(one && *one == "pcm1" && bank.queued().empty(), "later misses are rendered on the spot, not queued");
  check(!bank.get(2) && !bank.ready(2), "a render that throws leaves nothing");
  check(fake.by(2) == std::this_thread::get_id(), "(rendered by the caller)");
  check(bank.get(2) && *bank.get(2) == "pcm2", "asked again, it is rendered again");
  check(fake.log() == Order({0, 1, 2, 2}), "nothing else rendered");
}

void testSfxRenderOrder()
{
  const Order order = Audio::sfxRenderOrder();
  check(order.size() == std::size_t(Sfx::Count), "every effect is in the order");
  Order sorted = order;
  std::sort(sorted.begin(), sorted.end());
  Order all(std::size_t(Sfx::Count));
  std::iota(all.begin(), all.end(), std::size_t(0));
  check(sorted == all, "each once");
  check(order.size() > 3 && order[0] == std::size_t(Sfx::MenuMove) && order[1] == std::size_t(Sfx::MenuSelect),
    "the title's menu sounds come first");
  check(order[2] == std::size_t(Sfx::Jump), "then the enum's order, which starts with those every level uses");
}

} // namespace

int main()
{
  testRequestAndTake();
  testOneWorkerInOrder();
  testLevelTrackFirst();
  testTakeQueuedAndRendering();
  testCap();
  testCapEvictsStaleFirst();
  testCapKeepsWaitedResult();
  testDrop();
  testDropThenAskAgain();
  testDropWhileTaking();
  testStale();
  testWithoutThreads();
  testDestructor();
  testDestructorWhileTaking();
  testThrowingRender();
  testRequestFirst();
  testTakeReady();
  testDemote();
  testTakeFailed();
  testPendingMusic();
  testAudioLastRequestWins();
  testAudioCancelPending();
  testAudioSeek();
  testAudioPollRenews();
  testAudioFailedRender();
  testAudioCancelDemotes();
  testAudioPlayMusic();
  testAudioTrackCache();
  testAudioPlaySfx();
  testBankOrder();
  testBankTryGetNeverWaits();
  testBankGet();
  testBankGetAfterFailedRender();
  testBankWorkerHook();
  testBankWithoutThreads();
  testBankStopAndFailure();
  testSfxRenderOrder();
  std::printf("%s\n", gFailures == 0 ? "all render queue checks passed" : "render queue checks FAILED");
  return gFailures == 0 ? 0 : 1;
}
