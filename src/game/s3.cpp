#include "game/s3.hpp"

#include "base/atomic_file.hpp"
#include "game/savegame.hpp"

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <vector>

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
  std::ostringstream contents;
  contents << "endpoint=" << config.endpoint << '\n'
    << "bucket=" << config.bucket << '\n'
    << "region=" << config.region << '\n'
    << "prefix=" << config.prefix << '\n'
    << "access_key=" << config.accessKey << '\n'
    << "secret_key=" << config.secretKey << '\n';
  // writeFileAtomic with mode 0600: the secret key goes to a new temp file
  // of this write's own (O_EXCL, so a symlink planted at any temp name, the
  // fixed "cloud.txt.tmp" of older builds included, is never written
  // through), readable by the owner only from its first byte on, synced
  // before the rename. A failed write keeps the old cloud.txt.
  //
  // The temp names are "cloud.txt.tmp.<pid>.<n>", which the exact paths of
  // the Android backup rules cannot list (they take no wildcards; see
  // android/app/src/main/res/xml/backup_rules.xml). That is accepted: a temp
  // file holds the secret from its creation until the rename, a window that
  // spans the write and the fsync (tens to hundreds of milliseconds on phone
  // flash), and outlives it only when the process is killed inside it or
  // when a failed write's cleanup unlink fails too (see atomic_file.hpp).
  // Such leftovers, and the "cloud.txt.tmp" of older builds, are removed by
  // removeCloudLeftovers at game start once at least kCloudLeftoverAge
  // (10 minutes) old, as a younger one may be another game process's write
  // in progress, and by removeS3Config at any age. Not taken: keeping
  // cloud.txt and its temps in a folder of their own (saves/private/, say),
  // which the backup rules could exclude as a whole; it would need a
  // migration of existing cloud.txt files.
  return writeFileAtomic(saveDir + "/cloud.txt", contents.str(), error, 0600);
}

