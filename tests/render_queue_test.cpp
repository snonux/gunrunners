// The background renderer behind Audio::preloadMusic (src/audio/render_queue.hpp),
// with a fake render that can be held at a gate instead of 1-4 s of synthesis.
// Checks that:
//   - a request renders once on the worker and take() hands it over once;
//   - a repeated request (queued, rendering or finished) is not rendered again;
//   - one worker renders one thing at a time, oldest request first;
//   - take() on a request still queued withdraws it (the caller renders it),
//     and on one rendering waits for it;
//   - finished results past the cap are thrown away oldest first;
//   - drop() cancels the queue, frees finished results and throws away the
//     render in progress, unless it is asked for again (request or take);
//   - without threads nothing renders, take() hands back nothing;
//   - the destructor cancels the queue and waits for the render in progress;
//   - a render that throws leaves no result and the worker keeps going.

#include "audio/render_queue.hpp"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace gr;

namespace
{

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
// closed, holds the worker inside it. "boom" throws.
struct FakeRender
{
  std::mutex m;
  std::condition_variable cv;
  bool open = true;
  std::vector<std::string> started; // in order
  int active = 0;
  int maxActive = 0;

  std::unique_ptr<std::string> render(const std::string& id)
  {
    std::unique_lock<std::mutex> lock(m);
    started.push_back(id);
    maxActive = std::max(maxActive, ++active);
    cv.wait(lock, [&] { return open; });
    --active;
    if (id == "boom")
      throw std::runtime_error("boom");
    return std::make_unique<std::string>("pcm:" + id);
  }
  RenderQueue<std::string>::Render fn()
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
  std::vector<std::string> log()
  {
    std::lock_guard<std::mutex> lock(m);
    return started;
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

// Opens the gate after a pause, from another thread, so the test thread can
// block in take() or the destructor meanwhile.
std::thread releaseLater(FakeRender& fake, int ms = 200)
{
  return std::thread([&fake, ms] {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    fake.release();
  });
}

void testRequestAndTake()
{
  FakeRender fake;
  RenderQueue<std::string> q(fake.fn(), 4);
  check(q.request("a"), "a new id is queued");
  check(waitUntil([&] { return q.ready("a"); }), "the worker renders it");
  check(!q.request("a"), "a finished id is not queued again");
  const auto out = q.take("a");
  check(out && *out == "pcm:a", "take() hands the result over");
  check(!q.take("a"), "only once");
  check(q.readyCount() == 0, "nothing is left behind");
  check(fake.log() == std::vector<std::string>{"a"}, "it rendered exactly once");
}

void testOneWorkerInOrder()
{
  FakeRender fake;
  fake.close();
  RenderQueue<std::string> q(fake.fn(), 8);
  q.request("a");
  check(fake.reached("a"), "the worker starts on the first request");
  q.request("b");
  q.request("c");
  check(!q.request("a"), "an id rendering now is not queued again");
  check(!q.request("b"), "a queued id is not queued twice");
  check(q.queuedCount() == 2, "the rest wait in the queue");
  fake.release();
  check(waitUntil([&] { return q.readyCount() == 3; }), "all three finish");
  check(fake.log() == std::vector<std::string>{"a", "b", "c"}, "oldest request first, each once");
  check(fake.maxActive == 1, "one render at a time");
}

void testTakeQueuedAndRendering()
{
  FakeRender fake;
  fake.close();
  RenderQueue<std::string> q(fake.fn(), 8);
  q.request("a");
  check(fake.reached("a"), "the worker is held on a");
  q.request("b");
  check(!q.take("b"), "take() of a queued id hands back nothing (the caller renders it)");
  check(q.queuedCount() == 0, "and withdraws the request");
  std::thread opener = releaseLater(fake);
  const auto a = q.take("a");
  opener.join();
  check(a && *a == "pcm:a", "take() of the id rendering waits for it");
  q.request("z");
  check(waitUntil([&] { return q.ready("z"); }), "the worker keeps going");
  check(fake.log() == std::vector<std::string>{"a", "z"}, "the withdrawn request never rendered");
}

void testCap()
{
  FakeRender fake;
  RenderQueue<std::string> q(fake.fn(), 2);
  for (const char* id : {"a", "b", "c"})
    q.request(id);
  check(waitUntil([&] { return q.ready("c"); }), "every request finishes");
  check(q.readyCount() == 2, "finished results stop at the cap");
  check(!q.ready("a") && q.ready("b") && q.ready("c"), "the oldest went first");
  check(!q.take("a"), "an evicted id hands back nothing (the caller renders it)");
  check(q.request("a"), "and can be requested again");
  check(waitUntil([&] { return q.ready("a"); }), "which renders it again");
  check(q.readyCount() == 2 && !q.ready("b"), "evicting the next oldest");
}

void testDrop()
{
  FakeRender fake;
  RenderQueue<std::string> q(fake.fn(), 8);
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
  check(fake.log() == std::vector<std::string>{"old", "a", "z"}, "the cancelled request never rendered");
}

void testDropThenAskAgain()
{
  FakeRender fake;
  fake.close();
  RenderQueue<std::string> q(fake.fn(), 8);
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
  std::thread opener = releaseLater(fake);
  const auto b = q.take("b");
  opener.join();
  check(b && *b == "pcm:b", "take() after drop() still gets the render in progress");
  check(fake.log() == std::vector<std::string>{"a", "b"}, "neither rendered twice");
}

void testWithoutThreads()
{
  FakeRender fake;
  RenderQueue<std::string> q(fake.fn(), 4, false);
  check(q.request("a"), "a request is remembered");
  check(!q.request("a"), "once");
  check(q.queuedCount() == 1, "and waits in the queue");
  check(!q.take("a"), "take() hands back nothing: the caller renders it");
  check(q.queuedCount() == 0, "and withdraws it");
  q.request("b");
  q.drop();
  check(q.queuedCount() == 0, "drop() empties the queue");
  check(fake.log().empty(), "nothing ever rendered in the background");
}

void testDestructor()
{
  FakeRender fake;
  fake.close();
  std::thread opener;
  {
    RenderQueue<std::string> q(fake.fn(), 8);
    q.request("a");
    check(fake.reached("a"), "the worker is held on a");
    q.request("b");
    q.request("c");
    opener = releaseLater(fake);
    // ~RenderQueue cancels b and c at once, then waits for a, which the
    // opener lets finish a moment later.
  }
  opener.join();
  check(fake.log() == std::vector<std::string>{"a"}, "the destructor cancels the queue and finishes the render in progress");
  check(fake.active == 0, "nothing is left rendering");
}

void testThrowingRender()
{
  FakeRender fake;
  RenderQueue<std::string> q(fake.fn(), 4);
  q.request("boom");
  q.request("ok");
  check(waitUntil([&] { return q.ready("ok"); }), "the worker survives a render that throws");
  check(!q.ready("boom") && !q.take("boom"), "which leaves no result");
}

} // namespace

int main()
{
  testRequestAndTake();
  testOneWorkerInOrder();
  testTakeQueuedAndRendering();
  testCap();
  testDrop();
  testDropThenAskAgain();
  testWithoutThreads();
  testDestructor();
  testThrowingRender();
  std::printf("%s\n", gFailures == 0 ? "all render queue checks passed" : "render queue checks FAILED");
  return gFailures == 0 ? 0 : 1;
}
