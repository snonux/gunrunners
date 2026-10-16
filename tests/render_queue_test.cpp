// The background renderer behind Audio::preloadMusic (src/audio/render_queue.hpp),
// with a fake render that can be held at a gate instead of 1-4 s of synthesis.
// Checks that:
//   - a request renders once on the worker and take() hands it over once;
//   - a repeated request (queued, rendering or finished) is not rendered again;
//   - one worker renders one thing at a time, oldest request first;
//   - requestFirst() goes ahead of the queue (moving a queued id), so a
//     level's music queued before its briefing's survives a skip;
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
//     and the worker keeps going.
//
// No check depends on a pause being long enough: a take() is known to be
// blocked once RenderQueue::waiting() counts it, and only then is the gate
// opened. waitUntil only bounds how long a broken queue may hang the test.

#include "audio/render_queue.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <functional>
#include <memory>
#include <mutex>
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

void testRequestFirst()
{
  FakeRender fake;
  fake.close();
  Queue q(fake.fn(), 8);
  q.request("a");
  check(fake.reached("a"), "the worker is held on a");
  q.request("b");
  q.request("c");
  check(q.requestFirst("first"), "requestFirst() queues a new id");
  check(!q.requestFirst("c"), "and moves one already queued instead of queueing it twice");
  check(!q.requestFirst("a"), "the id rendering now is not queued again");
  check(q.queuedCount() == 3, "nothing was queued twice");
  fake.release();
  check(waitUntil([&] { return q.readyCount() == 4; }), "all four finish");
  check(fake.log() == Log{"a", "c", "first", "b"}, "front requests next (the latest first), then the rest in order");
  check(!q.requestFirst("b") && q.queuedCount() == 0, "a finished id is not queued again");
}

// What Game::playBriefing does: the level's music first, then the scenes'
// tracks; the first scene's track is taken at once, and the briefing is
// skipped before any cue plays (the level start: markStale, take, dropStale).
void testLevelTrackFirst()
{
  FakeRender fake;
  fake.close();
  Queue q(fake.fn(), 8);
  q.requestFirst("level");
  check(fake.reached("level"), "the level's track renders first");
  q.request("scene");
  q.request("cue");
  check(!q.take("scene"), "the first scene's track, still queued, is rendered by the caller");
  q.markStale();
  Taker taker(q, "level");
  check(blockedIn(q, "level"), "the level start waits for its track in progress");
  fake.release();
  const auto level = taker.result();
  check(level && *level == "pcm:level", "and gets it, not cancelled by markStale()");
  q.dropStale();
  check(q.queuedCount() == 0 && q.readyCount() == 0, "the skipped cue was cancelled");
  check(fake.log() == Log{"level"}, "only the level's track rendered");
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

} // namespace

int main()
{
  testRequestAndTake();
  testOneWorkerInOrder();
  testRequestFirst();
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
  std::printf("%s\n", gFailures == 0 ? "all render queue checks passed" : "render queue checks FAILED");
  return gFailures == 0 ? 0 : 1;
}
