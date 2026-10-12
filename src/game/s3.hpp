#pragma once

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
bool removeS3Config(const std::string& saveDir, std::string* error = nullptr);

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
