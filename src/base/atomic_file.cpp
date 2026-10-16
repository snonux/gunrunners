#include "base/atomic_file.hpp"

#include <filesystem>
#include <fstream>

namespace gr
{

bool writeFileAtomic(const std::string& path, const std::string& data, std::string* error)
{
  namespace fs = std::filesystem;
  const std::string tmp = path + ".tmp";
  std::error_code ec;
  {
    std::ofstream f(tmp, std::ios::trunc);
    if (!f)
    {
      // Nothing was opened (tmp may even be something we must not touch,
      // such as a directory), so there is nothing to clean up.
      if (error)
        *error = "cannot write " + tmp;
      return false;
    }
    // A small file sits in the stream buffer until the flush, so a full disk
    // or a file size limit only shows up there (or at close). The destructor
    // would swallow that error, hence the explicit flush and close and the
    // state check before the rename may replace the old file.
    f << data;
    f.flush();
    f.close();
    if (f.fail())
    {
      if (error)
        *error = "cannot write " + tmp;
      fs::remove(tmp, ec);
      return false;
    }
  }
  fs::rename(tmp, path, ec);
  if (ec)
  {
    if (error)
      *error = ec.message();
    std::error_code ignored;
    fs::remove(tmp, ignored);
    return false;
  }
  return true;
}

} // namespace gr
