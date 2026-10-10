// Runner select: a carousel of the built-in runners, the custom ones and a
// NEW RUNNER card, with EDIT / REMIX under the chosen card. Also the runner
// editor around it (runner_editor.cpp does the editing).

#include "frontend/game.hpp"

#include "data/campaign.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace gr
{

namespace
{

constexpr Color kInk = rgb(16, 12, 26);
constexpr float kCardW = 340.0f;
constexpr float kCardH = 440.0f;
constexpr float kCardStep = 370.0f;
constexpr float kCardY = 180.0f;

int customCount() { return characterCount() - kDefaultRunners; }

} // namespace

int Game::selectCards() const
{
  return characterCount() + (customCount() < kMaxCustomRunners ? 1 : 0);
}

bool Game::tickSelect(Input& in, const Input& raw)
{
  if (mOptions.autoplay)
    in = mBot.menu(mCursor, mOptions.character, mModeTicks) | raw;
  auto edge = [&](bool Input::*f) { return in.*f && !(mPrev.*f); };
  const int cards = selectCards();
  mCursor = std::clamp(mCursor, 0, cards - 1);
  const bool onRunner = mCursor < characterCount();

  if (edge(&Input::up))
  {
    if (mSelectFocus == SelectFocus::Action)
      mSelectFocus = SelectFocus::Cards;
    else if (mSelectFocus == SelectFocus::Cards && !campaign())
      mSelectFocus = SelectFocus::Load;
    sound(Sfx::MenuMove);
  }
  if (edge(&Input::down))
  {
    if (mSelectFocus == SelectFocus::Load)
      mSelectFocus = SelectFocus::Cards;
    else if (mSelectFocus == SelectFocus::Cards && onRunner)
      mSelectFocus = SelectFocus::Action;
    sound(Sfx::MenuMove);
  }
  if (mSelectFocus != SelectFocus::Load && (edge(&Input::left) || edge(&Input::right)))
  {
    mCursor = (mCursor + (edge(&Input::left) ? cards - 1 : 1)) % cards;
    if (mCursor >= characterCount())
      mSelectFocus = SelectFocus::Cards;
    sound(Sfx::MenuMove);
  }
  // The carousel glides to the chosen card (the long way round when the
  // cursor wraps).
  mSelectScroll += (float(mCursor) - mSelectScroll) * 0.22f;

  const bool go = (edge(&Input::confirm) || edge(&Input::jump)) && mModeTicks > 20;
  if (go)
  {
    sound(Sfx::MenuSelect);
    if (mSelectFocus == SelectFocus::Load)
    {
      mSlotsForSave = false;
      openMenu(Menu::Slots);
    }
    else if (mCursor >= characterCount())
    {
      openEditor(-1);
    }
    else if (mSelectFocus == SelectFocus::Action)
    {
      openEditor(mCursor);
    }
    else if (campaign())
    {
      beginCampaignLevel(mPendingLevel, mPendingNewGame);
    }
    else
    {
      startLevel();
    }
    return true;
  }
  if (mSelectFocus != SelectFocus::Cards && edge(&Input::back))
  {
    mSelectFocus = SelectFocus::Cards;
    return true;
  }
  if (campaign())
  {
    if (edge(&Input::back) || edge(&Input::pause))
      goTitle();
  }
  else if (edge(&Input::pause) && !in.confirm)
  {
    return false; // Esc on the title screen quits
  }
  return true;
}

void Game::openEditor(int index)
{
  if (index < 0 || !characterByIndex(index).custom)
  {
    if (customCount() >= kMaxCustomRunners)
    {
      sound(Sfx::Hurt);
      notice("NO ROOM FOR MORE RUNNERS");
      return;
    }
    if (index < 0)
    {
      mEditor = std::make_unique<RunnerEditor>(mRenderer, characterByIndex(0), true);
      mEditor->randomize();
      mEditor->randomName();
    }
    else
    {
      mEditor = std::make_unique<RunnerEditor>(mRenderer, remixRunner(characterByIndex(index)), true);
    }
  }
  else
  {
    mEditor = std::make_unique<RunnerEditor>(mRenderer, characterByIndex(index), false);
  }
  mEditorReturn = mCursor;
  setMode(Mode::Editor);
}

void Game::tickEditor(const Input& in)
{
  auto edge = [&](bool Input::*f) { return in.*f && !(mPrev.*f); };
  // Start opens menus elsewhere; here, like Esc, it backs out.
  const bool cancel = edge(&Input::back) || edge(&Input::pause);
  const bool ok = !cancel && (edge(&Input::confirm) || edge(&Input::jump)) && mModeTicks > 10;
  const int dir = edge(&Input::up) ? -1 : (edge(&Input::down) ? 1 : 0);
  const int side = edge(&Input::left) ? -1 : (edge(&Input::right) ? 1 : 0);
  if (dir != 0 || side != 0)
    sound(Sfx::MenuMove);
  const bool wasNaming = mEditor->naming();
  const auto result = mEditor->tick(ok, cancel, dir, side);
  if (ok && result == RunnerEditor::Result::None)
    sound(wasNaming ? Sfx::MenuMove : Sfx::MenuSelect);

  switch (result)
  {
    case RunnerEditor::Result::None:
      return;
    case RunnerEditor::Result::Save:
    {
      const CharacterDef def = mEditor->runner();
      mCursor = putCustomRunner(def);
      if (!saveCustomRunners(saveDir()))
      {
        std::fprintf(stderr, "could not save the runners to %s\n", saveDir().c_str());
        notice("COULD NOT SAVE THE RUNNERS");
      }
      else
      {
        notice(def.name + " IS READY TO RUN");
      }
      sound(Sfx::Checkpoint);
      break;
    }
    case RunnerEditor::Result::Delete:
    {
      const CharacterDef def = mEditor->runner();
      removeCustomRunner(def.id);
      saveCustomRunners(saveDir());
      notice(def.name + " HAS LEFT THE TEAM");
      mCursor = std::min(mEditorReturn, characterCount() - 1);
      sound(Sfx::Hurt);
      break;
    }
    case RunnerEditor::Result::Cancel:
      mCursor = mEditorReturn;
      sound(Sfx::MenuMove);
      break;
  }
  mEditor.reset();
  mSelectFocus = SelectFocus::Cards;
  mSelectScroll = float(mCursor);
  setMode(Mode::Select);
}

bool Game::wantsText() const
{
  return mMode == Mode::Editor && mEditor && mEditor->naming();
}

void Game::typeText(const std::string& text)
{
  if (wantsText())
    mEditor->typeText(text);
}

void Game::textBackspace()
{
  if (wantsText())
    mEditor->backspace();
}

void Game::textDone()
{
  if (wantsText())
  {
    mEditor->finishName();
    sound(Sfx::MenuSelect);
    // Enter is still down on the next tick: it must not count as a press
    // (it would open the name again).
    mPrev.confirm = true;
  }
}

void Game::renderEditor()
{
  if (mEditor)
    mEditor->render(mRenderer, *mArt, theme(), mFrame);
}

void Game::renderSelect()
{
  auto& r = mRenderer;
  const auto& t = theme();
  drawBackdrop(r, *mArt, float(mFrame) * 1.5f, 0.0f, 0.0f);
  r.fillRect(0, 0, float(kScreenW), float(kScreenH), rgba(4, 2, 12, 110));
  r.draw(mArt->vignette, 0, 0);

  const float bounce = std::sin(float(mFrame) * 0.06f) * 5.0f;
  const float cx = float(kScreenW) / 2.0f;
  r.drawText("GUNRUNNERS", cx + 7, 18 + bounce + 7, {100.0f, withAlpha(t.platform, 200), 0, true}, Align::Center);
  r.drawText("GUNRUNNERS", cx, 18 + bounce, {100.0f, t.accentA, kInk, true}, Align::Center);
  if (campaign())
  {
    char buf[128];
    std::snprintf(buf, sizeof(buf), "CHOOSE YOUR RUNNER FOR LEVEL %d  -  %s", mPendingLevel,
      campaignLevel(mPendingLevel).title);
    r.drawText(buf, cx, 138, {22.0f, t.accentB, kInk, true}, Align::Center);
  }
  else
  {
    r.drawText("CHOOSE YOUR RUNNER", cx, 138, {24.0f, t.hudText, kInk}, Align::Center);
  }

  const int cards = selectCards();
  for (int i = 0; i < cards; ++i)
  {
    const float x = cx - kCardW * 0.5f + (float(i) - mSelectScroll) * kCardStep;
    if (x < -kCardW || x > float(kScreenW))
      continue;
    const bool selected = i == mCursor;
    const float y = kCardY;
    if (selected)
      drawGlow(r, *mArt, x + 170, y + 190, 300, t.accentA, 0.18f + 0.06f * std::sin(float(mFrame) * 0.12f));
    r.draw(selected ? mCardPanelSelected : mCardPanel, x, y);

    if (i >= characterCount())
    {
      // Make your own.
      const float pulse = selected ? 1.0f + 0.05f * std::sin(float(mFrame) * 0.1f) : 1.0f;
      r.drawText("+", x + 170, y + 30, {150.0f * pulse, selected ? t.accentA : t.hudText, kInk, true}, Align::Center);
      r.drawText("NEW RUNNER", x + 170, y + 226, {36.0f, selected ? t.accentA : t.hudText, kInk, true}, Align::Center);
      r.drawText("BUILD YOUR OWN", x + 170, y + 278, {17.0f, rgb(180, 178, 200)}, Align::Center);
      r.drawText("LOOK, NAME, GUN AND STATS", x + 170, y + 316, {15.0f, rgb(160, 158, 182)}, Align::Center);
      continue;
    }

    const auto& def = characterByIndex(i);
    DrawOpts portrait;
    if (!selected)
      portrait.tint = rgb(120, 116, 140);
    const float hop = selected ? -std::abs(std::sin(float(mFrame) * 0.1f)) * 10.0f : 0.0f;
    r.draw(mArt->portrait(def), x + 170, y - 22 + hop, portrait);
    if (def.custom)
      r.drawText("CUSTOM", x + kCardW - 20, y + 14, {14.0f, t.accentB, kInk, true}, Align::Right);

    r.drawText(def.name, x + 170, y + 222, {40.0f, selected ? t.accentA : t.hudText, kInk, true}, Align::Center);
    std::string role = def.role;
    role += "  -  ";
    role += weaponName(def.startWeapon);
    r.drawText(role, x + 170, y + 274, {17.0f, rgb(180, 178, 200)}, Align::Center);
    const char* labels[3] = {"HEALTH", "JUMP", "POWER"};
    const int pips[3] = {def.healthPips, def.jumpPips, def.powerPips};
    for (int s = 0; s < 3; ++s)
    {
      const float sy = y + 314 + float(s) * 36;
      r.drawText(labels[s], x + 28, sy, {17.0f, rgb(205, 205, 222)});
      for (int k = 0; k < 5; ++k)
        r.fillRect(x + 128 + float(k) * 38, sy + 5, 32, 13, k < pips[s] ? t.accentB : rgba(255, 255, 255, 40));
    }
  }
  if (cards > 3)
  {
    const float a = 0.6f + 0.3f * std::sin(float(mFrame) * 0.1f);
    r.drawText("<", 26, 370, {48.0f, t.accentA, kInk, true}, Align::Center, a);
    r.drawText(">", float(kScreenW) - 26, 370, {48.0f, t.accentA, kInk, true}, Align::Center, a);
  }

  // EDIT for your own runners, REMIX (a copy to change) for the built-in.
  if (mCursor < characterCount())
  {
    const bool focus = mSelectFocus == SelectFocus::Action;
    r.draw(focus ? mActionButtonFocus : mActionButton, cx - 130, 634);
    r.drawText(characterByIndex(mCursor).custom ? "EDIT RUNNER" : "REMIX RUNNER", cx, 643,
      {20.0f, focus ? t.accentA : t.hudText, kInk, true}, Align::Center);
  }

  if (campaign())
    return;

  // LOAD GAME button, reached with up.
  const bool load = mSelectFocus == SelectFocus::Load;
  r.draw(load ? mLoadButtonFocus : mLoadButton, 1030, 124);
  r.drawText("LOAD GAME", 1140, 134, {20.0f, load ? t.accentA : t.hudText, kInk, true}, Align::Center);

  r.drawText(keysHint(), cx, 696, {14.0f, rgb(210, 210, 228), kInk}, Align::Center);
}

} // namespace gr
