#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace gr
{

// A named track asked for before its background render was done (see
// Audio::playMusicNamed). The music playing goes on until it is, and the
// track then starts where it would be had it started on time: a seek the
// caller asked for meanwhile (the level's music clock, see seekMusic) plus
// the frames played since, or at its start without one (a cutscene's).
//
// Time is counted in frames the mixer has played, the clock the music
// itself runs on: the beat stays on the level's clock however long the
// render took, without the audio code knowing about levels.
//
// Plain state, no locking: Audio guards it (it is touched on the game
// thread only, the frame count comes from the mixer under Audio's lock).
class PendingMusic
{
public:
  // Waits for `id` from now on, from its start until a seek() says
  // otherwise. Asking for the one already pending changes nothing, as
  // playMusicNamed of the track playing doesn't restart it.
  void want(const std::string& id)
  {
    if (id == mId)
      return;
    mId = id;
    mSeeked = false;
  }
  // Nothing is pending any more (it started, or other music was asked for).
  void cancel()
  {
    mId.clear();
    mSeeked = false;
  }
  bool active() const { return !mId.empty(); }
  const std::string& id() const { return mId; }

  // The pending track is to be `frame` frames in as of `playedFrames`
  // (the mixer's count then). A later seek replaces it.
  void seek(std::uint64_t frame, std::uint64_t playedFrames)
  {
    mSeekFrame = frame;
    mSeekAt = playedFrames;
    mSeeked = true;
  }

  // The frame the track starts at if it arrives once the mixer has played
  // `playedFrames`: the seek moved on by what played since, wrapped to the
  // track's length as seekMusic wraps it. 0 without a seek, or for an
  // empty track.
  std::size_t startFrame(std::uint64_t playedFrames, std::size_t trackFrames) const
  {
    if (!mSeeked || trackFrames == 0)
      return 0;
    const std::uint64_t late = playedFrames >= mSeekAt ? playedFrames - mSeekAt : 0;
    return std::size_t((mSeekFrame + late) % trackFrames);
  }

private:
  std::string mId; // empty: nothing pending
  bool mSeeked = false;
  std::uint64_t mSeekFrame = 0;
  std::uint64_t mSeekAt = 0;
};

} // namespace gr
