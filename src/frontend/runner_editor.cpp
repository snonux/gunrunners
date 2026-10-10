#include "frontend/runner_editor.hpp"

#include <algorithm>
#include <cstdio>
#include <random>

namespace gr
{

namespace
{

constexpr Color kInk = rgb(16, 12, 26);
constexpr const char* kKeys = "ABCDEFGHIJKLMNOPQRSTUVWXYZ-.!'0123456789";
constexpr int kKeyCols = 10;
constexpr int kKeyChars = 40;
constexpr int kKeySpace = 40, kKeyDelete = 41, kKeyDone = 42;
constexpr float kRowY = 84.0f;
constexpr float kRowH = 28.0f;
const Weapon kGuns[4] = {Weapon::Normal, Weapon::Laser, Weapon::Rocket, Weapon::Flame};

int wrap(int v, int n) { return ((v % n) + n) % n; }

int jumpCells(const CharacterDef& d)
{
  int cells = 0;
  for (int v : d.jumpArc)
    cells += v;
  return cells;
}

std::mt19937& rng()
{
  static std::mt19937 g{std::random_device{}()};
  return g;
}

int roll(int n) { return std::uniform_int_distribution<int>(0, n - 1)(rng()); }

} // namespace

RunnerEditor::RunnerEditor(const Renderer& renderer, const CharacterDef& start, bool isNew)
  : mRenderer(renderer)
  , mId(isNew ? newRunnerId() : start.id)
  , mIsNew(isNew)
  , mName(start.name)
  , mParts(start.parts)
  , mGun(start.startWeapon == Weapon::Proto ? Weapon::Normal : start.startWeapon)
  , mPips{std::clamp(start.healthPips, kMinPips[0], kMaxPips), std::clamp(start.jumpPips, kMinPips[1], kMaxPips),
      std::clamp(start.powerPips, kMinPips[2], kMaxPips)}
{
  // A built-in runner can spend more than a custom one: trim the extra.
  for (int s = 2; s >= 0; --s)
    while (mPips[s] > kMinPips[s] && runnerPointsLeft(mPips[0], mPips[1], mPips[2]) < 0)
      --mPips[s];
  mPanel = makePanel(renderer, 450, 590, rgba(8, 6, 22, 215), rgba(255, 255, 255, 70), 26);
  mListPanel = makePanel(renderer, 770, 590, rgba(8, 6, 22, 215), rgba(255, 255, 255, 70), 26);
  mKeysPanel = makePanel(renderer, 780, 470, rgba(8, 6, 22, 245), rgba(255, 255, 255, 110), 26);
}

CharacterDef RunnerEditor::runner() const
{
  CharacterDef d = makeCustomRunner(mId, mName, mParts, mGun, mPips[0], mPips[1], mPips[2]);
  d.transient = false;
  return d;
}

std::vector<RunnerEditor::Row> RunnerEditor::rows() const
{
  std::vector<Row> out = {Row::Name, Row::Body, Row::Build, Row::Hair, Row::Face, Row::Outfit, Row::Arms, Row::Skin,
    Row::HairColor, Row::Top, Row::Pants, Row::Boots, Row::Glow, Row::Gun, Row::Health, Row::Jump, Row::Power,
    Row::Randomize, Row::Save};
  if (!mIsNew)
    out.push_back(Row::Delete);
  return out;
}

void RunnerEditor::changePips(int stat, int step)
{
  int& v = mPips[stat];
  const int next = v + step;
  if (next < kMinPips[stat] || next > kMaxPips)
  {
    // OK on a full stat wraps it back to the minimum.
    if (step > 0 && next > kMaxPips)
      v = kMinPips[stat];
    return;
  }
  if (step > 0 && runnerPointsLeft(mPips[0], mPips[1], mPips[2]) <= 0)
    return;
  v = next;
}

void RunnerEditor::change(Row row, int step)
{
  auto& p = mParts;
  switch (row)
  {
    case Row::Body:
      p.body = wrap(p.body + step, 2);
      p.hair = std::min(p.hair, hairStyleCount(p.body) - 1);
      p.face = std::min(p.face, faceCount(p.body) - 1);
      p.skin = std::min(p.skin, skinToneCount(p.body) - 1);
      break;
    case Row::Build: p.build = wrap(p.build + step, 3); break;
    case Row::Hair: p.hair = wrap(p.hair + step, hairStyleCount(p.body)); break;
    case Row::Face: p.face = wrap(p.face + step, faceCount(p.body)); break;
    case Row::Outfit: p.outfit = wrap(p.outfit + step, outfitCount()); break;
    case Row::Arms: p.sleeves = wrap(p.sleeves + step, 2); break;
    case Row::Skin: p.skin = wrap(p.skin + step, skinToneCount(p.body)); break;
    case Row::HairColor: p.hairColor = wrap(p.hairColor + step, paletteSize()); break;
    case Row::Top: p.top = wrap(p.top + step, paletteSize()); break;
    case Row::Pants: p.pants = wrap(p.pants + step, paletteSize()); break;
    case Row::Boots: p.boots = wrap(p.boots + step, paletteSize()); break;
    case Row::Glow: p.glow = wrap(p.glow + step, paletteSize()); break;
    case Row::Gun:
    {
      int i = 0;
      while (i < 4 && kGuns[i] != mGun)
        ++i;
      mGun = kGuns[wrap(i + step, 4)];
      break;
    }
    case Row::Health: changePips(0, step); break;
    case Row::Jump: changePips(1, step); break;
    case Row::Power: changePips(2, step); break;
    default: break;
  }
}

void RunnerEditor::randomize()
{
  auto& p = mParts;
  p.body = roll(4) == 0 ? 1 : 0;
  p.build = roll(3);
  p.hair = roll(hairStyleCount(p.body));
  p.face = roll(faceCount(p.body));
  p.outfit = roll(outfitCount());
  p.sleeves = roll(2);
  p.skin = roll(skinToneCount(p.body));
  p.hairColor = roll(paletteSize());
  p.top = roll(paletteSize());
  p.pants = roll(paletteSize());
  p.boots = roll(paletteSize());
  p.glow = roll(paletteSize());
  mGun = kGuns[roll(4)];
  // Spend the whole budget, one point at a time on a random stat.
  mPips[0] = kMinPips[0];
  mPips[1] = kMinPips[1];
  mPips[2] = kMinPips[2];
  while (runnerPointsLeft(mPips[0], mPips[1], mPips[2]) > 0)
  {
    const int s = roll(3);
    if (mPips[s] < kMaxPips)
      ++mPips[s];
  }
}

void RunnerEditor::randomName()
{
  static const char* const kNames[] = {"ACE", "BLAZE", "ECHO", "FLUX", "GHOST", "HEX", "IVY", "JINX", "KAI", "LUX",
    "MAX", "NYX", "ONYX", "PIXEL", "QUINN", "REX", "SOL", "VOLT", "ZED", "ZIGGY", "RUBY", "STORM", "TURBO", "COBALT",
    "MAGPIE", "RIOT", "SPARK", "ORBIT"};
  mName = kNames[roll(int(sizeof(kNames) / sizeof(kNames[0])))];
}

RunnerEditor::Result RunnerEditor::tick(bool ok, bool cancel, int dir, int side)
{
  if (mNaming)
  {
    tickNaming(ok, cancel, dir, side);
    return Result::None;
  }
  const auto list = rows();
  const int n = int(list.size());
  if (dir != 0)
  {
    mCursor = wrap(mCursor + dir, n);
    mConfirmDelete = false;
  }
  if (cancel)
    return Result::Cancel;
  const Row row = list[std::size_t(std::clamp(mCursor, 0, n - 1))];
  if (side != 0)
    change(row, side);
  if (!ok)
    return Result::None;
  switch (row)
  {
    case Row::Name:
      mNaming = true;
      mKey = kKeyDone;
      return Result::None;
    case Row::Randomize:
      randomize();
      return Result::None;
    case Row::Save:
      return Result::Save;
    case Row::Delete:
      if (mConfirmDelete)
        return Result::Delete;
      mConfirmDelete = true;
      return Result::None;
    default:
      change(row, 1);
      return Result::None;
  }
}

void RunnerEditor::typeText(const std::string& text)
{
  for (char c : text)
  {
    if (c >= 'a' && c <= 'z')
      c = char(c - 'a' + 'A');
    const bool ok = (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '-' || c == '.' ||
      c == '!' || c == '\'';
    if (ok && int(mName.size()) < kMaxRunnerName && !(c == ' ' && mName.empty()))
      mName += c;
  }
}

void RunnerEditor::backspace()
{
  if (!mName.empty())
    mName.pop_back();
}

void RunnerEditor::tickNaming(bool ok, bool cancel, int dir, int side)
{
  if (cancel)
  {
    mNaming = false;
    return;
  }
  // The key grid: four rows of ten, then SPACE, DEL and DONE under them.
  if (mKey < kKeyChars)
  {
    const int row = mKey / kKeyCols, col = mKey % kKeyCols;
    if (side != 0)
      mKey = row * kKeyCols + wrap(col + side, kKeyCols);
    if (dir < 0)
      mKey = row == 0 ? (col < 3 ? kKeySpace : col < 6 ? kKeyDelete : kKeyDone) : mKey - kKeyCols;
    else if (dir > 0)
      mKey = row == 3 ? (col < 3 ? kKeySpace : col < 6 ? kKeyDelete : kKeyDone) : mKey + kKeyCols;
  }
  else
  {
    if (side != 0)
      mKey = kKeySpace + wrap(mKey - kKeySpace + side, 3);
    const int col = mKey == kKeySpace ? 1 : mKey == kKeyDelete ? 4 : 7;
    if (dir < 0)
      mKey = 3 * kKeyCols + col;
    else if (dir > 0)
      mKey = col;
  }
  if (!ok)
    return;
  if (mKey < kKeyChars)
    typeText(std::string(1, kKeys[mKey]));
  else if (mKey == kKeySpace)
    typeText(" ");
  else if (mKey == kKeyDelete)
    backspace();
  else
    mNaming = false;
}

std::string RunnerEditor::rowLabel(Row row) const
{
  const bool robot = mParts.body == 1;
  switch (row)
  {
    case Row::Name: return "NAME";
    case Row::Body: return "BODY";
    case Row::Build: return "BUILD";
    case Row::Hair: return robot ? "HEAD KIT" : "HAIR";
    case Row::Face: return robot ? "OPTICS" : "FACE";
    case Row::Outfit: return "OUTFIT";
    case Row::Arms: return "ARMS";
    case Row::Skin: return robot ? "PLATING" : "SKIN";
    case Row::HairColor: return robot ? "TRIM" : "HAIR COLOUR";
    case Row::Top: return "TOP";
    case Row::Pants: return "PANTS";
    case Row::Boots: return "BOOTS";
    case Row::Glow: return "GLOW";
    case Row::Gun: return "STARTING GUN";
    case Row::Health: return "HEALTH";
    case Row::Jump: return "JUMP";
    case Row::Power: return "POWER";
    case Row::Randomize: return "RANDOMIZE LOOK AND STATS";
    case Row::Save: return "SAVE RUNNER";
    case Row::Delete: return mConfirmDelete ? "PRESS AGAIN TO DELETE" : "DELETE RUNNER";
  }
  return "";
}

std::string RunnerEditor::rowValue(Row row) const
{
  const auto& p = mParts;
  const CharacterDef d = runner();
  char buf[64];
  switch (row)
  {
    case Row::Name: return mName.empty() ? "-" : mName;
    case Row::Body: return p.body == 1 ? "ROBOT" : "HUMAN";
    case Row::Build: return buildName(p.build);
    case Row::Hair: return hairStyleName(p.body, p.hair);
    case Row::Face: return faceName(p.body, p.face);
    case Row::Outfit: return outfitName(p.outfit);
    case Row::Arms: return p.sleeves == 1 ? (p.body == 1 ? "BARE METAL" : "BARE") : "SLEEVES";
    case Row::Gun: return weaponName(mGun);
    case Row::Health:
      std::snprintf(buf, sizeof(buf), "%d HEARTS", d.maxHp);
      return buf;
    case Row::Jump:
      std::snprintf(buf, sizeof(buf), "%d HIGH", jumpCells(d));
      return buf;
    case Row::Power:
      if (mGun == Weapon::Normal)
      {
        if (d.startRapidFire <= 0)
          return "NO RAPID FIRE";
        std::snprintf(buf, sizeof(buf), "RAPID FIRE %dS", d.startRapidFire / 15);
        return buf;
      }
      std::snprintf(buf, sizeof(buf), "%d SHOTS", d.startAmmo);
      return buf;
    default: return "";
  }
}

const Texture& RunnerEditor::pose(int p)
{
  const CharacterDef d = runner();
  const std::string key = encodeRunner(d);
  if (key != mPreviewKey)
  {
    mPreview.clear();
    mPreviewKey = key;
  }
  auto it = mPreview.find(p);
  if (it == mPreview.end())
    it = mPreview.emplace(p, bakeRunnerPose(mRenderer, d, p, 2.9f)).first;
  return it->second;
}

void RunnerEditor::render(Renderer& r, const Art& art, const Theme& t, int frame)
{
  drawBackdrop(r, art, float(frame) * 1.5f, 0.0f, 0.0f);
  r.fillRect(0, 0, float(kScreenW), float(kScreenH), rgba(4, 2, 12, 150));
  r.draw(art.vignette, 0, 0);
  r.drawText(mIsNew ? "NEW RUNNER" : "EDIT RUNNER", 640, 14, {46.0f, t.accentA, kInk, true}, Align::Center);

  // The runner, going through its moves: standing, a run, a jump.
  const CharacterDef d = runner();
  r.draw(mPanel, 24, 84);
  static const int kShow[] = {0, 8, 0, 8, 10, 11, 12, 13, 14, 15, 16, 17, 10, 11, 12, 13, 14, 15, 16, 17, 6, 2, 7,
    3, 4, 0};
  const int step = (frame / 7) % int(sizeof(kShow) / sizeof(kShow[0]));
  const int shown = kShow[step];
  const float lift = shown == 2 ? 40.0f : shown == 7 ? 20.0f : 0.0f;
  drawGlow(r, art, 249, 440, 210, art.runnerColor(d), 0.22f);
  r.fillRect(110, 508, 278, 6, rgba(0, 0, 0, 90));
  r.draw(pose(shown), 249, 520 - lift);

  r.drawText(d.name, 249, 532, {38.0f, t.accentA, kInk, true}, Align::Center);
  r.drawText(d.role, 249, 578, {18.0f, rgb(200, 198, 220)}, Align::Center);
  const int left = runnerPointsLeft(mPips[0], mPips[1], mPips[2]);
  char buf[64];
  std::snprintf(buf, sizeof(buf), "STAT POINTS LEFT: %d", left);
  r.drawText(buf, 249, 616, {18.0f, left > 0 ? t.accentB : rgb(170, 168, 190), kInk}, Align::Center);

  // The rows.
  r.draw(mListPanel, 486, 84);
  const auto list = rows();
  for (int i = 0; i < int(list.size()); ++i)
  {
    const Row row = list[std::size_t(i)];
    const bool sel = i == mCursor && !mNaming;
    const float y = kRowY + 14.0f + float(i) * kRowH;
    if (sel)
      r.fillRect(500, y - 3, 742, kRowH - 2, withAlpha(t.accentA, 60));
    const bool action = row == Row::Randomize || row == Row::Save || row == Row::Delete;
    const Color labelColor = sel ? t.accentA : (action ? t.accentB : t.hudText);
    if (action)
    {
      r.drawText(rowLabel(row), 871, y, {19.0f, row == Row::Delete && mConfirmDelete ? rgb(255, 90, 90) : labelColor,
        kInk, sel}, Align::Center);
      continue;
    }
    r.drawText(rowLabel(row), 516, y, {18.0f, labelColor, kInk, sel});
    const float vx = 990.0f;
    if (sel && row != Row::Name)
    {
      r.drawText("<", 790, y, {18.0f, t.accentA, kInk, true}, Align::Center);
      r.drawText(">", 1190, y, {18.0f, t.accentA, kInk, true}, Align::Center);
    }
    Color swatch = 0;
    switch (row)
    {
      case Row::Skin: swatch = skinTone(mParts.body, mParts.skin).first; break;
      case Row::HairColor: swatch = paletteColor(mParts.hairColor); break;
      case Row::Top: swatch = paletteColor(mParts.top); break;
      case Row::Pants: swatch = paletteColor(mParts.pants); break;
      case Row::Boots: swatch = paletteColor(mParts.boots); break;
      case Row::Glow: swatch = paletteColor(mParts.glow); break;
      default: break;
    }
    if (swatch != 0)
    {
      r.fillRect(vx - 62, y + 1, 124, 20, kInk);
      r.fillRect(vx - 60, y + 3, 120, 16, swatch);
      continue;
    }
    if (row == Row::Health || row == Row::Jump || row == Row::Power)
    {
      const int stat = row == Row::Health ? 0 : row == Row::Jump ? 1 : 2;
      for (int k = 0; k < kMaxPips; ++k)
      {
        const Color c = k < mPips[stat] ? t.accentB : (k < kMinPips[stat] ? rgba(255, 255, 255, 20) : rgba(255, 255, 255, 50));
        r.fillRect(830 + float(k) * 34, y + 4, 28, 13, c);
      }
      r.drawText(rowValue(row), 1172, y, {16.0f, rgb(205, 205, 222)}, Align::Right);
      continue;
    }
    std::string value = rowValue(row);
    if (row == Row::Name && sel)
      value += "   (ENTER / A TO TYPE)";
    r.drawText(value, vx, y, {18.0f, sel ? rgb(255, 255, 255) : rgb(205, 205, 222), kInk}, Align::Center);
  }

  r.drawText("UP/DOWN choose   LEFT/RIGHT change   ENTER / A select   ESC / B back without saving", 640, 690,
    {15.0f, rgb(210, 210, 228), kInk}, Align::Center);
  if (mNaming)
    renderNaming(r, t);
}

void RunnerEditor::renderNaming(Renderer& r, const Theme& t)
{
  r.fillRect(0, 0, float(kScreenW), float(kScreenH), rgba(0, 0, 0, 150));
  r.draw(mKeysPanel, 250, 130);
  r.drawText("NAME YOUR RUNNER", 640, 150, {36.0f, t.accentA, kInk, true}, Align::Center);
  r.fillRect(400, 204, 480, 52, rgba(255, 255, 255, 24));
  std::string shown = mName;
  if (int(shown.size()) < kMaxRunnerName)
    shown += '_';
  r.drawText(shown, 640, 210, {36.0f, rgb(255, 255, 255), kInk, true}, Align::Center);

  const float gx = 300.0f, gy = 280.0f, cw = 68.0f, ch = 50.0f;
  for (int i = 0; i < kKeyChars; ++i)
  {
    const float x = gx + float(i % kKeyCols) * cw, y = gy + float(i / kKeyCols) * ch;
    const bool sel = i == mKey;
    r.fillRect(x + 3, y + 3, cw - 6, ch - 6, sel ? withAlpha(t.accentA, 120) : rgba(255, 255, 255, 22));
    r.drawText(std::string(1, kKeys[i]), x + cw * 0.5f, y + 9, {26.0f, sel ? rgb(255, 255, 255) : t.hudText, kInk, sel},
      Align::Center);
  }
  const char* specials[3] = {"SPACE", "DEL", "DONE"};
  const float sx[3] = {gx, gx + 3 * cw, gx + 6 * cw};
  const float sw[3] = {3 * cw, 3 * cw, 4 * cw};
  for (int i = 0; i < 3; ++i)
  {
    const bool sel = mKey == kKeySpace + i;
    const float y = gy + 4 * ch;
    r.fillRect(sx[i] + 3, y + 3, sw[i] - 6, ch - 6, sel ? withAlpha(t.accentA, 120) : rgba(255, 255, 255, 22));
    r.drawText(specials[i], sx[i] + sw[i] * 0.5f, y + 11, {22.0f, sel ? rgb(255, 255, 255) : t.accentB, kInk, sel},
      Align::Center);
  }
  r.drawText("Type on the keyboard, or pick letters with the arrows / d-pad.   ENTER done   ESC / B done", 640, 556,
    {15.0f, rgb(210, 210, 228), kInk}, Align::Center);
}

} // namespace gr
