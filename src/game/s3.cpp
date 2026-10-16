#include "game/s3.hpp"

#include "base/atomic_file.hpp"
#include "game/savegame.hpp"

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fcntl.h>
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include <unordered_map>

#ifndef GR_NO_CURL
#include <curl/curl.h>
#endif

namespace gr
{

bool S3Config::valid(std::string* error) const
{
  const auto fail = [&](const char* message) {
    if (error)
      *error = message;
    return false;
  };
  if (!enabled())
    return fail("S3 endpoint is empty");
  if (bucket.empty())
    return fail("S3 bucket is empty");
  if (region.empty())
    return fail("S3 region is empty");
  if (accessKey.empty())
    return fail("S3 access key is empty");
  if (secretKey.empty())
    return fail("S3 secret key is empty");
  return true;
}

std::string S3Config::object(const std::string& name) const
{
  if (prefix.empty())
    return name;
  return prefix.back() == '/' ? prefix + name : prefix + '/' + name;
}

bool parseS3Config(const std::string& text, S3Config& config, std::string* error)
{
  S3Config parsed;
  std::istringstream lines(text);
  std::string line;
  while (std::getline(lines, line))
  {
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    if (line.empty() || line[0] == '#')
      continue;
    const std::size_t equals = line.find('=');
    if (equals == std::string::npos)
    {
      if (error)
        *error = "expected key=value";
      return false;
    }
    const std::string key = line.substr(0, equals);
    const std::string value = line.substr(equals + 1);
    if (key == "endpoint") parsed.endpoint = value;
    else if (key == "bucket") parsed.bucket = value;
    else if (key == "region") parsed.region = value;
    else if (key == "prefix") parsed.prefix = value;
    else if (key == "access_key") parsed.accessKey = value;
    else if (key == "secret_key") parsed.secretKey = value;
    else
    {
      if (error)
        *error = "unknown cloud setting: " + key;
      return false;
    }
  }
  if (!parsed.valid(error))
    return false;
  config = std::move(parsed);
  return true;
}

S3Config loadS3Config(const std::string& saveDir)
{
  std::ifstream file(saveDir + "/cloud.txt");
  if (!file)
    return {};
  std::ostringstream text;
  text << file.rdbuf();
  S3Config config;
  std::string error;
  if (!parseS3Config(text.str(), config, &error))
    return {};
  return config;
}

bool saveS3Config(const std::string& saveDir, const S3Config& config,
  std::string* error)
{
  if (!config.valid(error))
    return false;
  namespace fs = std::filesystem;
  std::error_code ec;
  fs::create_directories(saveDir, ec);
  if (ec)
  {
    if (error)
      *error = ec.message();
    return false;
  }
  const std::string path = saveDir + "/cloud.txt";
  const std::string temporary = path + ".tmp";
  std::ostringstream contents;
  contents << "endpoint=" << config.endpoint << '\n'
    << "bucket=" << config.bucket << '\n'
    << "region=" << config.region << '\n'
    << "prefix=" << config.prefix << '\n'
    << "access_key=" << config.accessKey << '\n'
    << "secret_key=" << config.secretKey << '\n';
  const std::string data = contents.str();
  {
    const int fd = ::open(temporary.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0)
    {
      if (error)
        *error = "cannot write " + temporary;
      return false;
    }
    // O_CREAT does not tighten an existing temp file's mode.
    bool ok = ::fchmod(fd, 0600) == 0;
    std::size_t written = 0;
    while (ok && written < data.size())
    {
      const ssize_t n = ::write(fd, data.data() + written, data.size() - written);
      if (n <= 0)
        ok = false;
      else
        written += std::size_t(n);
    }
    if (::close(fd) != 0)
      ok = false;
    if (!ok)
    {
      fs::remove(temporary, ec);
      if (error)
        *error = "cannot securely write " + temporary;
      return false;
    }
  }
  fs::rename(temporary, path, ec);
  if (ec)
  {
    const std::string message = ec.message();
    fs::remove(temporary, ec);
    if (error)
      *error = "cannot replace cloud.txt: " + message;
    return false;
  }
  return true;
}

bool removeS3Config(const std::string& saveDir, std::string* error)
{
  std::error_code ec;
  std::filesystem::remove(saveDir + "/cloud.txt", ec);
  if (!ec)
    return true;
  if (error)
    *error = ec.message();
  return false;
}

namespace
{

#ifndef GR_NO_CURL
struct Curl
{
  Curl()
  {
    static const CURLcode initialized = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (initialized == CURLE_OK)
      handle = curl_easy_init();
  }
  ~Curl() { curl_easy_cleanup(handle); }
  CURL* handle = nullptr;
};
#endif

std::shared_ptr<std::mutex> pathMutex(const std::string& path)
{
  static std::mutex mapMutex;
  static std::unordered_map<std::string, std::weak_ptr<std::mutex>> mutexes;
  std::lock_guard<std::mutex> lock(mapMutex);
  auto mutex = mutexes[path].lock();
  if (!mutex)
  {
    mutex = std::make_shared<std::mutex>();
    mutexes[path] = mutex;
  }
  return mutex;
}

#ifdef GR_NO_CURL
// The browser build has no libcurl (a page cannot open its own sockets).
constexpr const char* kNoCloud = "cloud saves are not available in the browser";
#else
size_t appendBody(char* data, size_t size, size_t count, void* user)
{
  constexpr std::size_t kMaxResponse = 16 * 1024 * 1024;
  if (size != 0 && count > kMaxResponse / size)
    return 0;
  const std::size_t bytes = size * count;
  auto& body = *static_cast<std::string*>(user);
  if (bytes > kMaxResponse - body.size())
    return 0;
  body.append(data, bytes);
  return bytes;
}

int transferProgress(void* user, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
{
  const auto* active = static_cast<const S3Active*>(user);
  return active && *active && !(*active)() ? 1 : 0;
}

std::string escapeSegment(CURL* curl, const std::string& value)
{
  char* encoded = curl_easy_escape(curl, value.c_str(), int(value.size()));
  if (!encoded)
    return {};
  std::string result(encoded);
  curl_free(encoded);
  return result;
}

std::string escapePath(CURL* curl, const std::string& value)
{
  std::string result;
  std::size_t start = 0;
  while (start <= value.size())
  {
    const std::size_t slash = value.find('/', start);
    if (!result.empty())
      result += '/';
    result += escapeSegment(curl, value.substr(start, slash - start));
    if (slash == std::string::npos)
      break;
    start = slash + 1;
  }
  return result;
}

bool configure(CURL* curl, const S3Config& config, const std::string& object,
  std::string& url, std::string& credentials, std::string& sigv4,
  char* curlError, std::string* error, const S3Active& active)
{
  if (!config.valid(error))
    return false;
  if (!curl)
  {
    if (error)
      *error = "cannot initialize libcurl";
    return false;
  }
  url = config.endpoint;
  while (!url.empty() && url.back() == '/')
    url.pop_back();
  url += '/' + escapeSegment(curl, config.bucket) + '/' + escapePath(curl, object);
  credentials = config.accessKey + ':' + config.secretKey;
  sigv4 = "aws:amz:" + config.region + ":s3";
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_USERPWD, credentials.c_str());
  curl_easy_setopt(curl, CURLOPT_AWS_SIGV4, sigv4.c_str());
  if (!config.caBundle.empty())
    curl_easy_setopt(curl, CURLOPT_CAINFO, config.caBundle.c_str());
  curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlError);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
  if (active)
  {
    // libcurl calls transferProgress about once a second even while it
    // resolves, connects or waits on a silent server, so a cancel (S3Active
    // turning false) aborts the request within about that.
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, transferProgress);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &active);
#if LIBCURL_VERSION_NUM >= 0x075700
    // Without this, cleaning up a request aborted during its DNS lookup
    // joins the threaded resolver, i.e. waits for getaddrinfo to give up
    // (up to the system resolver's timeouts), which would hold up quitting.
    // The cost: each such aborted lookup leaks its resolver thread until
    // getaddrinfo returns. libcurl older than 7.87 lacks the option and
    // keeps that wait.
    curl_easy_setopt(curl, CURLOPT_QUICK_EXIT, 1L);
#endif
  }
  return true;
}

