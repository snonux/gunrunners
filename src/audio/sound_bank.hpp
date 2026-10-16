#pragma once

#include <algorithm>
#include <condition_variable>
#include <cstddef>
#include <cstdio>
#include <deque>
#include <exception>
#include <functional>
#include <iterator>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace gr
{

// A fixed set of things numbered 0..count-1 (the sound effects, see
// Audio::play), each rendered once, on one background thread, so the first
// frame does not wait for them: all 425 effects take ~3.5 s of CPU, which
// Audio's constructor used to spend before the window showed anything.
//
// prerender() queues them in the order they are likely needed (the menu's
// first, see Audio::sfxRenderOrder). A rendered item is kept for the bank's
// lifetime at a fixed address, so a pointer handed out stays valid (the
// mixer plays straight from it) and the item never changes again.
//
// Two ways to ask for one:
//   - tryGet() never waits: one not rendered yet comes back as nullptr and
//     moves to the front of the queue (behind the render in progress), so
//     a sound played before its turn is skipped once and is there a few
//     milliseconds later, for its next play;
//   - get() always hands it over: one queued or never asked for is rendered
//     by the caller, one the worker is on is waited for (one item, a few
//     milliseconds, never the whole set). Headless recording uses it, so a
//     clip has every sound whatever the worker's speed, and so does the
//     browser build without threads, where nothing renders in the
//     background.
//
// Without threads (threaded = false) requests are not even remembered:
// tryGet() only hands over what get() rendered.
//
// All members may be called from any thread; the render runs without the
// lock held, on the worker or on get()'s caller.
template <class T>
class SoundBank
{
public:
  using Render = std::function<T(std::size_t)>;

  SoundBank(Render render, std::size_t count, bool threaded = true)
    : mRender(std::move(render))
    , mThreaded(threaded)
    , mItems(count)
    , mState(count, State::Idle)
  {
    if (threaded)
      mWorker = std::thread([this] { run(); });
  }

  // Cancels the queue, then waits for the render in progress (it can't be
  // interrupted). Nobody may be inside get() any more.
  ~SoundBank()
  {
    stop();
    if (mWorker.joinable())
      mWorker.join();
  }

  SoundBank(const SoundBank&) = delete;
  SoundBank& operator=(const SoundBank&) = delete;

  // Cancels the queue and has the worker end after the render in progress.
  // Later requests are not remembered, as without threads; get() still
  // renders on the spot.
  void stop()
  {
    {
      std::lock_guard<std::mutex> lock(mMutex);
      mStop = true;
      for (const std::size_t i : mQueue)
        mState[i] = State::Idle;
      mQueue.clear();
    }
    mWake.notify_all();
  }

  // Queues every item of `order` not queued, rendering or rendered yet,
  // behind what is queued already, in that order. Numbers past the end are
  // ignored.
  void prerender(const std::vector<std::size_t>& order)
  {
    {
      std::lock_guard<std::mutex> lock(mMutex);
      for (const std::size_t i : order)
        if (i < mState.size() && mState[i] == State::Idle)
          enqueueLocked(i, false);
    }
    mWake.notify_one();
  }

  // The item if it is rendered; nullptr otherwise, never waiting. A miss
  // asks for it ahead of the rest of the queue (one being rendered stays
  // where it is), so the worker renders it next.
  const T* tryGet(std::size_t i)
  {
    {
      std::lock_guard<std::mutex> lock(mMutex);
      if (i >= mState.size())
        return nullptr;
      if (mState[i] == State::Done)
        return mItems[i].get();
      if (mState[i] == State::Rendering)
        return nullptr;
      enqueueLocked(i, true);
    }
    mWake.notify_one();
    return nullptr;
  }

  // The item, rendered here if nobody has (a queued request is withdrawn),
  // or waited for while the worker renders it. nullptr only for a number
  // past the end or a render that failed (logged; asked for again, it is
  // rendered again).
  const T* get(std::size_t i)
  {
    std::unique_lock<std::mutex> lock(mMutex);
    if (i >= mState.size())
      return nullptr;
    if (mState[i] == State::Rendering)
      waitForLocked(lock, i);
    if (mState[i] == State::Done)
      return mItems[i].get();
    // Queued or idle (or the worker's render just failed): rendered here.
    if (mState[i] == State::Queued)
      mQueue.erase(std::find(mQueue.begin(), mQueue.end(), i));
    mState[i] = State::Rendering;
    lock.unlock();
    std::unique_ptr<const T> out = renderSafely(i);
    lock.lock();
    finishLocked(i, std::move(out));
    return mItems[i].get();
  }

  // Whether item `i` is rendered.
  bool ready(std::size_t i) const
  {
    std::lock_guard<std::mutex> lock(mMutex);
    return i < mState.size() && mState[i] == State::Done;
  }
  std::size_t readyCount() const
  {
    std::lock_guard<std::mutex> lock(mMutex);
    return std::size_t(std::count(mState.begin(), mState.end(), State::Done));
  }
  // The requests not started yet, next first (the one rendering is not
  // listed).
  std::vector<std::size_t> queued() const
  {
    std::lock_guard<std::mutex> lock(mMutex);
    return std::vector<std::size_t>(mQueue.begin(), mQueue.end());
  }
  // How many get()s wait for the worker's render of `i`. One counted here
  // is already inside its wait, which lets a test release the render
  // knowing get() really waited for it.
  std::size_t waiting(std::size_t i) const
  {
    std::lock_guard<std::mutex> lock(mMutex);
    return std::size_t(std::count(mWaiting.begin(), mWaiting.end(), i));
  }

private:
  enum class State : unsigned char
  {
    Idle,      // not asked for (or its render failed)
    Queued,    // in mQueue
    Rendering, // on the worker or a get()'s caller, without the lock
    Done,      // in mItems for good
  };

  // Puts idle or queued item `i` in the queue, at the front or the back
  // (a queued one only moves, and only to the front). Nothing without a
  // worker to render it.
  void enqueueLocked(std::size_t i, bool first)
  {
    if (!mThreaded || mStop)
      return;
    if (mState[i] == State::Queued)
    {
      if (first)
      {
        const auto q = std::find(mQueue.begin(), mQueue.end(), i);
        std::rotate(mQueue.begin(), q, std::next(q));
      }
      return;
    }
    if (first)
      mQueue.push_front(i);
    else
      mQueue.push_back(i);
    mState[i] = State::Queued; // after the push, which may throw
  }

  void run()
  {
    std::unique_lock<std::mutex> lock(mMutex);
    for (;;)
    {
      mWake.wait(lock, [this] { return mStop || !mQueue.empty(); });
      if (mStop)
        return;
      const std::size_t i = mQueue.front();
      mQueue.pop_front();
      mState[i] = State::Rendering;
      lock.unlock();
      std::unique_ptr<const T> out = renderSafely(i);
      lock.lock();
      finishLocked(i, std::move(out));
    }
  }

  // Stores a finished render (nothing here throws) and wakes the get()s
  // waiting for it. A failed one leaves the item idle: a later get()
  // renders it on the spot, a later tryGet() asks the worker again.
  void finishLocked(std::size_t i, std::unique_ptr<const T> out)
  {
    mItems[i] = std::move(out);
    mState[i] = mItems[i] ? State::Done : State::Idle;
    mDone.notify_all();
  }

  // A render that throws (out of memory) leaves no result rather than
  // ending the program on the worker (an exception escaping a thread calls
  // std::terminate). Logged to stderr, as the rest of the audio's failures.
  std::unique_ptr<const T> renderSafely(std::size_t i) noexcept
  {
    try
    {
      return std::make_unique<const T>(mRender(i));
    }
    catch (const std::exception& e)
    {
      std::fprintf(stderr, "render of sound %u failed: %s\n", unsigned(i), e.what());
    }
    catch (...)
    {
      std::fprintf(stderr, "render of sound %u failed\n", unsigned(i));
    }
    return nullptr;
  }

  // Blocks until the render of `i` in progress (the worker's, or another
  // get()'s) is over, counted in mWaiting meanwhile (see waiting()).
  void waitForLocked(std::unique_lock<std::mutex>& lock, std::size_t i)
  {
    mWaiting.push_back(i);
    mDone.wait(lock, [&] { return mState[i] != State::Rendering; });
    mWaiting.erase(std::find(mWaiting.begin(), mWaiting.end(), i));
  }

  const Render mRender;
  const bool mThreaded;
  mutable std::mutex mMutex;
  std::condition_variable mWake; // a request came in, or stop
  std::condition_variable mDone; // a render finished
  // One slot per item, never resized: a rendered item keeps its address.
  std::vector<std::unique_ptr<const T>> mItems;
  std::vector<State> mState;
  std::deque<std::size_t> mQueue;   // next first
  std::vector<std::size_t> mWaiting; // items get() waits for
  bool mStop = false;
  std::thread mWorker; // started in the constructor's body, joined in the destructor's
};

} // namespace gr
