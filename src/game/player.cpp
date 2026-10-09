// Player movement, shooting and death: a port of the Duke Nukem II player
// logic in RigelEngine's src/game_logic/player.cpp (GPL-2.0-or-later,
// Copyright (C) 2018 Nikolai Wuttke), adapted to Gunrunners' entity lists.
// The state machine, jump arc, ladder and pipe rules, firing rules and death
// sequence follow that file frame by frame.

#include "game/world.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>

namespace gr
{

namespace
{

constexpr int kMercyFrames = 30;      // RigelEngine: 40/30/20 by difficulty
constexpr int kInitialMercyFrames = 20;
// Gunrunners additions on top of Duke II: timed Turbo Mode and Virus.
constexpr int kEffectAboutToExpire = 45;
constexpr std::array<int, 8> kTurboJumpArc = {2, 2, 2, 2, 1, 1, 1, 0}; // 11 cells
constexpr std::array<int, 8> kVirusJumpArc = {2, 1, 1, 1, 0, 0, 0, 0};  // 5 cells
constexpr std::array<int, 8> kDuckJumpArc = {2, 1, 1, 1, 1, 0, 0, 0};   // 6 cells: Duck Rapids, for everyone
constexpr int kTemporaryItemFrames = 700;
constexpr int kItemAboutToExpire = 30;

constexpr std::array<int, 6> kDeathFlyUp{-2, -1, 0, 0, 1, 1};

// Where shots leave the gun, relative to the player position (bottom-left
// cell), per stance: Regular, Crouched, Up, Down, Jetpack.
constexpr int kShotOffsetRight[5][2] = {{3, -2}, {3, -1}, {2, -5}, {1, 1}, {1, 1}};
constexpr int kShotOffsetLeft[5][2] = {{-1, -2}, {-1, -1}, {0, -5}, {1, 1}, {1, 1}};

} // namespace

int Player::height() const
{
  if (state == PlayerState::Pipe)
    return 6;
  if (visual == PlayerVisual::Crouching)
    return 4;
  return 5;
}

CellBox Player::hitBox() const
{
  CellBox b = box();
  switch (visual)
  {
    case PlayerVisual::PullingLegsUp:
      b.h = 4;
      break;
    case PlayerVisual::Coiling:
      b.y += 1;
      b.h -= 1;
      break;
    case PlayerVisual::Somersault:
      b.y += 1;
      b.h = 4;
      break;
    default:
      break;
  }
  return b;
}

void World::updatePlayer(const PlayerInput& raw)
{
  auto& p = mPlayer;

  if (p.rapidFire > 0)
  {
    --p.rapidFire;
    if (p.rapidFire == kItemAboutToExpire)
      showMessage("RAPID FIRE IS RUNNING OUT");
  }

  if (p.turbo > 0)
  {
    --p.turbo;
    if (p.turbo == kEffectAboutToExpire)
      showMessage("TURBO IS RUNNING OUT");
    else if (p.turbo == 0)
      playSound(Sfx::EffectEnd);
  }
  if (p.virus > 0)
  {
    --p.virus;
    if (p.virus == 0)
    {
      showMessage("VIRUS CLEARED - BACK TO NORMAL");
      playSound(Sfx::EffectEnd);
    }
  }

  if (p.state == PlayerState::Dying)
  {
    updateDeathAnimation();
    return;
  }
  if (p.state == PlayerState::Teleporting)
    return;

  if (p.recoil > 0)
    --p.recoil;
  if (p.mercy > 0)
    --p.mercy;
  p.oddFrame = !p.oddFrame;

  // Conflicting directions cancel out, like in the original.
  PlayerInput in = mBeatStep ? beatStepInput(raw) : raw;
  if (mAutorun)
    in.left = in.right = false; // the duck does the paddling (world_sludge.cpp)
  if (in.left && in.right)
    in.left = in.right = false;
  if (in.up && in.down)
    in.up = in.down = false;
  const int mvX = in.left ? -1 : (in.right ? 1 : 0);
  const int mvY = in.up ? -1 : (in.down ? 1 : 0);

  if (mFreeFall)
  {
    updateFreeFall(mvX, mvY);
    return;
  }

  const int previousY = p.y;
  // Subwoofers: a jump started on the beat from a pad goes 2 cells higher,
  // also out of a pad's bump.
  if (!mPads.empty() && in.jump.triggered && onTheBeat())
  {
    const bool grounded = p.state == PlayerState::OnGround && padUnder(p.box()) >= 0;
    if (grounded || (mLaunch > 0 && mLaunchBump > 0))
    {
      const int risen = grounded ? 0 : mLaunchBump - mLaunch;
      startLaunch(std::max(1, jumpHeight() + 2 - risen));
      playSound(Sfx::Jump);
    }
  }
  if (mLaunch > 0)
    updateLaunch(mvX);
  else
  {
    updateLadderAttachment(mvX, mvY);
    updatePlayerMovement(mvX, mvY, in.jump, in.fire);
  }
  updateShooting(in.fire);

  if (p.visual == PlayerVisual::ClimbingLadder && p.y != previousY)
    ++p.climbFrame;

  // Looking up or crouching for a moment scrolls the view (manual scrolling).
  const bool looking = (p.visual == PlayerVisual::LookingUp || p.visual == PlayerVisual::Crouching) && !in.fire.pressed;
  mLookFrames = looking ? mLookFrames + 1 : 0;
  mManualScroll = mLookFrames > 4 ? (p.visual == PlayerVisual::LookingUp ? -1 : 1) : 0;

  if (p.y > mMap.height() + 3)
  {
    playSound(Sfx::Death);
    p.state = PlayerState::Dying;
    p.deathPhase = 3;
    p.frames = 0;
    p.hidden = true;
    ++mStats.deaths;
    if (!mSimulation && std::getenv("GR_PLANNER_DEBUG"))
      std::fprintf(stderr, "death (fell) at %d,%d frame %d\n", p.x, p.y, mStats.frames);
  }
}

void World::updatePlayerMovement(int mvX, int mvY, const Button& jumpButton, const Button& fireButton)
{
  auto& p = mPlayer;
  p.stance = Stance::Regular;

  if (jumpButton.triggered)
    p.jumpRequested = true;
  if (!jumpButton.pressed)
    p.jumpRequested = false;

  const bool shouldUseJetpack = canFire() && p.weapon == Weapon::Flame && mvY > 0 && fireButton.pressed;
  if (shouldUseJetpack && p.state != PlayerState::Jetpack)
  {
    p.state = PlayerState::Jetpack;
    p.frames = 0;
  }

  // Bonus rule "airjump": every press in mid-air starts a fresh jump.
  if (mAirJump && jumpButton.triggered &&
      ((p.state == PlayerState::Jumping && p.frames > 0) || p.state == PlayerState::Falling) &&
      !mMap.touchingCeiling(p.box()))
    jump();

  switch (p.state)
  {
    case PlayerState::OnGround:
    {
      if (mvY != 0)
      {
        p.stance = mvY < 0 ? Stance::Up : Stance::Crouched;
        setVisual(mvY < 0 ? PlayerVisual::LookingUp : PlayerVisual::Crouching);
        if (mvX != 0 && mvX != p.facing)
          switchOrientationWithPositionChange();
      }
      else
      {
        setVisual(PlayerVisual::Standing);
        if (mvX != 0)
        {
          if (mvX != p.facing)
          {
            // Turning around costs a frame.
            switchOrientation();
          }
          else
          {
            const int steps = horizontalSteps();
            if (steps == 0)
              setVisual(PlayerVisual::Walking); // infected: shuffle in place this frame
            for (int i = 0; i < steps; ++i)
            {
              if (mMap.moveHorizontallyWithStairStepping(p.x, p.y, Player::kWidth, p.height(), mvX) !=
                    MoveResult::Completed &&
                  !(wading() && wadeStep(mvX)))
                break;
              setVisual(PlayerVisual::Walking);
              ++p.walkFrame;
            }
          }
        }
      }

      // A held jump waits for 2 cells of headroom (so it fires the moment
      // a sign overhead goes dark), and still works for a moment after the
      // ground goes away ("coyote time"): beat signs are fair that way.
      const CellBox b = p.box();
      const bool headroom = !mMap.touchingCeiling(b) && !mMap.touchingCeiling({b.x, b.y - 1, b.w, b.h});
      // No jumping out of sludge: you wade, or step up onto the bank.
      if (p.jumpRequested && headroom && !(wading() && mFluids.size() > 0))
      {
        jump();
      }
      else if (!mMap.onSolidGround(p.box()) && !(!mFluids.empty() && buoyed()))
      {
        startFalling();
        if (p.state == PlayerState::Falling)
          p.coyote = 2;
      }
      break;
    }

    case PlayerState::Jumping:
      updateJumpMovement(mvX, jumpButton.pressed);
      break;

    case PlayerState::Falling:
    {
      if (p.coyote > 0)
      {
        --p.coyote;
        if (p.jumpRequested && !mMap.touchingCeiling(p.box()))
        {
          jump();
          break;
        }
      }
      if (!mFluids.empty() && buoyed())
      {
        // Sank into sludge: it holds you up.
        p.state = PlayerState::OnGround;
        setVisual(PlayerVisual::Standing);
        break;
      }
      const bool terminalVelocity = p.frames >= 2;
      if (terminalVelocity)
      {
        setVisual(PlayerVisual::FallingFull);
      }
      else
      {
        setVisual(PlayerVisual::Falling);
        ++p.frames;
      }
      updateHorizontalMovementInAir(mvX);
      bool attached = false;
      const auto result = moveVerticallyInAir(terminalVelocity ? 2 : 1, attached);
      if (!attached && result != MoveResult::Completed)
        landOnGround(terminalVelocity);
      break;
    }

    case PlayerState::Jetpack:
    {
      if (!shouldUseJetpack)
      {
        startFallingDelayed();
        break;
      }
      p.stance = Stance::Jetpack;
      setVisual(PlayerVisual::Jetpack);
      updateHorizontalMovementInAir(mvX);
      mMap.moveVertically(p.x, p.y, Player::kWidth, p.height(), -1);
      break;
    }

    case PlayerState::Recovering:
      p.state = PlayerState::OnGround;
      setVisual(PlayerVisual::Standing);
      playSound(Sfx::Land);
      break;

    case PlayerState::Ladder:
    {
      if (p.jumpRequested && !mMap.touchingCeiling(p.box()))
      {
        jumpFromLadder(mvX);
        break;
      }
      if (mvX != 0 && mvX != p.facing)
        switchOrientation();
      if (mvY != 0)
      {
        const CellBox b = p.box();
        const int attachX = b.left() + 1;
        const int nextY = mvY < 0 ? b.top() - 1 : b.bottom() + 1;
        if (mMap.ladder(attachX, nextY))
          mMap.moveVertically(p.x, p.y, Player::kWidth, p.height(), mvY);
        else if (mvY > 0)
          startFalling();
      }
      break;
    }

    case PlayerState::Pipe:
    {
      if (mvY <= 0 && p.jumpRequested && !mMap.touchingCeiling(p.box()))
      {
        p.y -= 1;
        jumpFromLadder(mvX);
        break;
      }

      setVisual(PlayerVisual::Hanging);
      if (mvY != 0)
      {
        p.stance = mvY < 0 ? Stance::Up : Stance::Down;
        setVisual(mvY < 0 ? PlayerVisual::PullingLegsUp : PlayerVisual::AimingDownOnPipe);
        if (mvX != 0 && mvX != p.facing)
          switchOrientationWithPositionChange();
        if (p.jumpRequested && mvY > 0)
          startFallingDelayed(); // down + jump lets go
      }
      else if (mvX != 0 && !fireButton.pressed)
      {
        if (mvX != p.facing)
        {
          switchOrientation();
        }
        else
        {
          const CellBox before = p.box();
          const int testX = mvX < 0 ? before.left() : before.right();
          if (mMap.moveHorizontally(p.x, p.y, Player::kWidth, p.height(), p.facing) != MoveResult::Failed)
          {
            if (mMap.climbable(testX, before.top()))
            {
              setVisual(PlayerVisual::MovingOnPipe);
              ++p.pipeFrame;
            }
            else
            {
              startFallingDelayed();
            }
          }
        }
      }
      break;
    }

    case PlayerState::Dying:
    case PlayerState::Teleporting:
      break;
  }
}

void World::updateLadderAttachment(int /*mvX*/, int mvY)
{
  auto& p = mPlayer;
  const bool canAttach = p.state != PlayerState::Ladder && p.state != PlayerState::Dying &&
    p.state != PlayerState::Teleporting && (p.state != PlayerState::Jumping || p.frames >= 3);
  if (!canAttach || mvY >= 0)
    return;

  const CellBox b = p.box();
  for (int i = 0; i < b.w; ++i)
  {
    if (!mMap.ladder(b.left() + i, b.top()))
      continue;
    p.state = PlayerState::Ladder;
    p.frames = 0;
    p.somersault = -1;
    setVisual(PlayerVisual::ClimbingLadder);
    // Snap the player's centre onto the ladder.
    p.x -= (b.left() + 1) - (b.left() + i);
    return;
  }
}

void World::updateHorizontalMovementInAir(int mvX)
{
  auto& p = mPlayer;
  if (mvX == 0)
    return;
  if (mvX != p.facing)
    switchOrientation();
  else
    for (int i = 0; i < horizontalSteps(); ++i)
      mMap.moveHorizontally(p.x, p.y, Player::kWidth, p.height(), mvX);
}

int World::horizontalSteps() const
{
  const auto& p = mPlayer;
  if (p.turbo > 0)
    return 2;
  if (p.virus > 0 || (!mFluids.empty() && wading()))
    return p.oddFrame ? 0 : 1; // infected, or wading through sludge
  return 1;
}

const std::array<int, 8>& World::jumpArc() const
{
  if (mAutorun)
    return kDuckJumpArc;
  if (mPlayer.turbo > 0)
    return kTurboJumpArc;
  if (mPlayer.virus > 0)
    return kVirusJumpArc;
  return mCharacter->jumpArc;
}

void World::updateJumpMovement(int mvX, bool jumpPressed)
{
  auto& p = mPlayer;
  const auto& arc = jumpArc();

  if (p.frames == 0)
    setVisual(PlayerVisual::Jumping);
  if (p.frames != 0 || p.fromLadder)
    updateHorizontalMovementInAir(mvX);

  if (p.frames >= int(arc.size()))
  {
    startFalling();
    return;
  }

  const int offset = arc[std::size_t(p.frames)];
  MoveResult outcome;
  if (p.frames > 0)
  {
    bool attached = false;
    outcome = moveVerticallyInAir(-offset, attached);
    if (attached)
      return;
  }
  else
  {
    outcome = mMap.moveVertically(p.x, p.y, Player::kWidth, p.height(), -offset);
  }

  if (outcome != MoveResult::Completed)
  {
    if (offset == 2 && outcome == MoveResult::MovedPartially)
    {
      p.frames = 3;
    }
    else
    {
      startFalling();
      return;
    }
  }

  // Now and then a running jump turns into a somersault.
  if (p.somersault >= 0)
  {
    ++p.somersault;
    if (p.somersault >= 8 || mvX == 0)
    {
      p.somersault = -1;
      setVisual(PlayerVisual::Jumping);
    }
  }
  if (p.frames == 1 && p.somersault < 0 && mvX != 0 && mLogicRng.next() % 6 == 0)
  {
    p.somersault = 0;
    setVisual(PlayerVisual::Somersault);
  }

  // On the third frame, a released jump button cuts the arc short.
  const bool isShortJump = p.frames == 2 && !jumpPressed;
  p.frames = isShortJump ? 6 : p.frames + 1;
}

MoveResult World::moveVerticallyInAir(int amount, bool& attached)
{
  auto& p = mPlayer;
  attached = false;
  if (amount == 0)
  {
    attached = tryAttachToClimbable();
    return MoveResult::Completed;
  }
  const int step = amount > 0 ? 1 : -1;
  for (int i = 0; i < std::abs(amount); ++i)
  {
    if (tryAttachToClimbable())
    {
      attached = true;
      break;
    }
    if (mMap.moveVertically(p.x, p.y, Player::kWidth, p.height(), step) != MoveResult::Completed)
      return i == 0 ? MoveResult::Failed : MoveResult::MovedPartially;
  }
  return MoveResult::Completed;
}

bool World::tryAttachToClimbable()
{
  auto& p = mPlayer;
  CellBox b = p.box();
  if (p.state == PlayerState::Jumping)
    --b.y;
  if (!mMap.climbable(b.left() + 1, b.top()))
    return false;
  p.state = PlayerState::Pipe;
  p.frames = 0;
  p.somersault = -1;
  setVisual(PlayerVisual::Hanging);
  p.y = b.top() + 5;
  playSound(Sfx::AttachClimbable);
  return true;
}

void World::updateShooting(const Button& fire)
{
  auto& p = mPlayer;
  if (p.weapon == Weapon::Proto)
  {
    updateProtoShooting(fire);
    return;
  }
  // Turbo fires every frame while held; the virus takes rapid fire away.
  const bool hasRapidFire = p.virus == 0 && (p.rapidFire > 0 || p.weapon == Weapon::Flame || p.turbo > 0);
  if (!canFire())
    return;
  if (fire.triggered || (fire.pressed && hasRapidFire && (!p.rapidFiredLastFrame || p.turbo > 0)))
    fireShot();
  if (fire.pressed && hasRapidFire)
    p.rapidFiredLastFrame = !p.rapidFiredLastFrame;
  else
    p.rapidFiredLastFrame = false;
}

bool World::canFire() const
{
  const auto& p = mPlayer;
  const bool blocked = p.state == PlayerState::Ladder || p.state == PlayerState::Dying ||
    p.state == PlayerState::Teleporting || p.visual == PlayerVisual::Coiling ||
    (p.state == PlayerState::Pipe && p.stance == Stance::Up);
  return !blocked;
}

void World::fireShot()
{
  auto& p = mPlayer;
  const int s = int(p.stance);
  const auto& off = p.facing > 0 ? kShotOffsetRight[s] : kShotOffsetLeft[s];
  int dx = p.facing, dy = 0;
  if (p.stance == Stance::Up)
  {
    dx = 0;
    dy = -1;
  }
  else if (p.stance == Stance::Down || p.stance == Stance::Jetpack)
  {
    dx = 0;
    dy = 1;
  }

  ShotKind kind = ShotKind::Normal;
  Sfx sound = Sfx::Shot;
  switch (p.weapon)
  {
    case Weapon::Laser:
      kind = ShotKind::Laser;
      sound = Sfx::LaserShot;
      break;
    case Weapon::Rocket:
      kind = ShotKind::Rocket;
      sound = Sfx::RocketShot;
      break;
    case Weapon::Flame:
      kind = ShotKind::Flame;
      sound = Sfx::FlameShot;
      break;
    case Weapon::Normal:
    case Weapon::Proto:
      break;
  }
  if (p.weapon == Weapon::Proto)
  {
    fireProto(p.x + off[0], p.y + off[1], dx, dy);
    sound = protoDef(p.proto).speed >= 3 ? Sfx::LaserShot : Sfx::Shot;
  }
  else
  {
    spawnProjectile(kind, p.x + off[0], p.y + off[1], dx, dy);
  }
  playSound(sound);
  sonarPing(p.x + off[0], p.y + off[1]);
  p.recoil = 1;
  p.muzzleTicks = 6;
  p.muzzleStance = p.stance;

  if (p.weapon != Weapon::Normal && --p.ammo <= 0)
  {
    p.ammo = 0;
    p.weapon = Weapon::Normal;
    p.proto = -1;
    showMessage("OUT OF AMMO - BACK TO THE BLASTER");
  }
}

void World::jump()
{
  auto& p = mPlayer;
  p.state = PlayerState::Jumping;
  p.frames = 0;
  p.fromLadder = false;
  p.somersault = -1;
  p.coyote = 0;
  setVisual(PlayerVisual::Coiling);
  playSound(Sfx::Jump);
  p.jumpRequested = false;
}

void World::jumpFromLadder(int mvX)
{
  auto& p = mPlayer;
  p.state = PlayerState::Jumping;
  p.frames = 0;
  p.fromLadder = true;
  p.somersault = -1;
  updateJumpMovement(mvX, true);
  if (p.state == PlayerState::Jumping)
    setVisual(PlayerVisual::Jumping);
  playSound(Sfx::Jump);
  p.jumpRequested = false;
}

void World::startFalling()
{
  auto& p = mPlayer;
  p.somersault = -1;
  if (mMap.onSolidGround(p.box()))
  {
    p.state = PlayerState::OnGround;
    setVisual(PlayerVisual::Standing);
    return;
  }
  p.state = PlayerState::Falling;
  p.frames = 0;
  setVisual(PlayerVisual::Falling);
  bool attached = false;
  moveVerticallyInAir(1, attached);
}

void World::startFallingDelayed()
{
  auto& p = mPlayer;
  p.state = PlayerState::Falling;
  p.frames = 0;
  p.somersault = -1;
  setVisual(PlayerVisual::Jumping);
}

void World::landOnGround(bool needRecoveryFrame)
{
  auto& p = mPlayer;
  p.somersault = -1;
  if (needRecoveryFrame)
  {
    p.state = PlayerState::Recovering;
    setVisual(PlayerVisual::Coiling);
    const Vec2 feet{(float(p.x) + 1.5f) * kCellSize, float(p.y + 1) * kCellSize};
    burst(feet, rgba(255, 255, 255, 150), rgba(200, 200, 220, 110), 6, 0.9f, false);
  }
  else
  {
    p.state = PlayerState::OnGround;
    setVisual(PlayerVisual::Standing);
  }
}

void World::switchOrientation()
{
  auto& p = mPlayer;
  p.facing = -p.facing;
  // Push the player out of a wall the turn would leave them stuck in.
  CellBox b = p.box();
  b.x -= p.facing;
  const bool stuck = p.facing < 0 ? mMap.touchingLeftWall(b) : mMap.touchingRightWall(b);
  if (stuck)
    p.x -= p.facing;
}

void World::switchOrientationWithPositionChange()
{
  // The original also shifts the position by a cell here to make up for
  // Duke's lopsided sprite. Our sprites are symmetric, so just turn.
  switchOrientation();
}

void World::hurtPlayer(int amount)
{
  auto& p = mPlayer;
  if (p.state == PlayerState::Dying || p.state == PlayerState::Teleporting || p.mercy > 0 || p.turbo > 0 || mGod)
    return;
  p.hp -= amount;
  mStats.tookDamage = true;
  if (!mSimulation && std::getenv("GR_PLANNER_DEBUG"))
  {
    int shots = 0;
    for (const auto& pr : mProjectiles)
      shots += pr.alive && pr.kind == ShotKind::Enemy && pr.box().intersects(p.box());
    int touching = 0;
    for (const auto& e : mEnemies)
      touching += e.alive && e.box().intersects(p.box());
    std::fprintf(stderr, "hurt at %d,%d frame %d hp %d: strikes %zu shots %d touching %d sweep %d ram %d pod %d\n", p.x,
      p.y, mStats.frames, p.hp, mStrikes.size(), shots, touching, mBoss.sweepX, mBoss.ram, mBoss.podX);
  }
  if (p.hp <= 0)
  {
    p.hp = 0;
    killPlayer();
    return;
  }
  p.mercy = kMercyFrames;
  playSound(Sfx::Hurt);
  const CellBox b = p.box();
  burst({(float(b.x) + 1.5f) * kCellSize, (float(b.y) + 2.0f) * kCellSize}, rgb(255, 80, 80), rgb(255, 255, 255), 10, 1.5f);
}

void World::killPlayer()
{
  auto& p = mPlayer;
  if (p.state == PlayerState::Dying)
    return;
  p.state = PlayerState::Dying;
  p.deathPhase = 0;
  p.frames = 0;
  p.somersault = -1;
  p.mercy = 0;
  setVisual(PlayerVisual::Dying);
  playSound(Sfx::Death);
  mCamera.shake(12, 2.0f);
  ++mStats.deaths;
  if (!mSimulation && std::getenv("GR_PLANNER_DEBUG"))
    std::fprintf(stderr, "death at %d,%d frame %d\n", p.x, p.y, mStats.frames);
}

void World::updateDeathAnimation()
{
  auto& p = mPlayer;
  if (p.y > mMap.height() + 3 && p.deathPhase != 3)
  {
    p.deathPhase = 3;
    p.frames = 0;
    p.hidden = true;
  }

  switch (p.deathPhase)
  {
    case 0: // flying up
      p.y += kDeathFlyUp[std::size_t(p.frames)];
      if (++p.frames >= int(kDeathFlyUp.size()))
      {
        p.deathPhase = 1;
        p.frames = 0;
      }
      break;
    case 1: // falling down
      if (mMap.moveVertically(p.x, p.y, Player::kWidth, p.height(), 2) != MoveResult::Completed)
      {
        p.deathPhase = 2;
        p.frames = 0;
      }
      break;
    case 2: // lying there, then exploding
      if (++p.frames == 10)
      {
        p.hidden = true;
        const Vec2 c{(float(p.x) + 1.5f) * kCellSize, (float(p.y) - 1.5f) * kCellSize};
        burst(c, rgb(255, 210, 80), rgb(255, 90, 40), 40, 3.2f);
        burst(c, mArt.characterColor[std::size_t(mCharacterIndex)], rgb(255, 255, 255), 20, 2.2f);
        flashAt(c, 120.0f, rgb(255, 170, 60), 24);
        playSound(Sfx::Explosion);
        mCamera.shake(14, 2.5f);
      }
      if (p.frames >= 35)
        respawnPlayer();
      break;
    default: // fell out of the map
      if (++p.frames >= 15)
        respawnPlayer();
      break;
  }
}

void World::respawnPlayer()
{
  auto& p = mPlayer;
  p.x = p.prevX = mRespawnX;
  p.y = p.prevY = mRespawnY;
  p.state = PlayerState::OnGround;
  p.visual = PlayerVisual::Standing;
  p.stance = Stance::Regular;
  p.frames = 0;
  p.deathPhase = 0;
  p.somersault = -1;
  p.hidden = false;
  mLaunch = mLaunchBump = 0;
  mBreakdance = false;
  p.hp = p.maxHp;
  p.mercy = kInitialMercyFrames;
  p.jumpRequested = false;
  mCamera.centerOn(cameraTarget(), mMap.width(), mMap.height());
  if (mTrain)
    resetTrain();
  mPaint.clear();
  if (mHunter.on || mBoss.on)
    resetBossCycle();
  showMessage("BACK IN ACTION");
}

void World::updatePlayerInteractions()
{
  auto& p = mPlayer;
  if (p.state == PlayerState::Dying || p.state == PlayerState::Teleporting)
    return;
  const CellBox hit = p.hitBox();

  if (mMap.overlapsHazard(hit))
    hurtPlayer(1);

  // Force fields: walking up to one with the access card switches it off.
  if (p.hasKey && mMap.forceFieldsOn())
  {
    CellBox reach = p.box();
    reach.x -= 1;
    reach.w += 2;
    bool touching = false;
    for (int y = reach.top(); y <= reach.bottom() && !touching; ++y)
      for (int x = reach.left(); x <= reach.right() && !touching; ++x)
        touching = mMap.forceField(x, y);
    if (touching)
    {
      mMap.disableForceFields();
      p.hasKey = false;
      mFieldFlash = 40;
      playSound(Sfx::ForceFieldOff);
      showMessage("ACCESS GRANTED - FORCE FIELD DOWN");
      addScore(2000, {(float(p.x) + 1.5f) * kCellSize, float(p.y - 6) * kCellSize});
    }
  }

  for (auto& cp : mCheckpoints)
  {
    if (cp.active || !cp.box().intersects(hit))
      continue;
    cp.active = true;
    mRespawnX = cp.x;
    mRespawnY = cp.y;
    playSound(Sfx::Checkpoint);
    showMessage("CHECKPOINT - YOU WILL RESPAWN HERE");
    flashAt({(float(cp.x) + 1.0f) * kCellSize, (float(cp.y) - 3.0f) * kCellSize}, 90.0f, mTheme.accentB, 30);
  }

  const CellBox exitZone{mLevel->exitTx * kCellsPerTile, (mLevel->exitTy + 1) * kCellsPerTile - 6, 2, 6};
  if (exitZone.intersects(p.box()) && p.state == PlayerState::OnGround && exitPowered())
  {
    p.state = PlayerState::Teleporting;
    setVisual(PlayerVisual::Standing);
    mState = WorldState::Exiting;
    mStateFrames = 0;
    playSound(Sfx::Teleport);
  }
}

} // namespace gr