std::string requestError(CURLcode code, long status, const char* curlError,
  const std::string& body)
{
  if (code != CURLE_OK)
    return *curlError ? curlError : curl_easy_strerror(code);
  std::string message = "S3 returned HTTP " + std::to_string(status);
  if (!body.empty())
    message += ": " + body.substr(0, 300);
  return message;
}
#endif

} // namespace

bool s3PutFile(const S3Config& config, const std::string& object,
  const std::string& path, std::string* error, const S3Active& active)
{
#ifdef GR_NO_CURL
  (void)config, (void)object, (void)path, (void)active;
  if (error)
    *error = kNoCloud;
  return false;
#else
  if (active && !active())
    return false;
  std::ifstream input(path, std::ios::binary);
  if (!input)
  {
    if (error)
      *error = "cannot read " + path;
    return false;
  }
  std::ostringstream bytes;
  bytes << input.rdbuf();
  const std::string body = bytes.str();

  Curl request;
  std::string url, credentials, sigv4, response;
  char curlError[CURL_ERROR_SIZE] = {};
  if (!configure(request.handle, config, object, url, credentials, sigv4,
        curlError, error, active))
    return false;
  curl_easy_setopt(request.handle, CURLOPT_CUSTOMREQUEST, "PUT");
  curl_easy_setopt(request.handle, CURLOPT_POSTFIELDS, body.data());
  curl_easy_setopt(request.handle, CURLOPT_POSTFIELDSIZE_LARGE,
    curl_off_t(body.size()));
  curl_easy_setopt(request.handle, CURLOPT_WRITEFUNCTION, appendBody);
  curl_easy_setopt(request.handle, CURLOPT_WRITEDATA, &response);
  const CURLcode result = curl_easy_perform(request.handle);
  long status = 0;
  curl_easy_getinfo(request.handle, CURLINFO_RESPONSE_CODE, &status);
  if (result == CURLE_OK && status >= 200 && status < 300)
    return true;
  if (error)
    *error = requestError(result, status, curlError, response);
  return false;
#endif
}

