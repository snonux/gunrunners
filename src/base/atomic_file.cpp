#include "base/atomic_file.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <sys/stat.h>
#include <unistd.h>

namespace gr
{

namespace
{

void fail(std::string* error, const std::string& what, int err)
{
  if (error)
    *error = what + ": " + std::strerror(err);
}

// Opens a new, empty temp file at tmp for writing, or returns -1 with errno
// set. A non-directory already there is unlinked first: unlink removes a
// symlink itself, never its target, and a leftover file is replaced rather
// than truncated (which would also cut a file hard-linked to it). O_EXCL
// then refuses anything that appears at tmp in between, a symlink included,
// and a directory in the way, which is never removed.
int createTemp(const std::string& tmp)
{
  struct stat st;
  if (::lstat(tmp.c_str(), &st) == 0 && !S_ISDIR(st.st_mode) && ::unlink(tmp.c_str()) != 0)
    return -1;
  int fd;
  do
    fd = ::open(tmp.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0666);
  while (fd < 0 && errno == EINTR);
  return fd;
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
  const std::string tmp = path + ".tmp";
  const int fd = createTemp(tmp);
  if (fd < 0)
  {
    // Nothing was created, so there is nothing of ours to clean up.
    fail(error, "cannot write " + tmp, errno);
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
    fail(error, "cannot replace " + path, errno);
    ::unlink(tmp.c_str());
    return false;
  }
  syncParentDir(path);
  return true;
}

void makeDirs(const std::string& dir)
{
  std::string cur;
  for (std::size_t i = 0; i < dir.size(); ++i)
  {
    cur += dir[i];
    if (dir[i] == '/' && i > 0)
      ::mkdir(cur.c_str(), 0755);
  }
  ::mkdir(dir.c_str(), 0755);
}

} // namespace gr
