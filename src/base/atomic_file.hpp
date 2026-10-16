#pragma once

#include <string>

namespace gr
{

// Replaces the file at path with data, all or nothing. The data goes to a
// temp file of its own first, path + ".tmp." + pid + "." + a per-process
// counter, created with O_EXCL: a name that already exists (a leftover from
// a crash, a symlink, anything) is never opened, written through or
// removed; the next counter value is tried instead. So two writers to the
// same path, threads or game processes, never share a temp file, and a
// symlink planted at a temp name cannot redirect the write onto its target.
// Every byte is written and fsync'ed, the file closed, and only then is it
// renamed over path.
//
// True: path holds the new data in full (with concurrent writers, the last
// rename wins, never a mix), and since the data reached the disk before the
// rename, a crash or power loss afterwards leaves path with the old or the
// new content in full, never an empty or partial file. The parent directory
// is fsync'ed too, best effort (its failure is ignored), so the rename
// itself may still be lost to a power cut, bringing the old file back. A
// file system that cannot sync a file at all (EINVAL) gets no durability,
// but the swap is still all or nothing.
//
// False: nothing was replaced. Path keeps its old content, the temp file
// this call created (if any) is removed, nothing else is touched, and
// *error, when error is given, says why: "cannot write <temp>: ..." when
// the temp file could not be created or filled, "cannot replace <path>:
// ..." when the rename was refused. The parent directory must exist already.
//
// Only a process killed between creating its temp file and the rename
// leaves that file behind; nothing removes it later, but it never stands in
// the way of a write.
bool writeFileAtomic(const std::string& path, const std::string& data, std::string* error = nullptr);

} // namespace gr
