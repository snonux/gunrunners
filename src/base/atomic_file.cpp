#include "base/atomic_file.hpp"

#include <atomic>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <unistd.h>

namespace gr
{

namespace
{

// How many taken temp names a write skips before it gives up and fails with
// EEXIST. A name can be taken by a file planted there, by a leftover of an
// earlier process with our pid (pid reuse: in a container where the game is
// always pid 1, every crash leaves its temp file at one of the low counter
// values the next process tries first), or by a live process that shares
// our pid (pid namespaces in containers, hosts sharing an NFS directory,
// the web build, where getpid() is always 42). Processes sharing a pid stay
// safe: O_EXCL lets only one of them create a name, and the others skip to
// their next counter value. 64 is plenty for these cases; the bound keeps a
// hostile directory from spinning the write forever, and it means a write
// fails only when all 64 names it tries are taken (each attempt also moves
// the counter on, so the process's next write tries 64 new names).
constexpr int kTempAttempts = 64;

void fail(std::string* error, const std::string& what, int err)
{
  if (error)
    *error = what + ": " + std::strerror(err);
}

// A temp name no other write of this process uses: the pid tells game
// processes apart (mostly, see kTempAttempts for processes sharing a pid),
// the counter the threads and successive writes of one process.
std::string tempName(const std::string& path)
{
  static std::atomic<unsigned long long> counter{0};
  return path + ".tmp." + std::to_string(::getpid()) + "." +
    std::to_string(counter.fetch_add(1, std::memory_order_relaxed));
}

// Creates a new, empty temp file for path with mode (minus the umask) and
// returns its descriptor, with its name in tmp, or -1 with errno set (tmp
// then holds the last name tried). O_EXCL never opens what already exists
// at a name, a symlink (dangling or not) included, and nothing found there
// is unlinked: it belongs to someone else, so the next name is tried
// instead. As the file is always new, mode is exactly what it gets: no
// older file's looser permissions survive, so no fchmod is needed.
int createTemp(const std::string& path, std::string& tmp, mode_t mode)
{
  for (int attempt = 0; attempt < kTempAttempts; ++attempt)
  {
    tmp = tempName(path);
    int fd;
    do
      fd = ::open(tmp.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, mode);
    while (fd < 0 && errno == EINTR);
    if (fd >= 0 || errno != EEXIST)
      return fd;
  }
  return -1; // errno is EEXIST from the last attempt
}

// Writes all of data to fd and syncs it to the disk. False with errno set on
// a failed write (ENOSPC, EFBIG, EDQUOT...) or sync. A file system that
// cannot sync this file at all (EINVAL) counts as synced: there is no more
// durability to get, and the rename is still all or nothing.
bool writeAndSync(int fd, const std::string& data)
{
  std::size_t written = 0;
  while (written < data.size())
  {
    const ssize_t n = ::write(fd, data.data() + written, data.size() - written);
    if (n < 0 && errno == EINTR)
      continue;
    if (n <= 0)
    {
      if (n == 0)
        errno = EIO;
      return false;
    }
    written += std::size_t(n);
  }
  return ::fsync(fd) == 0 || errno == EINVAL;
}

// Makes the rename durable: fsync on the directory that holds path. Best
// effort, as some systems cannot open or sync a directory; the file itself
// is already on the disk, so a lost rename only brings the old file back.
void syncParentDir(const std::string& path)
{
  std::string dir = std::filesystem::path(path).parent_path().string();
  if (dir.empty())
    dir = ".";
  const int fd = ::open(dir.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if (fd < 0)
    return;
  ::fsync(fd);
  ::close(fd);
}

} // namespace

bool writeFileAtomic(const std::string& path, const std::string& data, std::string* error, mode_t mode)
{
  std::string tmp;
  const int fd = createTemp(path, tmp, mode);
  if (fd < 0)
  {
    // Nothing was created, so there is nothing of ours to clean up. errno
    // is saved before the strings below can change it.
    const int err = errno;
    fail(error, "cannot write " + tmp, err);
    return false;
  }
  // close() can report a deferred write error too (NFS, some FUSE file
  // systems), so its result counts as much as the write's and the sync's.
  bool ok = writeAndSync(fd, data);
  int err = errno;
  if (::close(fd) != 0 && ok)
  {
    ok = false;
    err = errno;
  }
  if (!ok)
  {
    // The cleanup comes first, so that fail(), which builds strings and may
    // throw (bad_alloc), cannot skip it. Best effort, unchecked: if even
    // the unlink fails, the temp file stays behind, as the header says, and
    // *error still names the first failure (err was saved above).
    ::unlink(tmp.c_str());
    fail(error, "cannot write " + tmp, err);
    return false;
  }
  if (::rename(tmp.c_str(), path.c_str()) != 0)
  {
    err = errno;
    ::unlink(tmp.c_str()); // first and best effort, as above
    fail(error, "cannot replace " + path, err);
    return false;
  }
  syncParentDir(path);
  return true;
}

} // namespace gr
