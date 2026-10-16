#pragma once

#include <algorithm>
#include <condition_variable>
#include <cstddef>
#include <cstdio>
#include <deque>
#include <exception>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace gr
{

// Renders things by id (the named music tracks, see Audio::preloadMusic) on
// one background thread, oldest request first, and keeps each result until
// someone take()s it.
//
// One worker instead of a thread per request: a level start asks for up to
// ~8 tracks at once (its variants and layers), and each takes 1-4 s of CPU,
// so a thread apiece had them all fighting the game thread. The price is
// readiness: a track waits for every request ahead of it, so callers
// request in the order the tracks are needed, and requestFirst() puts the
// one surely needed ahead of those that only may be (Game::playBriefing:
// the level's own music before the briefing's, which a skip never plays).
// One needed before the worker gets to it is rendered by the caller
// instead (see take()).
//
// Finished results nobody has taken yet are capped at `keepReady` (a track
// is ~12 MB): past that stale ones go first, then the oldest, and an evicted
// one is rendered again should it be asked for later.
//
// A new level marks everything stale (markStale()), asks for its own tracks
// (request() makes a stale one fresh again, so a track two levels share is
// neither freed nor rendered twice) and then frees what is still stale
// (dropStale()). drop() forgets everything at once (back to the title).
//
// Without threads (the browser build without pthreads) nothing renders in
// the background: requests are only remembered, and take() hands back
// nothing, so the caller renders the item when it is needed.
//
// All members may be called from any thread; render runs on the worker
// without the lock held. A result a take() is blocked on is never thrown
// away (not by the cap, drop() or dropStale()), and the destructor waits
// for such take()s to collect it.
template <class T>
class RenderQueue
{
public:
  using Render = std::function<std::unique_ptr<T>(const std::string&)>;

  RenderQueue(Render render, std::size_t keepReady, bool threaded = true)
    : mRender(std::move(render))
    , mKeepReady(std::max<std::size_t>(1, keepReady))
  {
    if (threaded)
      mWorker = std::thread([this] { run(); });
  }

  // Stops the worker, waits for the render in progress (it can't be
  // interrupted), then for every take() already blocked on it to collect
  // its result, so none of them touches a destroyed queue.
  ~RenderQueue()
  {
    stop();
    if (mWorker.joinable())
      mWorker.join();
    std::unique_lock<std::mutex> lock(mMutex);
    mDone.wait(lock, [this] { return mWanted.empty(); });
  }

  RenderQueue(const RenderQueue&) = delete;
  RenderQueue& operator=(const RenderQueue&) = delete;

  // Cancels everything queued and has the worker end after the render in
  // progress. Later requests are only remembered, as without threads.
  void stop()
  {
    {
      std::lock_guard<std::mutex> lock(mMutex);
      mStop = true;
      mQueue.clear();
    }
    mWake.notify_all();
  }

  // Queues `id` unless it is already queued, rendering or finished; one
  // already rendering or finished is wanted again (no longer stale, and a
  // finished one becomes the newest, the last the cap evicts). Returns
  // whether it was newly queued.
  bool request(const std::string& id) { return add(id, false); }

  // As request(), but ahead of every queued request, so the worker starts
  // it as soon as the render in progress ends; one already queued moves to
  // the front. For the result certain to be wanted when those queued only
  // may be: a level's own music goes before its briefing's tracks, so the
  // level has it even if the briefing is skipped at once.
  bool requestFirst(const std::string& id) { return add(id, true); }

  // Claims the result for `id`: a finished one is handed over, one in
  // progress is waited for. A request still in the queue is withdrawn and,
  // as for an id never requested (or dropped, or evicted, or whose render
  // threw), nullptr comes back: the caller renders it itself, which beats
  // waiting for the worker to get through the requests ahead of it.
  std::unique_ptr<T> take(const std::string& id)
  {
    std::unique_lock<std::mutex> lock(mMutex);
    if (renderingLocked(id))
      waitForLocked(lock, id);
    if (auto it = readyLocked(id); it != mReady.end())
    {
      std::unique_ptr<T> out = std::move(it->out);
      mReady.erase(it);
      return out;
    }
    if (auto q = queuedLocked(id); q != mQueue.end())
      mQueue.erase(q);
    return nullptr;
  }

  // A new level starts: cancels every queued request and marks every
  // finished result, and the render in progress, stale until a request()
  // or take() wants it again.
  void markStale()
  {
    std::lock_guard<std::mutex> lock(mMutex);
    mQueue.clear();
    for (auto& e : mReady)
      e.stale = true;
    if (mCurrentKeep == Keep::Fresh)
      mCurrentKeep = Keep::Stale;
  }

  // Frees the finished results still stale. The render in progress, if
  // still stale, runs to the end (it can't be interrupted) and is thrown
  // away then, unless it is requested or taken meanwhile.
  void dropStale()
  {
    std::lock_guard<std::mutex> lock(mMutex);
    eraseLocked([](const Entry& e) { return e.stale; });
    if (mCurrentKeep == Keep::Stale)
      mCurrentKeep = Keep::Drop;
  }

  // Forgets every queued request and finished result. The render in
  // progress runs to the end and is thrown away, unless it is requested or
  // taken again meanwhile.
  void drop()
  {
    std::lock_guard<std::mutex> lock(mMutex);
    mQueue.clear();
    eraseLocked([](const Entry&) { return true; });
    mCurrentKeep = Keep::Drop;
  }

  // Whether a finished result for `id` is waiting to be taken.
  bool ready(const std::string& id) const
  {
    std::lock_guard<std::mutex> lock(mMutex);
    return readyLocked(id) != mReady.end();
  }
  std::size_t readyCount() const
  {
    std::lock_guard<std::mutex> lock(mMutex);
    return mReady.size();
  }
  // Requests not started yet (the one rendering is not counted).
  std::size_t queuedCount() const
  {
    std::lock_guard<std::mutex> lock(mMutex);
    return mQueue.size();
  }
  // How many take()s are blocked on the render of `id`. A take() counted
  // here is already inside its wait, which lets a test release the render
  // knowing the take() really waited for it.
  std::size_t waiting(const std::string& id) const
  {
    std::lock_guard<std::mutex> lock(mMutex);
    return std::size_t(std::count(mWanted.begin(), mWanted.end(), id));
  }

private:
  struct Entry
  {
    std::string id;
    std::unique_ptr<T> out;
    bool stale = false;
  };
  // What becomes of the render in progress when it ends.
  enum class Keep
  {
    Fresh, // kept
    Stale, // kept, but freed by the next dropStale()
    Drop,  // thrown away (unless a take() waits for it)
  };

  // request() and requestFirst(): queues `id` at the back or the front.
  bool add(const std::string& id, bool first)
  {
    {
      std::lock_guard<std::mutex> lock(mMutex);
      if (renderingLocked(id))
      {
        mCurrentKeep = Keep::Fresh;
        return false;
      }
      if (auto it = readyLocked(id); it != mReady.end())
      {
        it->stale = false;
        std::rotate(it, std::next(it), mReady.end());
        return false;
      }
      if (auto q = queuedLocked(id); q != mQueue.end())
      {
        if (first)
          std::rotate(mQueue.begin(), q, std::next(q));
        return false;
      }
      if (first)
        mQueue.push_front(id);
      else
        mQueue.push_back(id);
    }
    mWake.notify_one();
    return true;
  }

  void run()
  {
    std::unique_lock<std::mutex> lock(mMutex);
    for (;;)
    {
      mWake.wait(lock, [this] { return mStop || !mQueue.empty(); });
      if (mStop)
        return;
      renderNextLocked(lock);
    }
  }

  // Renders the oldest request with the lock released. An exception
  // escaping the worker's thread would end the program (std::terminate),
  // so nothing here can throw: the id is swapped (not copied) into
  // mCurrent, the render and the store catch their own failures, and a
  // failure only costs the preload (the caller renders it when needed).
  void renderNextLocked(std::unique_lock<std::mutex>& lock)
  {
    mCurrent.swap(mQueue.front());
    mQueue.pop_front();
    mBusy = true;
    mCurrentKeep = Keep::Fresh;
    lock.unlock();
    // Only this thread writes mCurrent, so reading it unlocked is safe.
    std::unique_ptr<T> out = renderSafely(mCurrent);
    lock.lock();
    mBusy = false;
    const bool wanted = wantedLocked(mCurrent);
    if (out && (mCurrentKeep != Keep::Drop || wanted))
      storeLocked(std::move(out), mCurrentKeep == Keep::Stale && !wanted);
    mDone.notify_all();
  }

  // A render that throws (out of memory) leaves no result rather than
  // ending the program on the worker; take() then hands back nullptr and
  // the caller renders it itself. The failure goes to stderr, as the rest
  // of the audio's do, so a broken track is visible.
  std::unique_ptr<T> renderSafely(const std::string& id) noexcept
  {
    try
    {
      return mRender(id);
    }
    catch (const std::exception& e)
    {
      std::fprintf(stderr, "background render of '%s' failed: %s\n", id.c_str(), e.what());
    }
    catch (...)
    {
      std::fprintf(stderr, "background render of '%s' failed\n", id.c_str());
    }
    return nullptr;
  }

  // Adds the finished result for mCurrent and trims to the cap. Out of
  // memory for the entry itself, the result is freed instead (logged).
  void storeLocked(std::unique_ptr<T> out, bool stale) noexcept
  {
    try
    {
      mReady.push_back(Entry{mCurrent, std::move(out), stale});
    }
    catch (const std::exception& e)
    {
      std::fprintf(stderr, "background render of '%s' dropped: %s\n", mCurrent.c_str(), e.what());
      return;
    }
    trimLocked();
  }

  // Evicts past the cap, stale results first, then the oldest; one a
  // take() is blocked on always stays (it is gone again once taken).
  void trimLocked()
  {
    for (const bool staleOnly : {true, false})
      for (auto it = mReady.begin(); mReady.size() > mKeepReady && it != mReady.end();)
        it = ((staleOnly && !it->stale) || wantedLocked(it->id)) ? std::next(it) : mReady.erase(it);
  }

  // Frees the finished results `pick` selects, except those a take() is
  // blocked on. It runs under the lock but allocates nothing; freeing a
  // few buffers only delays the worker's next store a little.
  template <class Pick>
  void eraseLocked(Pick pick)
  {
    const auto gone = [&](const Entry& e) { return pick(e) && !wantedLocked(e.id); };
    mReady.erase(std::remove_if(mReady.begin(), mReady.end(), gone), mReady.end());
  }

  // Blocks until the render of `id` in progress is over. Listed as wanted
  // meanwhile, so neither the cap, drop() nor dropStale() can throw the
  // result away before this thread gets the lock back. The destructor
  // waits for the list to empty, hence the notify.
  void waitForLocked(std::unique_lock<std::mutex>& lock, const std::string& id)
  {
    mWanted.push_back(id);
    mDone.wait(lock, [&] { return !renderingLocked(id); });
    mWanted.erase(std::find(mWanted.begin(), mWanted.end(), id));
    mDone.notify_all();
  }

  bool renderingLocked(const std::string& id) const { return mBusy && mCurrent == id; }
  bool wantedLocked(const std::string& id) const
  {
    return std::find(mWanted.begin(), mWanted.end(), id) != mWanted.end();
  }
  typename std::deque<std::string>::iterator queuedLocked(const std::string& id)
  {
    return std::find(mQueue.begin(), mQueue.end(), id);
  }
  typename std::vector<Entry>::iterator readyLocked(const std::string& id)
  {
    return std::find_if(mReady.begin(), mReady.end(), [&](const Entry& e) { return e.id == id; });
  }
  typename std::vector<Entry>::const_iterator readyLocked(const std::string& id) const
  {
    return std::find_if(mReady.begin(), mReady.end(), [&](const Entry& e) { return e.id == id; });
  }

  const Render mRender;
  const std::size_t mKeepReady;
  mutable std::mutex mMutex;
  std::condition_variable mWake; // a request came in, or stop
  std::condition_variable mDone; // a render finished, or a take() stopped waiting
  std::deque<std::string> mQueue;   // oldest first
  std::vector<Entry> mReady;        // oldest first
  std::vector<std::string> mWanted; // ids take() is blocked on
  std::string mCurrent;             // what the worker renders, while mBusy
  bool mBusy = false;
  Keep mCurrentKeep = Keep::Fresh;
  bool mStop = false;
  std::thread mWorker; // started in the constructor's body, joined in the destructor's
};

} // namespace gr
