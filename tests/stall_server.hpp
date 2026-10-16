#pragma once

// A local "S3 server" that never answers, for the cloud cancel tests, and
// the helpers those tests share.
//
// Stall::Response (the default) listens on a free loopback port but never
// answers: the kernel completes the TCP handshakes into the listen backlog,
// so libcurl connects, sends its request and then waits for a response that
// never comes, as with a server that accepts and stalls. Without a cancel,
// every request runs into its 30 s CURLOPT_TIMEOUT. Keep the server alive
// until the requests are done: closing it (or a connection taken with
// acceptClients) resets the connections, which would end them early.
//
// Stall::Connect (Linux only) fills the accept queue with its own
// connections first, so the kernel drops every further SYN and a client
// sits in its SYN retransmits: the request never gets past connecting.
//
// The constructor sets no_proxy for 127.0.0.1, so the requests reach this
// server even when the environment configures an HTTP proxy.

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

namespace grtest
{

// No .cloud-upload-* snapshot or .cloud-incoming-* download left in dir.
inline bool noCloudTemporaries(const std::filesystem::path& dir)
{
  for (const auto& entry : std::filesystem::directory_iterator(dir))
  {
    const std::string name = entry.path().filename().string();
    if (name.find(".cloud-upload-") != std::string::npos ||
        name.find(".cloud-incoming-") != std::string::npos)
      return false;
  }
  return true;
}

class StallServer
{
public:
  enum class Stall
  {
    Response, // connections complete, requests wait for an answer
    Connect,  // the accept queue is full, connections never complete
  };

  explicit StallServer(Stall stall = Stall::Response)
  {
    ::setenv("no_proxy", "127.0.0.1", 1);
    ::setenv("NO_PROXY", "127.0.0.1", 1);
    if (!listenOnLoopback(stall == Stall::Connect ? 1 : 64))
      return;
    if (stall == Stall::Connect && !fillAcceptQueue())
      closeAll();
  }
  ~StallServer() { closeAll(); }
  StallServer(const StallServer&) = delete;
  StallServer& operator=(const StallServer&) = delete;

  bool ok() const { return mFd >= 0; }
  std::string endpoint() const { return "http://127.0.0.1:" + std::to_string(mPort); }
  // True once a client has connected, waiting up to timeoutMs. Connections
  // stay queued, so this stays true afterwards (unless acceptClients takes
  // them).
  bool waitForClient(int timeoutMs) const
  {
    pollfd waiting{mFd, POLLIN, 0};
    return ::poll(&waiting, 1, timeoutMs) == 1;
  }

  // Takes queued connections until count are taken or timeoutMs passes,
  // and returns how many are taken so far. They stay open (never answered)
  // until the server is destroyed.
  int acceptClients(int count, int timeoutMs)
  {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (int(mClients.size()) < count)
    {
      const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
        deadline - std::chrono::steady_clock::now()).count();
      if (left <= 0 || !waitForClient(int(left)))
        break;
      const int client = ::accept(mFd, nullptr, nullptr);
      if (client >= 0)
        mClients.push_back(client);
    }
    return int(mClients.size());
  }

  // How many taken connections the client has not closed: it sent its
  // request (read and dropped here) and still waits for the answer.
  int openClients() const
  {
    int open = 0;
    for (int client : mClients)
      open += clientClosed(client, 0) ? 0 : 1;
    return open;
  }

  // True once the client closed every taken connection, waiting up to
  // timeoutMs for each.
  bool clientsClosed(int timeoutMs) const
  {
    for (int client : mClients)
      if (!clientClosed(client, timeoutMs))
        return false;
    return true;
  }

#ifdef __linux__
  // The connections waiting in the accept queue (the kernel reports the
  // queue's length and limit for a listening socket in tcp_info).
  int queuedConnections() const
  {
    tcp_info info{};
    socklen_t length = sizeof(info);
    if (::getsockopt(mFd, IPPROTO_TCP, TCP_INFO, &info, &length) != 0)
      return -1;
    return int(info.tcpi_unacked);
  }
#endif

private:
  bool listenOnLoopback(int backlog)
  {
    mFd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (mFd < 0)
      return false;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0; // any free port
    socklen_t length = sizeof(addr);
    if (::bind(mFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 ||
        ::listen(mFd, backlog) != 0 ||
        ::getsockname(mFd, reinterpret_cast<sockaddr*>(&addr), &length) != 0)
    {
      ::close(mFd);
      mFd = -1;
      return false;
    }
    mPort = ntohs(addr.sin_port);
    mAddress = addr;
    return true;
  }

  // Connects (never accepted) clients of its own until the accept queue is
  // full: Linux then drops every new SYN (the queue holds backlog + 1).
  // Elsewhere, or if the queue does not fill, the server is not ok().
  bool fillAcceptQueue()
  {
#ifdef __linux__
    for (int i = 0; i < 4 && !acceptQueueFull(); ++i)
    {
      const int before = queuedConnections();
      const int filler = ::socket(AF_INET, SOCK_STREAM, 0);
      if (filler < 0)
        return false;
      mFillers.push_back(filler);
      ::fcntl(filler, F_SETFL, O_NONBLOCK);
      if (::connect(filler, reinterpret_cast<const sockaddr*>(&mAddress), sizeof(mAddress)) != 0 &&
          errno != EINPROGRESS)
        return false;
      // Bounded wait for the kernel to queue the new connection.
      const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
      while (queuedConnections() <= before)
      {
        if (std::chrono::steady_clock::now() > deadline)
          return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
      }
    }
    return acceptQueueFull();
#else
    return false;
#endif
  }

#ifdef __linux__
  // The kernel's own test (sk_acceptq_is_full): more queued than the backlog.
  bool acceptQueueFull() const
  {
    tcp_info info{};
    socklen_t length = sizeof(info);
    return ::getsockopt(mFd, IPPROTO_TCP, TCP_INFO, &info, &length) == 0 &&
      info.tcpi_unacked > info.tcpi_sacked;
  }
#endif

  // Reads and drops whatever the client sent; true once it closed the
  // connection (end of stream or a reset), waiting up to timeoutMs for it.
  static bool clientClosed(int client, int timeoutMs)
  {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    char buffer[4096];
    for (;;)
    {
      const ssize_t got = ::recv(client, buffer, sizeof(buffer), MSG_DONTWAIT);
      if (got == 0 || (got < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR))
        return true;
      if (got > 0)
        continue;
      const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
        deadline - std::chrono::steady_clock::now()).count();
      pollfd waiting{client, POLLIN, 0};
      if (left <= 0 || ::poll(&waiting, 1, int(left)) == 0)
        return false;
    }
  }

  void closeAll()
  {
    for (int fd : mClients)
      ::close(fd);
    for (int fd : mFillers)
      ::close(fd);
    mClients.clear();
    mFillers.clear();
    if (mFd >= 0)
      ::close(mFd);
    mFd = -1;
  }

  int mFd = -1;
  int mPort = 0;
  sockaddr_in mAddress{};
  std::vector<int> mClients; // taken by acceptClients, never answered
  std::vector<int> mFillers; // Stall::Connect's own queued connections
};

} // namespace grtest
