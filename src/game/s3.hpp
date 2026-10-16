#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

namespace gr
{

struct SaveGame;

struct S3Config
{
  std::string endpoint;
  std::string bucket;
  std::string region = "us-east-1";
  std::string accessKey;
  std::string secretKey;
  std::string prefix;
  std::string caBundle; // bundled CA roots on Android; system defaults elsewhere

  bool enabled() const { return !endpoint.empty(); }
  bool valid(std::string* error = nullptr) const;
  std::string object(const std::string& name) const;
};

struct S3Active
{
  std::function<bool()> check;
  std::shared_ptr<std::mutex> commitMutex;

  explicit operator bool() const { return bool(check); }
  bool operator()() const { return !check || check(); }
};

// A copy/paste-friendly key=value block. cloud.txt is stored beside the
// local saves with owner-only permissions.
bool parseS3Config(const std::string& text, S3Config& config,
  std::string* error = nullptr);
S3Config loadS3Config(const std::string& saveDir);
bool saveS3Config(const std::string& saveDir, const S3Config& config,
  std::string* error = nullptr);
// Removes cloud.txt, and first every write temp of it
// ("cloud.txt.tmp.<pid>.<n>" and the fixed "cloud.txt.tmp" of older
// builds; regular files only, as for removeCloudLeftovers) at any age, as
// they may hold the secret key. A temp file that cannot be removed, or a
// saveDir that cannot be listed (it may hide some), fails the call before
// cloud.txt is touched, so the settings stay as they were. A saveDir that
// is not there is no failure.
bool removeS3Config(const std::string& saveDir, std::string* error = nullptr);

// What a game process killed (or crashed) during a cloud sync or a write
// leaves in saveDir, and nothing else ever removes: the sync's upload
// snapshots ("<save>.cloud-upload-<nonce>") and downloads
// ("<save>.cloud-incoming-<nonce>", and its write temp:
// "...<nonce>.tmp.<pid>.<n>", or the fixed "...<nonce>.s3.tmp" of older
// builds), the write temps of the saves and of their dirty markers
// ("<save>.tmp.<pid>.<n>", "<save>.cloud-dirty.tmp.<pid>.<n>", and the
// fixed "<save>.tmp" and "<save>.cloud-dirty.tmp" of older builds) and of
// cloud.txt (see removeS3Config). <save> is one of the five slots or the
// quick save, and every number is decimal digits only, so a save, its
// dirty marker, cloud.txt or any other file (even one merely named like a
// save's leftover) is never touched. Only regular files go, never a
// directory or a symlink (nor its target).
//
// Only those written at least minAge ago go (0: all of them): a second
// game process on the same saves may be syncing right now, and a sync's
// file lives through one request at most (30 s, see configure in s3.cpp),
// so kCloudLeftoverAge spares it by a wide margin. The game runs this once
// at start, before its own first sync. Returns how many files were
// removed; a file that cannot be removed, or a saveDir that cannot be
// listed, is skipped, as the next start tries again.
constexpr std::chrono::minutes kCloudLeftoverAge{10};
int removeCloudLeftovers(const std::string& saveDir,
  std::chrono::seconds minAge = kCloudLeftoverAge);

enum class S3GetResult
{
  Downloaded,
  NotFound,
  Error,
};

enum class S3SyncResult
{
  Unchanged,
  Uploaded,
  Downloaded,
  Error,
};

// Uploads/downloads one file using the S3 path-style API and AWS Signature V4.
// Downloads replace the destination atomically only after the request succeeds.
bool s3PutFile(const S3Config& config, const std::string& object,
  const std::string& path, std::string* error = nullptr,
  const S3Active& active = {});
S3GetResult s3GetFile(const S3Config& config, const std::string& object,
  const std::string& path, std::string* error = nullptr,
  const S3Active& active = {});

// writeS3Save serializes replacement and dirty marking with reconciliation.
// Reconciliation uploads a marked local file first; otherwise it validates
// both saves and keeps the newest one.
bool markS3Dirty(const std::string& path, std::string* error = nullptr);
bool writeS3Save(const SaveGame& save, const std::string& path,
  std::string* error = nullptr);
S3SyncResult syncS3File(const S3Config& config, const std::string& object,
  const std::string& path, std::string* error = nullptr,
  const S3Active& active = {});

} // namespace gr
