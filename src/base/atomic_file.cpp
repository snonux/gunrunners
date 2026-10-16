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

// How many taken temp names a write skips before it gives up. Only stale
// leftovers whose pid matches ours (pid reuse) or planted files can take a
// name, so a handful is plenty; the bound keeps a hostile directory from
// spinning the write forever.
constexpr int kTempAttempts = 64;

void fail(std::string* error, const std::string& what, int err)
{
  if (error)
    *error = what + ": " + std::strerror(err);
}

// A temp name no other write uses: the pid tells game processes apart, the
// counter the threads and successive writes of one process.
std::string tempName(const std::string& path)
{
  static std::atomic<unsigned long long> counter{0};
  return path + ".tmp." + std::to_string(::getpid()) + "." +
    std::to_string(counter.fetch_add(1, std::memory_order_relaxed));
}

// Creates a new, empty temp file for path and returns its descriptor, with
// its name in tmp, or -1 with errno set (tmp then holds the last name
// tried). O_EXCL never opens what already exists at a name, a symlink
// (dangling or not) included, and nothing found there is unlinked: it
// belongs to someone else, so the next name is tried instead.
int createTemp(const std::string& path, std::string& tmp)
{
  for (int attempt = 0; attempt < kTempAttempts; ++attempt)
  {
    tmp = tempName(path);
    int fd;
    do
      fd = ::open(tmp.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0666);
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

bool writeFileAtomic(const std::string& path, const std::string& data, std::string* error)
{
  std::string tmp;
  const int fd = createTemp(path, tmp);
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
    fail(error, "cannot write " + tmp, err);
    ::unlink(tmp.c_str());
    return false;
  }
  if (::rename(tmp.c_str(), path.c_str()) != 0)
  {
    err = errno;
    fail(error, "cannot replace " + path, err);
    ::unlink(tmp.c_str());
    return false;
  }
  syncParentDir(path);
  return true;
}

} // namespace gr