namespace
{

// The leftover names below are matched exactly, every number in them by
// decimal digits only, so the sweeps never take a file that just looks
// similar (see removeCloudLeftovers in s3.hpp).

// name[from, to) is one or more decimal digits.
bool digitsOnly(const std::string& name, std::size_t from, std::size_t to)
{
  if (from >= to || to > name.size())
    return false;
  for (std::size_t i = from; i < to; ++i)
    if (name[i] < '0' || name[i] > '9')
      return false;
  return true;
}

bool startsWith(const std::string& name, const std::string& prefix)
{
  return name.compare(0, prefix.size(), prefix) == 0;
}

// rest is ".tmp.<pid>.<n>", the suffix of writeFileAtomic's temp files.
bool isAtomicTempSuffix(const std::string& rest)
{
  const std::string tmp = ".tmp.";
  if (!startsWith(rest, tmp))
    return false;
  const std::size_t dot = rest.find('.', tmp.size());
  return dot != std::string::npos && digitsOnly(rest, tmp.size(), dot) &&
    digitsOnly(rest, dot + 1, rest.size());
}

// rest is kind followed by a sync's nonce (a steady clock count; see
// syncS3File) and, where withTemp, maybe by the download's write temp
// suffix too: writeFileAtomic's, or the fixed ".s3.tmp" s3GetFile wrote
// through in older builds. The upload snapshot (copyFile) has no temp.
bool isSyncFileSuffix(const std::string& rest, const std::string& kind,
  bool withTemp)
{
  if (!startsWith(rest, kind))
    return false;
  const std::size_t dot = rest.find('.', kind.size());
  const std::size_t end = dot == std::string::npos ? rest.size() : dot;
  if (!digitsOnly(rest, kind.size(), end))
    return false;
  if (end == rest.size())
    return true;
  const std::string temp = rest.substr(end);
  return withTemp && (temp == ".s3.tmp" || isAtomicTempSuffix(temp));
}

// What follows a save's file name in the name of one of its leftovers:
// the write temps of the save and of its dirty marker, writeFileAtomic's
// (".tmp.<pid>.<n>") and the fixed ".tmp" older builds wrote both through,
// and a sync's files. The dirty marker itself (".cloud-dirty") is none: it
// is real state.
bool isSaveLeftoverSuffix(const std::string& rest)
{
  const std::string dirty = ".cloud-dirty";
  const std::string legacyTemp = ".tmp";
  return rest == legacyTemp || isAtomicTempSuffix(rest) ||
    rest == dirty + legacyTemp ||
    (startsWith(rest, dirty) && isAtomicTempSuffix(rest.substr(dirty.size()))) ||
    isSyncFileSuffix(rest, ".cloud-upload-", false) ||
    isSyncFileSuffix(rest, ".cloud-incoming-", true);
}

// A write temp of cloud.txt: writeFileAtomic's, or the fixed name older
// builds wrote it through.
bool isConfigLeftover(const std::string& name)
{
  const std::string config = "cloud.txt";
  return startsWith(name, config) &&
    (name == config + ".tmp" || isAtomicTempSuffix(name.substr(config.size())));
}

// The file names of the five slots and the quick save, the only files
// that are synced.
std::vector<std::string> saveFileNames(const std::string& saveDir)
{
  std::vector<std::string> names;
  for (int i = 0; i <= kSaveSlots; ++i)
  {
    const std::string path = i == kSaveSlots ? quickSavePath(saveDir) : slotPath(saveDir, i);
    names.push_back(std::filesystem::path(path).filename().string());
  }
  return names;
}

bool isCloudLeftover(const std::string& name, const std::vector<std::string>& saves)
{
  if (isConfigLeftover(name))
    return true;
  for (const std::string& save : saves)
    if (name.size() > save.size() && startsWith(name, save) &&
        isSaveLeftoverSuffix(name.substr(save.size())))
      return true;
  return false;
}

// A regular file (no directory, no symlink, whatever its target) last
// written at least minAge before now; with minAge 0, at any time. A file
// written "in the future" (the clock was set back) counts as new.
bool oldEnough(const std::filesystem::path& path,
  std::filesystem::file_time_type now, std::chrono::seconds minAge)
{
  namespace fs = std::filesystem;
  std::error_code ec;
  if (!fs::is_regular_file(fs::symlink_status(path, ec)) || ec)
    return false;
  if (minAge <= std::chrono::seconds::zero())
    return true;
  const fs::file_time_type written = fs::last_write_time(path, ec);
  return !ec && now - written >= minAge;
}

// Removes the files in dir whose names isLeftover picks and that are
// oldEnough. The names are collected first, so the removals do not run
// under the directory iteration. Returns how many were removed. The first
// failure goes to error, if that is still empty: a dir that cannot be
// listed in full (EACCES, EIO; its leftovers may be among the names not
// seen), then a removal that failed. A dir that is not there is no
// failure, as it holds nothing to remove.
int sweepLeftovers(const std::string& dir,
  const std::function<bool(const std::string&)>& isLeftover,
  std::chrono::seconds minAge, std::string& error)
{
  namespace fs = std::filesystem;
  std::vector<fs::path> found;
  std::error_code ec;
  for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
    if (isLeftover(it->path().filename().string()))
      found.push_back(it->path());
  if (ec && ec != std::errc::no_such_file_or_directory && error.empty())
    error = "cannot list " + dir + ": " + ec.message();
  const fs::file_time_type now = fs::file_time_type::clock::now();
  int removed = 0;
  for (const fs::path& path : found)
  {
    if (!oldEnough(path, now, minAge))
      continue;
    std::error_code why;
    if (fs::remove(path, why))
      ++removed;
    else if (why && error.empty())
      error = "cannot remove " + path.string() + ": " + why.message();
  }
  return removed;
}

} // namespace

bool removeS3Config(const std::string& saveDir, std::string* error)
{
  // The write temps first, at any age: one left by a kill (or a failed
  // cleanup unlink, see atomic_file.hpp) holds the secret key, and the
  // Android backup rules cannot exclude its name. A saveS3Config running in
  // another game process right now loses its temp file and fails; the
  // player is turning the cloud off anyway. A failed sweep (a temp that
  // cannot be removed, or a save dir that cannot be listed, as temps may
  // hide in it) fails the call with cloud.txt still in place, so the
  // player sees the error and the secret is not left behind unnoticed.
  std::string why;
  sweepLeftovers(saveDir, isConfigLeftover, std::chrono::seconds::zero(), why);
  if (why.empty())
  {
    std::error_code ec;
    std::filesystem::remove(saveDir + "/cloud.txt", ec);
    if (!ec)
      return true;
    why = ec.message();
  }
  if (error)
    *error = why;
  return false;
}

int removeCloudLeftovers(const std::string& saveDir, std::chrono::seconds minAge)
{
  const std::vector<std::string> saves = saveFileNames(saveDir);
  std::string ignored; // best effort, listing included: the next start tries again
  return sweepLeftovers(saveDir,
    [&saves](const std::string& name) { return isCloudLeftover(name, saves); },
    minAge, ignored);
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
  // rename, the temp file removed (best effort) on any failure, so a full
  // disk does not collect half-downloaded temp files. The caller still
  // validates path before installing it anywhere.
  if (!writeFileAtomic(path, body, error))
    return S3GetResult::Error;
  return S3GetResult::Downloaded;
#endif
}

