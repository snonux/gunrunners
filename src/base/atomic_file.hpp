#pragma once

#include <string>

namespace gr
{

// Replaces the file at path with data, all or nothing: the data goes to
// path + ".tmp" first, and only once every byte is written, flushed and the
// file closed without error is the temp file renamed over path. On any
// failure (temp not creatable, disk full, file size limit, rename refused)
// the temp file is removed, path keeps its old content and the result is
// false, with a short reason in *error when error is given. The parent
// directory must exist already.
bool writeFileAtomic(const std::string& path, const std::string& data, std::string* error = nullptr);

} // namespace gr
