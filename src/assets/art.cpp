#include "assets/art.hpp"

#include "base/math.hpp"
#include "data/level.hpp"

#include <cmath>
#include <string>
#include <vector>

namespace td
{

namespace
{

using Rows = std::vector<std::string>;

constexpr Color kOutline = rgb(20, 16, 28);

Palette basePalette()
{
  Palette p{};
  p.fill(0);
  p['K'] = kOutline;
  p['W'] = rgb(255, 255, 255);
  return p;
}

// --- Characters --------------------------------------------------------------
// Sprites are 16x24, facing right, composed of head (rows 0-8), torso
// (rows 9-15) and legs (rows 16-23). Palette letters:
//   K outline  S/s skin  H/h hair  P/p outfit  Q/q pants  B boots
//   G/g gun  V accent/visor  E eye  R bandana

const Rows kHeadDash{
  "......K.K.K.....",
  ".....KHKHKHK....",
  "....KHHHHHHHK...",
  "...KHHHHHHHHHK..",
  "...KHHhSSSSSK...",
  "...KHhSSVVVVVK..",
  "....KSSSSSSSSK..",
  ".....KSSSsssK...",
  "......KKSSKK....",
};

const Rows kHeadRocco{
  "................",
  ".....KKKKKK.....",
  "....KRRRRRRK....",
  "...KRRRRRRRRK...",
  "..KRKHSSSSSSK...",
  "..KKKSSSSSESK...",
  ".....KSSSSSSSK..",
  ".....KHHHSHHK...",
  "......KHHHHK....",
};

const Rows kHeadNova{
  "......KKKKK.....",
  ".....KHHHHHK....",
  "....KHHHHHHHK...",
  "..KKHVVVVVVVHK..",
  ".KHHKHHSSSSSK...",
  ".KHHKHSSSSESK...",
  "..KHKKSSSSSSK...",
  "...KK.KSSssK....",
  "......KKSSK.....",
};

const Rows kTorsoNormal{
  "....KKPPPKK.....",
  "...KPPPPPPPK....",
  "...KPpPPPPSSKKKK",
  "...KPpPPPPKGGGgK",
  "...KPPPPPKKGKKK.",
  "....KPPPPK.K....",
  "....KQQQQQK.....",
};

const Rows kTorsoHeavy{
  "...KKPPPPPKK....",
  "..KPPPPPPPPPK...",
  "..KPpPPPPPSSKKKK",
  "..KPpPPPPKGGGGgK",
  "..KPPPPPPKGKKGK.",
  "...KPPPPPK.K....",
  "....KQQQQQK.....",
};

const Rows kTorsoSlim{
  ".....KPPPK......",
  "....KPVPPPK.....",
  "....KPVPPSSKKKKK",
  "....KPPPKKGGGgK.",
  "....KPPPK.KGK...",
  "....KPPPK.......",
  "....KVQQQK......",
};

const Rows kLegsStand{
  "....KQQKQQK.....",
  "....KQQKQQK.....",
  "....KQQKQQK.....",
  "....KqQKqQK.....",
  "....KQQKQQK.....",
  "....KBBKBBK.....",
  "....KBBKBBBK....",
  "....KKKKKKKK....",
};

const Rows kLegsRun1{
  "....KQQQQQK.....",
  "...KQQKKQQQK....",
  "...KQK..KQQK....",
  "..KQK....KQQK...",
  "..KqK.....KqQK..",
  ".KBBK.....KBBK..",
  ".KBBK.....KBBBK.",
  ".KKKK.....KKKKK.",
};

const Rows kLegsRun2{
  "....KQQQQQK.....",
  "....KQQKQQK.....",
  "....KQQKQQK.....",
  "...KQQK.KQK.....",
  "...KqK..KqQK....",
  "...KBBK.KBBK....",
  "...KBBBKKBBBK...",
  "...KKKKKKKKKK...",
};

const Rows kLegsJump{
  "....KQQQQQK.....",
  "...KQQQKQQQK....",
  "..KQQK..KQQQK...",
  "..KBBK...KQQK...",
  "..KBBBK..KBBK...",
  "..KKKK...KBBBK..",
  ".........KKKK...",
  "................",
};

Image composeCharacter(
  const Rows& head,
  const Rows& torso,
  const Rows& legs,
  const Palette& pal)
{
  Rows all;
  all.insert(all.end(), head.begin(), head.end());
  all.insert(all.end(), torso.begin(), torso.end());
  all.insert(all.end(), legs.begin(), legs.end());
  return imageFromAscii(all, pal);
}

CharacterArt buildCharacter(int index)
{
  Palette p = basePalette();
  p['E'] = kOutline;
  const Rows* head = &kHeadDash;
  const Rows* torso = &kTorsoNormal;

  switch (index)
  {
    case 0: // DASH
      p['H'] = rgb(255, 214, 64);
      p['h'] = rgb(220, 150, 30);
      p['S'] = rgb(241, 194, 150);
      p['s'] = rgb(205, 150, 110);
      p['V'] = rgb(64, 224, 255);
      p['P'] = rgb(230, 60, 50);
      p['p'] = rgb(160, 30, 40);
      p['Q'] = rgb(50, 70, 140);
      p['q'] = rgb(30, 40, 90);
      p['B'] = rgb(70, 45, 30);
      p['G'] = rgb(160, 170, 190);
      p['g'] = rgb(255, 230, 80);
      break;
    case 1: // ROCCO
      head = &kHeadRocco;
      torso = &kTorsoHeavy;
      p['H'] = rgb(40, 28, 22);
      p['S'] = rgb(160, 100, 64);
      p['s'] = rgb(120, 72, 44);
      p['R'] = rgb(220, 40, 40);
      p['P'] = rgb(100, 140, 60);
      p['p'] = rgb(64, 96, 40);
      p['Q'] = rgb(105, 105, 118);
      p['q'] = rgb(70, 70, 80);
      p['B'] = rgb(45, 40, 40);
      p['G'] = rgb(220, 130, 40);
      p['g'] = rgb(255, 220, 120);
      break;
    default: // NOVA
      head = &kHeadNova;
      torso = &kTorsoSlim;
      p['H'] = rgb(255, 80, 180);
      p['S'] = rgb(250, 212, 182);
      p['s'] = rgb(220, 160, 140);
      p['V'] = rgb(64, 255, 220);
      p['P'] = rgb(125, 60, 210);
      p['p'] = rgb(80, 35, 140);
      p['Q'] = rgb(125, 60, 210);
      p['q'] = rgb(80, 35, 140);
      p['B'] = rgb(235, 235, 245);
      p['G'] = rgb(120, 240, 255);
      p['g'] = rgb(255, 255, 255);
      break;
  }

  CharacterArt art;
  art.frames[kFrameIdle] = composeCharacter(*head, *torso, kLegsStand, p);
  art.frames[kFrameRun1] = composeCharacter(*head, *torso, kLegsRun1, p);
  art.frames[kFrameRun2] = composeCharacter(*head, *torso, kLegsRun2, p);
  art.frames[kFrameJump] = composeCharacter(*head, *torso, kLegsJump, p);
  return art;
}

// --- Enemies & items ---------------------------------------------------------

Palette enemyPalette(const Theme& t)
{
  Palette p = basePalette();
  p['G'] = t.enemyBody;
  p['g'] = t.enemyLight;
  p['d'] = t.enemyDark;
  p['R'] = t.enemyEye;
  return p;
}

const Rows kWalkerA{
  "....KKKKKKKK....",
  "...KgGGGGGGGK...",
  "...KGKKKKKKGK...",
  "...KGKRRRRKGK...",
  "...KGKKKKKKGK...",
  "...KGGGGGGGdK...",
  "....KKKKKKKK....",
  "..KKgGGGGGGdKK..",
  ".KgGGdddddddGGK.",
  ".KGKGGGGGGGGKGK.",
  ".KGKGddddddGKGK.",
  ".KKKGGGGGGGdKKK.",
  "....KGGKKGGK....",
  "....KGdK.KGdK...",
  "...KKKKK.KKKKK..",
  "................",
};

const Rows kWalkerB{
  "................",
  "....KKKKKKKK....",
  "...KgGGGGGGGK...",
  "...KGKKKKKKGK...",
  "...KGKRRRRKGK...",
  "...KGKKKKKKGK...",
  "...KGGGGGGGdK...",
  "..KKKKKKKKKKKK..",
  ".KgGGdddddddGGK.",
  ".KGKGGGGGGGGKGK.",
  ".KGKGddddddGKGK.",
  ".KKKGGGGGGGdKKK.",
  "....KGGKKGGK....",
  "...KGdK..KGdK...",
  "..KKKKK..KKKKK..",
  "................",
};

const Rows kFlyerA{
  ".KKKKK....KKKKK.",
  "....K......K....",
  "....K.KKKK.K....",
  "....KKggGGKK....",
  "...KgGGGGGGdK...",
  "..KgGKKKKKKGdK..",
  "..KGKRRRRRRKGK..",
  "..KGdKKKKKKddK..",
  "...KGGGGGGddK...",
  "....KKdKKdKK....",
  ".....K.KK.K.....",
  "....K..KK..K....",
  "................",
  "................",
  "................",
  "................",
};

const Rows kFlyerB{
  "...KKKK..KKKK...",
  "....K......K....",
  "....K.KKKK.K....",
  "....KKggGGKK....",
  "...KgGGGGGGdK...",
  "..KgGKKKKKKGdK..",
  "..KGKRRRRRRKGK..",
  "..KGdKKKKKKddK..",
  "...KGGGGGGddK...",
  "....KKdKKdKK....",
  "....K..KK..K....",
  ".....K.KK.K.....",
  "................",
  "................",
  "................",
  "................",
};

const Rows kTurret{
  "................",
  "................",
  "................",
  "......KKKK......",
  "....KKgGGGKK....",
  "...KgGRRRRGdK...",
  "...KGGGGGGGdKKKK",
  "...KGGGGGGGGGGgK",
  "...KGGGGGGGdKKKK",
  "..KKKKKKKKKKKK..",
  "..KggggggggggK..",
  ".KGGGGGGGGGGGGK.",
  ".KGdGdGdGdGdGdK.",
  ".KGGGGGGGGGGGGK.",
  ".KKKKKKKKKKKKKK.",
  "................",
};

const Rows kGem{
  "..KKKK..",
  ".KWAAAK.",
  "KWAAAAaK",
  "KAAAAAaK",
  ".KAAAaK.",
  "..KAaK..",
  "...KK...",
  "........",
};

const Rows kHealth{
  ".KK.KK..",
  "KRRKRRK.",
  "KRWRRRK.",
  "KRRRRRK.",
  ".KRRRK..",
  "..KRK...",
  "...K....",
  "........",
};

// --- Tiles -------------------------------------------------------------------

void bevel(Image& img, Color light, Color dark)
{
  for (int i = 0; i < img.w; ++i)
  {
    img.set(i, 0, light);
    img.set(i, img.h - 1, dark);
  }
  for (int i = 0; i < img.h; ++i)
  {
    img.set(0, i, light);
    img.set(img.w - 1, i, dark);
  }
}

Image buildSolid(const Theme& t, int variant)
{
  Image img(kTileSize, kTileSize, t.rock);
  Rng rng(std::uint32_t(variant * 7919 + int(t.id) * 31 + 1));
  switch (t.id)
  {
    case ThemeId::NeonOverdrive:
      bevel(img, t.rockLight, t.rockDark);
      img.fill(2, 7, 12, 1, t.rockDark);
      if (variant == 1)
      {
        img.fill(3, 2, 3, 3, withAlpha(t.accentA, 200));
        img.fill(9, 9, 3, 3, withAlpha(t.accentA, 140));
      }
      else if (variant == 2)
      {
        img.fill(4, 10, 8, 2, withAlpha(t.trim, 90));
      }
      else if (variant == 3)
      {
        img.fill(10, 2, 3, 3, withAlpha(t.accentB, 160));
      }
      break;
    case ThemeId::TempleOfTurbo:
      // two rows of offset bricks
      for (int y = 0; y < 16; ++y)
      {
        for (int x = 0; x < 16; ++x)
        {
          const bool mortarRow = (y == 7 || y == 15);
          const int off = (y < 8) ? 0 : 4;
          const bool mortarCol = ((x + off) % 8) == 7;
          if (mortarRow || mortarCol)
            img.set(x, y, t.rockDark);
          else if (rng.uniform() < 0.12f)
            img.set(x, y, t.rockLight);
        }
      }
      if (variant == 2)
      {
        img.fill(1, 1, 4, 2, withAlpha(t.trim, 200));
        img.fill(2, 3, 2, 1, withAlpha(t.trim, 160));
      }
      if (variant == 3)
        img.fill(9, 9, 4, 3, withAlpha(t.rockDark, 160));
      break;
    case ThemeId::StationZero:
      bevel(img, t.rockLight, t.rockDark);
      img.set(2, 2, t.rockLight);
      img.set(13, 2, t.rockLight);
      img.set(2, 13, t.rockLight);
      img.set(13, 13, t.rockLight);
      if (variant == 1)
        for (int y = 4; y < 12; y += 2)
          img.fill(4, y, 8, 1, t.rockDark);
      if (variant == 2)
      {
        img.fill(5, 5, 6, 6, t.rockDark);
        img.fill(6, 6, 4, 4, withAlpha(t.accentB, 150));
      }
      if (variant == 3)
        img.fill(0, 11, 16, 2, withAlpha(t.rockLight, 110));
      break;
  }
  return img;
}

Image buildSolidTop(const Theme& t)
{
  Image img(kTileSize, kTileSize, 0);
  switch (t.id)
  {
    case ThemeId::NeonOverdrive:
      img.fill(0, 0, 16, 2, t.trimGlow);
      img.fill(0, 2, 16, 1, t.trim);
      img.fill(0, 3, 16, 2, withAlpha(t.trim, 70));
      break;
    case ThemeId::TempleOfTurbo:
      img.fill(0, 0, 16, 3, t.trim);
      img.fill(0, 0, 16, 1, t.trimGlow);
      for (int x = 0; x < 16; ++x)
      {
        const int len = int(hash2(x, 3) % 4);
        img.fill(x, 3, 1, len, withAlpha(t.trim, 220));
      }
      break;
    case ThemeId::StationZero:
      for (int x = 0; x < 16; ++x)
        for (int y = 0; y < 3; ++y)
          img.set(x, y, ((x + y) / 3) % 2 == 0 ? t.trim : kOutline);
      // a little frost on top
      img.fill(0, 0, 16, 1, withAlpha(rgb(230, 250, 255), 220));
      break;
  }
  return img;
}

Image buildPlatform(const Theme& t)
{
  Image img(kTileSize, kTileSize, 0);
  switch (t.id)
  {
    case ThemeId::NeonOverdrive:
      img.fill(0, 0, 16, 5, t.platformDark);
      img.fill(0, 0, 16, 2, t.platform);
      img.fill(0, 0, 16, 1, rgb(255, 200, 240));
      img.fill(3, 5, 2, 3, withAlpha(t.platformDark, 200));
      img.fill(11, 5, 2, 3, withAlpha(t.platformDark, 200));
      break;
    case ThemeId::TempleOfTurbo:
      img.fill(0, 0, 16, 5, t.platform);
      img.fill(0, 4, 16, 1, t.platformDark);
      img.fill(0, 0, 16, 1, scaleColor(t.platform, 1.3f));
      img.fill(5, 2, 4, 1, t.platformDark);
      img.fill(0, 0, 1, 5, t.platformDark);
      img.fill(15, 0, 1, 5, t.platformDark);
      break;
    case ThemeId::StationZero:
      img.fill(0, 0, 16, 4, t.platformDark);
      for (int x = 1; x < 16; x += 3)
        img.fill(x, 1, 2, 2, t.platform);
      img.fill(0, 0, 16, 1, rgb(230, 250, 255));
      break;
  }
  return img;
}

Image buildSpikes(const Theme& t)
{
  Image img(kTileSize, kTileSize, 0);
  for (int s = 0; s < 4; ++s)
  {
    const int baseX = s * 4;
    for (int y = 0; y < 10; ++y)
    {
      const int half = (y * 2) / 10; // 0..1 width growth
      const int yy = 6 + y;
      for (int x = 1 - half; x <= 2 + half; ++x)
        img.set(baseX + x, yy, x <= 1 ? t.hazardLight : t.hazard);
    }
    img.set(baseX + 1, 6, t.hazardLight);
  }
  img.fill(0, 15, 16, 1, scaleColor(t.hazard, 0.5f));
  return img;
}

Image buildCrate(const Theme& t)
{
  Image img(kTileSize, kTileSize, t.platform);
  bevel(img, scaleColor(t.platform, 1.35f), t.platformDark);
  img.fill(1, 1, 14, 1, scaleColor(t.platform, 1.2f));
  for (int i = 2; i < 14; ++i)
  {
    img.set(i, i, t.platformDark);
    img.set(15 - i, i, t.platformDark);
  }
  img.fill(0, 0, 16, 1, kOutline);
  img.fill(0, 15, 16, 1, kOutline);
  img.fill(0, 0, 1, 16, kOutline);
  img.fill(15, 0, 1, 16, kOutline);
  return img;
}

// --- Backdrops ---------------------------------------------------------------

constexpr int kBackW = 480;
constexpr int kBackH = 180;

void wrapFill(Image& img, int x, int y, int w, int h, Color c)
{
  for (int i = 0; i < w; ++i)
  {
    const int xx = ((x + i) % img.w + img.w) % img.w;
    img.fill(xx, y, 1, h, c);
  }
}

Image buildBackFar(const Theme& t)
{
  Image img(kBackW, kBackH, 0);
  Rng rng(1234u + std::uint32_t(t.id));
  switch (t.id)
  {
    case ThemeId::NeonOverdrive:
    {
      int x = 0;
      while (x < kBackW)
      {
        const int w = rng.irange(18, 44);
        const int h = rng.irange(50, 120);
        wrapFill(img, x, kBackH - h, w, h, t.farLayer);
        for (int wy = kBackH - h + 4; wy < kBackH - 4; wy += 6)
          for (int wx = x + 3; wx < x + w - 3; wx += 5)
            if (rng.uniform() < 0.28f)
              wrapFill(img, wx, wy, 2, 3, withAlpha(rng.uniform() < 0.5f ? t.accentA : t.accentB, 150));
        x += w + rng.irange(0, 6);
      }
      break;
    }
    case ThemeId::TempleOfTurbo:
    {
      for (int x = 0; x < kBackW; ++x)
      {
        const float fx = float(x) / float(kBackW) * 6.2831853f;
        const int h = 70 + int(22.0f * std::sin(fx * 2.0f) + 12.0f * std::sin(fx * 5.0f + 1.0f));
        img.fill(x, kBackH - h, 1, h, t.farLayer);
      }
      // stepped pyramid
      const int px = 300, base = kBackH - 60;
      for (int step = 0; step < 6; ++step)
      {
        const int w = 120 - step * 18;
        img.fill(px - w / 2, base - step * 10, w, 10, scaleColor(t.farLayer, 0.85f));
      }
      img.fill(px - 8, base - 70, 16, 10, scaleColor(t.farLayer, 0.85f));
      img.fill(px - 3, base - 66, 6, 6, withAlpha(t.accentA, 160));
      break;
    }
    case ThemeId::StationZero:
    {
      int x = 0;
      while (x < kBackW)
      {
        const int w = rng.irange(30, 70);
        const int h = rng.irange(30, 80);
        const int y = kBackH - h - rng.irange(10, 40);
        wrapFill(img, x, y, w, h, t.farLayer);
        wrapFill(img, x + w / 2 - 1, y + h, 3, kBackH - y - h, t.farLayer);
        for (int lx = x + 4; lx < x + w - 4; lx += 8)
          wrapFill(img, lx, y + 4, 3, 2, withAlpha(t.accentB, 170));
        x += w + rng.irange(20, 60);
      }
      break;
    }
  }
  return img;
}

Image buildBackNear(const Theme& t)
{
  Image img(kBackW, kBackH, 0);
  Rng rng(9876u + std::uint32_t(t.id));
  switch (t.id)
  {
    case ThemeId::NeonOverdrive:
    {
      int x = 0;
      while (x < kBackW)
      {
        const int w = rng.irange(30, 60);
        const int h = rng.irange(40, 90);
        wrapFill(img, x, kBackH - h, w, h, t.nearLayer);
        if (rng.uniform() < 0.7f)
        {
          const Color sign = rng.uniform() < 0.5f ? t.platform : t.trim;
          const int sx = x + 5, sy = kBackH - h + 8, sw = w - 10;
          wrapFill(img, sx, sy, sw, 1, sign);
          wrapFill(img, sx, sy + 7, sw, 1, sign);
          wrapFill(img, sx, sy, 1, 8, sign);
          wrapFill(img, sx + sw - 1, sy, 1, 8, sign);
          wrapFill(img, sx + 3, sy + 3, sw - 6, 2, withAlpha(sign, 140));
        }
        x += w + rng.irange(4, 30);
      }
      break;
    }
    case ThemeId::TempleOfTurbo:
    {
      int x = 0;
      while (x < kBackW)
      {
        const int trunkH = rng.irange(70, 130);
        wrapFill(img, x + 8, kBackH - trunkH, 6, trunkH, t.nearLayer);
        // canopy blobs
        for (int b = 0; b < 6; ++b)
        {
          const int cx = x + rng.irange(-6, 26);
          const int cy = kBackH - trunkH + rng.irange(-12, 8);
          const int r = rng.irange(8, 14);
          for (int yy = -r; yy <= r; ++yy)
          {
            const int half = int(std::sqrt(float(r * r - yy * yy)));
            wrapFill(img, cx - half, cy + yy, half * 2, 1, t.nearLayer);
          }
        }
        // vines
        const int vx = x + rng.irange(0, 24);
        wrapFill(img, vx, kBackH - trunkH, 1, rng.irange(20, 50), scaleColor(t.trim, 0.6f));
        x += rng.irange(40, 80);
      }
      break;
    }
    case ThemeId::StationZero:
    {
      for (int x = 0; x < kBackW; x += 64)
      {
        // truss tower
        wrapFill(img, x, 40, 3, kBackH - 40, t.nearLayer);
        wrapFill(img, x + 21, 40, 3, kBackH - 40, t.nearLayer);
        for (int y = 40; y < kBackH; y += 20)
        {
          wrapFill(img, x, y, 24, 2, t.nearLayer);
          for (int i = 0; i < 20; ++i)
          {
            wrapFill(img, x + i + 2, y + i, 1, 1, t.nearLayer);
            wrapFill(img, x + 21 - i, y + i, 1, 1, t.nearLayer);
          }
        }
        wrapFill(img, x + 10, 36, 4, 4, withAlpha(t.accentA, 220));
      }
      break;
    }
  }
  return img;
}

} // namespace


Art Art::build(const Theme& theme)
{
  Art art;
  for (int i = 0; i < 3; ++i)
    art.characters[std::size_t(i)] = buildCharacter(i);

  const Palette ep = enemyPalette(theme);
  art.walker[0] = imageFromAscii(kWalkerA, ep);
  art.walker[1] = imageFromAscii(kWalkerB, ep);
  art.flyer[0] = imageFromAscii(kFlyerA, ep);
  art.flyer[1] = imageFromAscii(kFlyerB, ep);
  art.turret = imageFromAscii(kTurret, ep);

  const std::array<Color, 4> gemColors{
    theme.accentA, theme.accentB, rgb(120, 255, 120), rgb(255, 120, 255)};
  for (std::size_t i = 0; i < 4; ++i)
  {
    Palette gp = basePalette();
    gp['A'] = gemColors[i];
    gp['a'] = scaleColor(gemColors[i], 0.6f);
    art.gem[i] = imageFromAscii(kGem, gp);
  }
  Palette hp = basePalette();
  hp['R'] = rgb(255, 50, 70);
  art.health = imageFromAscii(kHealth, hp);

  for (int v = 0; v < 4; ++v)
    art.solid[std::size_t(v)] = buildSolid(theme, v);
  art.solidTop = buildSolidTop(theme);
  art.platform = buildPlatform(theme);
  art.spikes = buildSpikes(theme);
  art.crate = buildCrate(theme);

  // Player projectiles: blaster bolt, scatter pellet, rapid needle.
  art.playerBullet[0] = Image(6, 3, rgb(255, 240, 120));
  art.playerBullet[0].fill(0, 0, 6, 1, withAlpha(rgb(255, 160, 40), 200));
  art.playerBullet[0].fill(0, 2, 6, 1, withAlpha(rgb(255, 160, 40), 200));
  art.playerBullet[1] = Image(4, 4, rgb(255, 170, 60));
  art.playerBullet[1].fill(1, 1, 2, 2, rgb(255, 255, 220));
  art.playerBullet[2] = Image(7, 2, rgb(120, 255, 255));
  art.playerBullet[2].fill(4, 0, 3, 2, rgb(255, 255, 255));
  art.enemyBullet = Image(5, 5, 0);
  art.enemyBullet.fill(1, 0, 3, 5, theme.enemyEye);
  art.enemyBullet.fill(0, 1, 5, 3, theme.enemyEye);
  art.enemyBullet.fill(1, 1, 3, 3, rgb(255, 255, 255));

  art.backFar = buildBackFar(theme);
  art.backNear = buildBackNear(theme);
  return art;
}


void drawSky(Canvas& c, const Theme& t, int frame)
{
  const int h = c.h;
  for (int y = 0; y < h; ++y)
  {
    const float f = float(y) / float(h - 1);
    const Color col = f < 0.55f ? lerpColor(t.skyTop, t.skyMid, f / 0.55f)
                                : lerpColor(t.skyMid, t.skyBottom, (f - 0.55f) / 0.45f);
    c.fill(0, y, c.w, 1, col);
  }

  switch (t.id)
  {
    case ThemeId::NeonOverdrive:
    {
      // striped synthwave sun
      const int cx = 220, cy = 92, r = 44;
      for (int y = -r; y <= r; ++y)
      {
        if (y > 0 && ((y / 4) % 2 == 1) && y > 8)
          continue;
        const int half = int(std::sqrt(float(r * r - y * y)));
        const Color col = lerpColor(rgb(255, 240, 90), rgb(255, 60, 160), float(y + r) / float(2 * r));
        c.fill(cx - half, cy + y, half * 2, 1, col);
      }
      for (int i = 0; i < 40; ++i)
      {
        const int sx = int(hash2(i, 1) % std::uint32_t(c.w));
        const int sy = int(hash2(i, 2) % 60u);
        if ((hash2(i, frame / 20) & 7u) != 0)
          c.set(sx, sy, rgb(255, 220, 255));
      }
      break;
    }
    case ThemeId::TempleOfTurbo:
    {
      const int cx = 80, cy = 40;
      for (int r = 36; r > 0; r -= 4)
      {
        const Color col = withAlpha(rgb(255, 250, 200), 30 + (36 - r) * 4);
        for (int y = -r; y <= r; ++y)
        {
          const int half = int(std::sqrt(float(r * r - y * y)));
          c.fill(cx - half, cy + y, half * 2, 1, col);
        }
      }
      break;
    }
    case ThemeId::StationZero:
    {
      for (int i = 0; i < 90; ++i)
      {
        const int sx = int(hash2(i, 11) % std::uint32_t(c.w));
        const int sy = int(hash2(i, 12) % std::uint32_t(h));
        const bool twinkle = (hash2(i, frame / 15) & 15u) == 0;
        c.set(sx, sy, twinkle ? rgb(120, 140, 200) : rgb(230, 240, 255));
      }
      // ringed planet
      const int cx = 250, cy = 46, r = 26;
      for (int y = -r; y <= r; ++y)
      {
        const int half = int(std::sqrt(float(r * r - y * y)));
        for (int x = -half; x < half; ++x)
        {
          const float light = 0.55f + 0.45f * float(-x - y) / float(2 * r);
          c.set(cx + x, cy + y, scaleColor(rgb(120, 170, 255), std::max(0.25f, light)));
        }
      }
      for (int x = -46; x <= 46; ++x)
      {
        const int y = int(float(x) * 0.22f);
        if (std::abs(x) > 24 || y > 0)
          c.fill(cx + x, cy + y, 1, 2, withAlpha(rgb(220, 230, 255), 190));
      }
      break;
    }
  }
}

void drawBackdrop(Canvas& c, const Art& art, float camX, float camY, float baseCamY)
{
  auto layer = [&](const Image& img, float parallaxX, float parallaxY) {
    const int ox = int(std::floor(camX * parallaxX)) % img.w;
    const int oy = int((camY - baseCamY) * parallaxY);
    for (int x = -ox; x < c.w; x += img.w)
      c.blit(img, x, c.h - img.h - oy);
  };
  layer(art.backFar, 0.15f, 0.08f);
  layer(art.backNear, 0.35f, 0.2f);
}

void drawDecoration(Canvas& c, const Theme& t, int x, int y, int frame)
{
  switch (t.id)
  {
    case ThemeId::NeonOverdrive:
    {
      // flickering neon arrow sign on a pole
      const bool on = (hash2(x, frame / 6) % 11u) != 0;
      const Color col = on ? t.platform : scaleColor(t.platform, 0.35f);
      c.fill(x + 7, y + 8, 2, 8, kOutline);
      c.fill(x, y, 16, 9, kOutline);
      c.frameRect(x + 1, y + 1, 14, 7, col);
      c.fill(x + 4, y + 4, 6, 1, col);
      c.fill(x + 9, y + 3, 1, 3, col);
      c.fill(x + 10, y + 4, 1, 1, col);
      if (on)
        c.fillRect(x - 2, y - 2, 20, 13, withAlpha(t.platform, 28));
      break;
    }
    case ThemeId::TempleOfTurbo:
    {
      // torch with flickering flame
      c.fill(x + 6, y + 8, 4, 8, rgb(90, 55, 30));
      c.fill(x + 5, y + 7, 6, 2, rgb(60, 40, 20));
      const int flick = int(hash2(x, frame / 4) % 3u);
      c.fill(x + 5, y + 2 + flick, 6, 5 - flick, rgb(255, 120, 30));
      c.fill(x + 6, y + 3 + flick, 4, 3 - flick / 2, rgb(255, 220, 80));
      c.fillRect(x - 4, y - 4, 24, 18, withAlpha(rgb(255, 170, 60), 30));
      break;
    }
    case ThemeId::StationZero:
    {
      // rotating warning beacon
      c.fill(x + 5, y + 10, 6, 6, t.rockDark);
      c.fill(x + 6, y + 6, 4, 4, t.accentA);
      const float a = float(frame) * 0.15f;
      const int bx = int(std::cos(a) * 9.0f);
      if (std::sin(a) > -0.2f)
        c.fillRect(x + 8 + std::min(0, bx), y + 6, std::abs(bx) + 1, 4, withAlpha(t.accentA, 120));
      break;
    }
  }
}

void drawExit(Canvas& c, const Theme& t, int x, int y, int frame)
{
  // y is the top of a 16x32 teleporter
  c.fill(x - 2, y + 28, 20, 4, kOutline);
  c.fill(x - 1, y + 28, 18, 2, t.accentA);
  c.fill(x - 2, y, 20, 3, kOutline);
  c.fill(x - 1, y + 1, 18, 1, t.accentA);
  for (int i = 0; i < 25; ++i)
  {
    const float wave = 0.5f + 0.5f * std::sin(float(i + frame / 2) * 0.6f);
    c.fillRect(x + 1, y + 3 + i, 14, 1, withAlpha(t.accentB, 50 + int(wave * 120.0f)));
  }
  const int sparkY = y + 27 - (frame / 2) % 24;
  c.fill(x + 3 + (frame / 3) % 9, sparkY, 2, 2, rgb(255, 255, 255));
  c.drawTextCentered("EXIT", x + 8, y - 10, t.accentA, 1, kOutline);
}

} // namespace td
