#pragma once

#include <algorithm>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
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
// so a thread apiece had them all fighting the game thread. Finished results
// nobody has taken yet are capped at `keepReady` (a track is ~12 MB): past
// that the oldest is thrown away, and rendered again should it be asked for
// later. drop() forgets everything at once (a new level starts).
//
// Without threads (the browser build without pthreads) nothing renders in
// the background: requests are only remembered, and take() hands back
// nothing, so the caller renders the item when it is needed.
//
// All members may be called from any thread; render runs on the worker
// without the lock held.
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

  // Cancels whatever is still queued and waits for the render in progress,
  // which can't be interrupted.
  ~RenderQueue()
  {
    {
      std::lock_guard<std::mutex> lock(mMutex);
      mStop = true;
      mQueue.clear();
    }
    mWake.notify_all();
    if (mWorker.joinable())
      mWorker.join();
  }

  RenderQueue(const RenderQueue&) = delete;
  RenderQueue& operator=(const RenderQueue&) = delete;

  // Queues `id` unless it is already queued, rendering or finished. Returns
  // whether it was newly queued.
  bool request(const std::string& id)
  {
    {
      std::lock_guard<std::mutex> lock(mMutex);
      if (renderingLocked(id))
      {
        mKeepCurrent = true; // wanted again after a drop()
        return false;
      }
      if (queuedLocked(id) != mQueue.end() || readyLocked(id) != mReady.end())
        return false;
      mQueue.push_back(id);
    }
    mWake.notify_one();
    return true;
  }

  // Claims the result for `id`: a finished one is handed over, one in
  // progress is waited for. A request still in the queue is withdrawn and,
  // as for an id never requested (or dropped, or evicted), nullptr comes
  // back: the caller renders it itself, which beats waiting for the worker
  // to get through the requests ahead of it.
  std::unique_ptr<T> take(const std::string& id)
  {
    std::unique_lock<std::mutex> lock(mMutex);
    if (renderingLocked(id))
    {
      // Listed as wanted, so neither a drop() nor the cap can throw the
      // result away before this thread gets the lock back.
      mWanted.push_back(id);
      mDone.wait(lock, [&] { return !renderingLocked(id); });
      mWanted.erase(std::find(mWanted.begin(), mWanted.end(), id));
    }
    if (auto it = readyLocked(id); it != mReady.end())
    {
      std::unique_ptr<T> out = std::move(it->second);
      mReady.erase(it);
      return out;
    }
    if (auto q = queuedLocked(id); q != mQueue.end())
      mQueue.erase(q);
    return nullptr;
  }

  // Forgets every queued request and finished result. The render in
  // progress runs to the end and is thrown away, unless it is requested or
  // taken again meanwhile.
  void drop()
  {
    std::vector<Entry> old;
    {
      std::lock_guard<std::mutex> lock(mMutex);
      mQueue.clear();
      old.swap(mReady); // freed on return, outside the lock
      mKeepCurrent = false;
    }
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

private:
  using Entry = std::pair<std::string, std::unique_ptr<T>>;

  void run()
  {
    std::unique_lock<std::mutex> lock(mMutex);
    for (;;)
    {
      mWake.wait(lock, [this] { return mStop || !mQueue.empty(); });
      if (mStop)
        return;
      const std::string id = std::move(mQueue.front());
      mQueue.pop_front();
      mCurrent = id;
      mBusy = true;
      mKeepCurrent = true;
      lock.unlock();
      std::unique_ptr<T> out = renderSafely(id);
      lock.lock();
      mBusy = false;
      if (out && (mKeepCurrent || wantedLocked(id)))
        storeLocked(id, std::move(out));
      mDone.notify_all();
    }
  }

  // A render that throws (out of memory) leaves no result rather than
  // ending the program on the worker; take() then hands back nullptr and
  // the caller renders it itself.
  std::unique_ptr<T> renderSafely(const std::string& id)
  {
    try
    {
      return mRender(id);
    }
    catch (...)
    {
      return nullptr;
    }
  }

  // Adds a finished result, evicting the oldest ones past the cap; one a
  // take() is waiting for always stays.
  void storeLocked(const std::string& id, std::unique_ptr<T> out)
  {
    mReady.emplace_back(id, std::move(out));
    for (auto it = mReady.begin(); mReady.size() > mKeepReady && it != mReady.end();)
      it = wantedLocked(it->first) ? std::next(it) : mReady.erase(it);
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
    return std::find_if(mReady.begin(), mReady.end(), [&](const Entry& e) { return e.first == id; });
  }
  typename std::vector<Entry>::const_iterator readyLocked(const std::string& id) const
  {
    return std::find_if(mReady.begin(), mReady.end(), [&](const Entry& e) { return e.first == id; });
  }

  const Render mRender;
  const std::size_t mKeepReady;
  mutable std::mutex mMutex;
  std::condition_variable mWake; // a request came in, or stop
  std::condition_variable mDone; // a render finished
  std::deque<std::string> mQueue; // oldest first
  std::vector<Entry> mReady;      // oldest first
  std::vector<std::string> mWanted; // ids take() is waiting for
  std::string mCurrent;           // what the worker renders, while mBusy
  bool mBusy = false;
  bool mKeepCurrent = true; // false once a drop() made mCurrent unwanted
  bool mStop = false;
  std::thread mWorker; // started in the constructor's body, joined in the destructor's
};

} // namespace gr
