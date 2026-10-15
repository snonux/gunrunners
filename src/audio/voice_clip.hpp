#pragma once

#include <string>
#include <vector>

namespace gr
{

// Reads a whole file through SDL, which also finds a relative path among
// the APK's assets on Android. False if it can't be read.
bool readDataFile(const std::string& path, std::vector<unsigned char>& out);

// Decodes Ogg Vorbis (any rate, mono or stereo) to mono floats at
// kAudioRate (see audio.hpp). Empty if the data is not Ogg Vorbis.
std::vector<float> decodeOggVorbis(const std::vector<unsigned char>& data);
// Just `count` samples from sample `start` on (at the file's own rate),
// the same way: a clip out of a bank of them.
std::vector<float> decodeOggVorbisRange(const std::vector<unsigned char>& data, int start, int count);

} // namespace gr
