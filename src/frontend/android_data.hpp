#pragma once

#include <string>

namespace gr
{

// Android only. The game reads levels/ and cutscenes/ with plain file I/O,
// but on Android they ship in the APK's bundled-data/ directory and are
// copied to internal storage on every start (always fresh after an
// update). Returns that data directory, or "" on failure.
std::string prepareAndroidData();

// Where savegames and the profile go: the app's private internal storage.
std::string androidSaveDir();

} // namespace gr
