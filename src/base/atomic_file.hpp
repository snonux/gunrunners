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
// False: nothing was replaced. Path keeps its old content, nothing but the
// temp file this call created (if any) is touched, and *error, when error
// is given, says why: "cannot write <temp>: ..." when the temp file could
// not be created or filled, "cannot replace <path>: ..." when the rename
// was refused. The parent directory must exist already. The temp file is
// unlinked, but that unlink's own result is not checked: should it fail
// too (EIO, the file system remounted read-only), the temp file stays.
//
// So a temp file is left behind only by a process killed between creating
// it and the rename, or by a failed write whose cleanup unlink failed as
// well (the unlink runs before anything that could throw). Nothing removes
// it later. A write that tries its name just skips it, as with any taken
// name, but a write tries at most 64 names: it fails ("cannot write
// <temp>: File exists") when all of them are taken, e.g. when leftovers of
// many crashes of a game that always runs as the same pid (pid 1 in a
// container) pile up at the low counter values a new process starts with.
bool writeFileAtomic(const std::string& path, const std::string& data, std::string* error = nullptr);

} // namespace gr
