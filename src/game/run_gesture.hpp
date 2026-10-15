#pragma once

namespace gr
{

// Running: a short tap of left or right, then the same way pressed again
// and held, makes the runner run that way until the direction is let go.
// Fed one 60 Hz tick at a time with the way held (-1, 0 or 1), so it works
// the same for keys, the d-pad, a stick and the touch stick.
class RunGesture
{
public:
  static constexpr int kTapTicks = 15; // the first press is at most this long (1/4 s)
  static constexpr int kGapTicks = 15; // and the second follows within this

  // Returns whether the runner is running this tick.
  bool update(int dir)
  {
    if (dir != 0 && dir == mHeld)
    {
      ++mHeldTicks;
    }
    else
    {
      // A short press that just ended is the tap; anything longer is a walk.
      if (mHeld != 0)
      {
        mTap = mHeldTicks <= kTapTicks ? mHeld : 0;
        mSinceTap = 0;
      }
      mRunning = dir != 0 && dir == mTap && mSinceTap <= kGapTicks;
      if (dir != 0)
        mTap = 0;
      mHeld = dir;
      mHeldTicks = dir != 0 ? 1 : 0;
    }
    if (mHeld == 0 && mTap != 0 && ++mSinceTap > kGapTicks)
      mTap = 0;
    return mRunning;
  }

  bool running() const { return mRunning; }
  void reset() { *this = RunGesture{}; }

private:
  int mHeld = 0;       // the way held now
  int mHeldTicks = 0;  // for this long
  int mTap = 0;        // the way of the tap that may start a run
  int mSinceTap = 0;   // ticks since it was let go
  bool mRunning = false;
};

} // namespace gr