S3GetResult s3GetFile(const S3Config& config, const std::string& object,
  const std::string& path, std::string* error, const S3Active& active)
{
#ifdef GR_NO_CURL
  (void)config, (void)object, (void)path, (void)active;
  if (error)
    *error = kNoCloud;
  return S3GetResult::Error;
#else
  if (active && !active())
    return S3GetResult::Error;
  Curl request;
  std::string url, credentials, sigv4, body;
  char curlError[CURL_ERROR_SIZE] = {};
  if (!configure(request.handle, config, object, url, credentials, sigv4,
        curlError, error, active))
    return S3GetResult::Error;
  curl_easy_setopt(request.handle, CURLOPT_HTTPGET, 1L);
  curl_easy_setopt(request.handle, CURLOPT_WRITEFUNCTION, appendBody);
  curl_easy_setopt(request.handle, CURLOPT_WRITEDATA, &body);
  const CURLcode result = curl_easy_perform(request.handle);
  long status = 0;
  curl_easy_getinfo(request.handle, CURLINFO_RESPONSE_CODE, &status);
  if (result == CURLE_OK && status == 404)
    return S3GetResult::NotFound;
  if (result != CURLE_OK || status < 200 || status >= 300)
  {
    if (error)
      *error = requestError(result, status, curlError, body);
    return S3GetResult::Error;
  }

  namespace fs = std::filesystem;
  const fs::path target(path);
  std::error_code ec;
  if (target.has_parent_path())
    fs::create_directories(target.parent_path(), ec);
  // writeFileAtomic, as profile.txt, runners.txt and the savegames use: a
  // temp file of this write's own, every byte written and synced before the
  // rename, the temp file removed on any failure, so a full disk does not
  // collect half-downloaded temp files. (saveS3Config and markS3Dirty in
  // this file do not use it yet: they still write a fixed-name ".tmp" by
  // hand with O_CREAT|O_TRUNC.) The caller still validates path before
  // installing it anywhere.
  if (!writeFileAtomic(path, body, error))
    return S3GetResult::Error;
  return S3GetResult::Downloaded;
#endif
}