// The marker holds a token unique to this call (steady clock time and a
// counter), which syncS3File reads before an upload and compares before it
// clears the marker, so a save marked dirty during the upload stays dirty.
// writeFileAtomic replaces the marker all or nothing, so a reader sees the
// old token or the new one, never a torn one, and its unique O_EXCL temp
// names keep concurrent markers (another game process) and planted symlinks
// out of each other's way. Mode 0600, as for cloud.txt: the marker is
// private game state.
bool markS3Dirty(const std::string& path, std::string* error)
{
  namespace fs = std::filesystem;
  std::error_code ec;
  const fs::path save(path);
  if (save.has_parent_path())
    fs::create_directories(save.parent_path(), ec);
  static std::atomic<unsigned long long> sequence{0};
  const std::string token = std::to_string(
    std::chrono::steady_clock::now().time_since_epoch().count()) + '-' +
    std::to_string(sequence.fetch_add(1, std::memory_order_relaxed));
  std::string why;
  if (writeFileAtomic(path + ".cloud-dirty", token, &why, 0600))
    return true;
  if (error)
    *error = "cannot mark cloud save dirty: " + why;
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

// Snapshots the save at from to to, a name of this sync's own. A copy that
// fails (a full disk can cut it short) has what it wrote of to removed, so
// no partial snapshot is left behind. The snapshot gets the current time as
// its mtime, best effort: some copies keep the source's (CopyFile on
// Windows), and removeCloudLeftovers in another game process starting now
// must see a snapshot in use as new, not as old as the save.
bool copyFile(const std::string& from, const std::string& to,
  std::string* error)
{
  namespace fs = std::filesystem;
  std::error_code ec;
  fs::copy_file(from, to, fs::copy_options::overwrite_existing, ec);
  if (ec)
  {
    std::error_code ignored;
    fs::remove(to, ignored);
    if (error)
      *error = "cannot snapshot local save: " + ec.message();
    return false;
  }
  fs::last_write_time(to, fs::file_time_type::clock::now(), ec);
  return true;
}

// Removes a sync's temp file (best effort) when the sync returns, on every
// path, early errors and exceptions included, so none of them can leave it
// behind (a missing file is fine: never created, or already renamed into
// place). Only a killed process leaves one, which removeCloudLeftovers
// collects at the next start.
class RemoveOnReturn
{
public:
  explicit RemoveOnReturn(std::string path) : mPath(std::move(path)) {}
  ~RemoveOnReturn()
  {
    std::error_code ec;
    std::filesystem::remove(mPath, ec);
  }
  RemoveOnReturn(const RemoveOnReturn&) = delete;
  RemoveOnReturn& operator=(const RemoveOnReturn&) = delete;

private:
  std::string mPath;
};

// One syncS3File call: its request, the save it syncs, that save's mutex
// (held by syncS3File's pathMutex pointer for the whole call) and the name
// of its upload snapshot.
struct SyncJob
{
  const S3Config& config;
  const std::string& object;
  const std::string& path;
  std::string* error;
  const S3Active& active;
  std::mutex& saveMutex;
  std::string upload;
};

// The commit lock of active, if it has one. Whoever turns active false
// (the game, cancelling or replacing its cloud sync) does so under this
// lock too, so a sync that checks active under it commits nothing after
// that.
std::unique_lock<std::mutex> lockCommit(const S3Active& active)
{
  if (!active.commitMutex)
    return {};
  return std::unique_lock<std::mutex>(*active.commitMutex);
}

// Uploads the job's snapshot, then clears the save's dirty marker if it
// still holds token: a save marked dirty again during the upload stays
// dirty for the next sync. A sync cancelled during the upload clears
// nothing.
S3SyncResult putSnapshot(const SyncJob& job, const std::string& token)
{
  if (!s3PutFile(job.config, job.object, job.upload, job.error, job.active))
    return S3SyncResult::Error;
  const auto commitLock = lockCommit(job.active);
  if (job.active && !job.active())
    return S3SyncResult::Error;
  std::lock_guard<std::mutex> lock(job.saveMutex);
  clearDirtyIf(job.path, token);
  return S3SyncResult::Uploaded;
}

// What reconcile decided: upload the local save (a snapshot of it), or
// leave it at result (Unchanged; Downloaded, the cloud save installed;
// Error).
struct SyncPlan
{
  bool upload = false;
  S3SyncResult result = S3SyncResult::Unchanged;
};

// Saved at the same time as the cloud save: equal bytes are Unchanged,
// else the greater bytes win, so two devices settle on the same save.
SyncPlan compareSameTime(const std::string& path, const std::string& incoming)
{
  const std::string localBytes = fileContents(path);
  const std::string remoteBytes = fileContents(incoming);
  SyncPlan plan;
  if (localBytes > remoteBytes)
    plan.upload = true;
  else if (localBytes < remoteBytes)
    plan.result = S3SyncResult::Downloaded;
  return plan;
}

// Picks the newer of the local save at path and the downloaded cloud save
// at incoming (fetched says whether there was one), and installs the
// download over path when it wins. A dirty local save (offline progress)
// always goes up, as does any local save when the cloud has none; a
// missing local save never does. Runs under the save's mutex and the
// commit lock.
SyncPlan reconcile(const std::string& path, const std::string& incoming,
  S3GetResult fetched, bool dirty, std::string* error)
{
  SyncPlan plan;
  const auto local = readSave(path);
  if (dirty || fetched == S3GetResult::NotFound)
  {
    plan.upload = bool(local);
    return plan;
  }
  const auto remote = readSave(incoming);
  if (!remote)
  {
    if (error)
      *error = "cloud object is not a valid Gunrunners save";
    plan.result = S3SyncResult::Error;
    return plan;
  }
  if (local && local->savedAt > remote->savedAt)
    plan.upload = true;
  else if (local && local->savedAt == remote->savedAt)
    plan = compareSameTime(path, incoming);
  else
    plan.result = S3SyncResult::Downloaded;
  if (plan.result != S3SyncResult::Downloaded)
    return plan;
  std::error_code ec;
  std::filesystem::rename(incoming, path, ec);
  if (ec)
  {
    if (error)
      *error = "cannot install cloud save: " + ec.message();
    plan.result = S3SyncResult::Error;
  }
  return plan;
}

// A clean local save: downloads the cloud save, reconciles the two and
// uploads the local one if it wins.
S3SyncResult downloadAndReconcile(const SyncJob& job, const std::string& nonce)
{
  const std::string incoming = job.path + ".cloud-incoming-" + nonce;
  // Removed on every return, the failed snapshot below included (it used
  // to leak the download); a download installed by the rename is gone from
  // this name already.
  const RemoveOnReturn incomingCleanup(incoming);
  const S3GetResult fetched = s3GetFile(job.config, job.object, incoming,
    job.error, job.active);
  if (fetched == S3GetResult::Error)
    return S3SyncResult::Error;
  std::string token;
  SyncPlan plan;
  {
    const auto commitLock = lockCommit(job.active);
    std::lock_guard<std::mutex> lock(job.saveMutex);
    if (job.active && !job.active())
      return S3SyncResult::Error;
    token = dirtyToken(job.path);
    plan = reconcile(job.path, incoming, fetched, !token.empty(), job.error);
    if (plan.result == S3SyncResult::Error)
      return S3SyncResult::Error;
    if (plan.upload && !copyFile(job.path, job.upload, job.error))
      return S3SyncResult::Error;
  }
  // The download is done with: remove it now rather than when the upload
  // below returns, which can be up to a request timeout later (30 s), so a
  // kill during the upload does not leave it behind as well.
  std::error_code ec;
  std::filesystem::remove(incoming, ec);
  if (!plan.upload)
    return plan.result;
  return putSnapshot(job, token);
}

} // namespace

// A dirty local save is snapshotted (under the save's mutex, so a game
// save cannot tear it) and uploaded as is; a clean one is reconciled with
// the cloud's (downloadAndReconcile). The sync's files carry a nonce of
// their own and are removed on every return.
S3SyncResult syncS3File(const S3Config& config, const std::string& object,
  const std::string& path, std::string* error, const S3Active& active)
{
  const auto mutex = pathMutex(path);
  const std::string nonce = std::to_string(
    std::chrono::steady_clock::now().time_since_epoch().count());
  const SyncJob job{config, object, path, error, active, *mutex,
    path + ".cloud-upload-" + nonce};
  const RemoveOnReturn uploadCleanup(job.upload);
  std::string initialToken;
  bool uploadDirty = false;
  {
    std::lock_guard<std::mutex> lock(*mutex);
    initialToken = dirtyToken(path);
    uploadDirty = !initialToken.empty() && bool(readSave(path));
    if (uploadDirty && !copyFile(path, job.upload, error))
      return S3SyncResult::Error;
  }
  if (uploadDirty)
    return putSnapshot(job, initialToken);
  return downloadAndReconcile(job, nonce);
}

} // namespace gr
