#pragma once

#include "assets/art.hpp"
#include "data/characters.hpp"
#include "render/renderer.hpp"

#include <map>
#include <string>
#include <vector>

namespace gr
{

// The runner editor: a new runner, or a change to one made earlier. A list
// of rows walked with up/down; left/right (or OK) changes the row's value,
// so a keyboard, a gamepad and the on-screen d-pad all work it the same.
// The name is typed on an on-screen keyboard, or on a real one.
class RunnerEditor
{
public:
  // start: the runner to change, or (isNew) the one to start a new one from.
  RunnerEditor(const Renderer& renderer, const CharacterDef& start, bool isNew);

  enum class Result
  {
    None,
    Save,
    Delete,
    Cancel,
  };
  // Menu input edges: OK, back, up/down (dir) and left/right (side).
  Result tick(bool ok, bool cancel, int dir, int side);
  // Typing on a real keyboard while the name is being entered.
  bool naming() const { return mNaming; }
  void typeText(const std::string& text);
  void backspace();
  void finishName() { mNaming = false; }

  // The runner as it stands.
  CharacterDef runner() const;
  // A random look, gun and stats (a new runner starts like this).
  void randomize();
  void randomName();
  bool isNew() const { return mIsNew; }

  void render(Renderer& r, const Art& art, const Theme& theme, int frame);

private:
  enum class Row
  {
    Name,
    Body,
    Build,
    Hair,
    Face,
    Outfit,
    Arms,
    Skin,
    HairColor,
    Top,
    Pants,
    Boots,
    Glow,
    Gun,
    Health,
    Jump,
    Power,
    Randomize,
    Save,
    Delete,
  };
  std::vector<Row> rows() const;
  void change(Row row, int step);
  void changePips(int stat, int step);
  void tickNaming(bool ok, bool cancel, int dir, int side);
  std::string rowLabel(Row row) const;
  std::string rowValue(Row row) const;
  void renderNaming(Renderer& r, const Theme& theme);
  const Texture& pose(int pose);

  const Renderer& mRenderer;
  std::string mId;
  bool mIsNew;
  std::string mName;
  RunnerParts mParts;
  Weapon mGun;
  int mPips[3];
  int mCursor = 0;
  bool mConfirmDelete = false;

  bool mNaming = false;
  int mKey = 0; // on-screen keyboard: 0-39 the characters, then SPACE, DEL, DONE

  // Preview poses, baked on demand for the runner as it stands.
  std::string mPreviewKey;
  std::map<int, Texture> mPreview;
  Texture mPanel, mListPanel, mKeysPanel;
};

} // namespace gr
