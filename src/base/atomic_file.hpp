#pragma once

#include <string>

namespace gr
{

// Replaces the file at path with data, all or nothing. The data goes to a
// fresh path + ".tmp" first: whatever non-directory sits at that name (a
// leftover temp file, a stray symlink) is unlinked, never written through,
// and the temp file is created anew, so a symlink there cannot redirect the
// write onto its target. Every byte is written and fsync'ed, the file
// closed, and only then is it renamed over path.
//
// True: path holds the new data, and since the data reached the disk before
// the rename, a crash or power loss afterwards leaves path with the old or
// the new content in full, never an empty or partial file. The parent
// directory is fsync'ed too, best effort (its failure is ignored), so the
// rename itself may still be lost to a power cut, bringing the old file
// back. A file system that cannot sync a file at all (EINVAL) gets no
// durability, but the swap is still all or nothing.
//
// False: nothing was replaced. Path keeps its old content, the temp file is
// removed if this call created it (a directory in the way is left alone)
// and *error, when error is given, says why. The parent directory must
// exist already (see makeDirs).
bool writeFileAtomic(const std::string& path, const std::string& data, std::string* error = nullptr);

// Creates dir and any missing parents (mode 0755), as `mkdir -p` would.
// Errors are ignored: a directory that cannot be made shows up as a failed
// write into it.
void makeDirs(const std::string& dir);

} // namespace gr