bool markS3Dirty(const std::string& path, std::string* error)
{
  namespace fs = std::filesystem;
  std::error_code ec;
  const fs::path save(path);
  if (save.has_parent_path())
    fs::create_directories(save.parent_path(), ec);
  const std::string marker = path + ".cloud-dirty";
  const std::string temporary = marker + ".tmp";
  const int fd = ::open(temporary.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
  static std::atomic<unsigned long long> sequence{0};
  const std::string token = std::to_string(
    std::chrono::steady_clock::now().time_since_epoch().count()) + '-' +
    std::to_string(sequence.fetch_add(1, std::memory_order_relaxed));
  const bool written = fd >= 0 && ::write(fd, token.data(), token.size()) == ssize_t(token.size());
  if (fd >= 0 && ::close(fd) == 0 && written)
  {
    fs::rename(temporary, marker, ec);
    if (!ec)
      return true;
  }
  fs::remove(temporary, ec);
  if (error)
    *error = "cannot mark cloud save dirty";
  return false;
}

bool writeS3Save(const SaveGame& save, const std::string& path,
  std::string* error)
{
  const auto mutex = pathMutex(path);
  std::lock_guard<std::mutex> lock(*mutex);
  if (!writeSave(save, path, error))
    return false;
  return markS3Dirty(path, error);
}

namespace
{

std::string dirtyToken(const std::string& path)
{
  std::ifstream marker(path + ".cloud-dirty");
  std::string token;
  marker >> token;
  return token;
}

void clearDirtyIf(const std::string& path, const std::string& token)
{
  if (token.empty() || dirtyToken(path) != token)
    return;
  std::error_code ec;
  std::filesystem::remove(path + ".cloud-dirty", ec);
}

std::string fileContents(const std::string& path)
{
  std::ifstream file(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

bool copyFile(const std::string& from, const std::string& to,
  std::string* error)
{
  std::error_code ec;
  std::filesystem::copy_file(from, to,
    std::filesystem::copy_options::overwrite_existing, ec);
  if (!ec)
    return true;
  if (error)
    *error = "cannot snapshot local save: " + ec.message();
  return false;
}

} // namespace

S3SyncResult syncS3File(const S3Config& config, const std::string& object,
  const std::string& path, std::string* error, const S3Active& active)
{
  namespace fs = std::filesystem;
  const auto mutex = pathMutex(path);
  const std::string nonce = std::to_string(
    std::chrono::steady_clock::now().time_since_epoch().count());
  const std::string upload = path + ".cloud-upload-" + nonce;
  std::string initialToken;
  bool uploadDirty = false;
  {
    std::lock_guard<std::mutex> lock(*mutex);
    initialToken = dirtyToken(path);
    uploadDirty = !initialToken.empty() && bool(readSave(path));
    if (uploadDirty && !copyFile(path, upload, error))
      return S3SyncResult::Error;
  }
  if (uploadDirty)
  {
    if (!s3PutFile(config, object, upload, error, active))
    {
      std::error_code ec;
      fs::remove(upload, ec);
      return S3SyncResult::Error;
    }
    {
      std::unique_lock<std::mutex> commitLock;
      if (active.commitMutex)
        commitLock = std::unique_lock<std::mutex>(*active.commitMutex);
      if (active && !active())
      {
        std::error_code ec;
        fs::remove(upload, ec);
        return S3SyncResult::Error;
      }
      std::lock_guard<std::mutex> lock(*mutex);
      clearDirtyIf(path, initialToken);
    }
    std::error_code ec;
    fs::remove(upload, ec);
    return S3SyncResult::Uploaded;
  }

  const std::string incoming = path + ".cloud-incoming-" + nonce;
  const S3GetResult fetched = s3GetFile(config, object, incoming, error, active);
  if (fetched == S3GetResult::Error)
    return S3SyncResult::Error;

  std::string currentToken;
  bool shouldUpload = false;
  S3SyncResult localResult = S3SyncResult::Unchanged;
  {
    std::unique_lock<std::mutex> commitLock;
    if (active.commitMutex)
      commitLock = std::unique_lock<std::mutex>(*active.commitMutex);
    std::lock_guard<std::mutex> lock(*mutex);
    if (active && !active())
    {
      std::error_code ec;
      fs::remove(incoming, ec);
      return S3SyncResult::Error;
    }
    currentToken = dirtyToken(path);
    const auto local = readSave(path);
    if (!currentToken.empty())
      shouldUpload = bool(local);
    else if (fetched == S3GetResult::NotFound)
      shouldUpload = bool(local);
    else
    {
      const auto remote = readSave(incoming);
      if (!remote)
      {
        std::error_code ec;
        fs::remove(incoming, ec);
        if (error)
          *error = "cloud object is not a valid Gunrunners save";
        return S3SyncResult::Error;
      }
      if (local && local->savedAt > remote->savedAt)
        shouldUpload = true;
      else if (local && local->savedAt == remote->savedAt)
      {
        const std::string localBytes = fileContents(path);
        const std::string remoteBytes = fileContents(incoming);
        if (localBytes == remoteBytes)
          localResult = S3SyncResult::Unchanged;
        else if (localBytes > remoteBytes)
          shouldUpload = true;
        else
          localResult = S3SyncResult::Downloaded;
      }
      else
        localResult = S3SyncResult::Downloaded;

      if (localResult == S3SyncResult::Downloaded)
      {
        std::error_code ec;
        fs::rename(incoming, path, ec);
        if (ec)
        {
          const std::string message = ec.message();
          fs::remove(incoming, ec);
          if (error)
            *error = "cannot install cloud save: " + message;
          return S3SyncResult::Error;
        }
      }
    }
    if (shouldUpload && !copyFile(path, upload, error))
      return S3SyncResult::Error;
  }
  std::error_code ec;
  fs::remove(incoming, ec);
  if (!shouldUpload)
    return localResult;
  if (!s3PutFile(config, object, upload, error, active))
  {
    fs::remove(upload, ec);
    return S3SyncResult::Error;
  }
  {
    std::unique_lock<std::mutex> commitLock;
    if (active.commitMutex)
      commitLock = std::unique_lock<std::mutex>(*active.commitMutex);
    if (active && !active())
    {
      fs::remove(upload, ec);
      return S3SyncResult::Error;
    }
    std::lock_guard<std::mutex> lock(*mutex);
    clearDirtyIf(path, currentToken);
  }
  fs::remove(upload, ec);
  return S3SyncResult::Uploaded;
}

} // namespace gr
