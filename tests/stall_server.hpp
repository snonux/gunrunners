#pragma once

// A local "S3 server" that never answers, for the cloud cancel tests. It
// listens on a free loopback port but never accepts: the kernel completes
// the TCP handshakes into the listen backlog, so libcurl connects, sends its
// request and then waits for a response that never comes, as with a server
// that accepts and stalls. Without a cancel, every request runs into its
// 30 s CURLOPT_TIMEOUT. Keep the server alive until the requests are done:
// closing it resets the queued connections, which would end them early.

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <string>

namespace grtest
{

class StallServer
{
public:
  StallServer()
  {
    mFd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (mFd < 0)
      return;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0; // any free port
    socklen_t length = sizeof(addr);
    // A backlog well above the game's 6 parallel slot requests.
    if (::bind(mFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 ||
        ::listen(mFd, 64) != 0 ||
        ::getsockname(mFd, reinterpret_cast<sockaddr*>(&addr), &length) != 0)
    {
      ::close(mFd);
      mFd = -1;
      return;
    }
    mPort = ntohs(addr.sin_port);
  }
  ~StallServer()
  {
    if (mFd >= 0)
      ::close(mFd);
  }
  StallServer(const StallServer&) = delete;
  StallServer& operator=(const StallServer&) = delete;

  bool ok() const { return mFd >= 0; }
  std::string endpoint() const { return "http://127.0.0.1:" + std::to_string(mPort); }
  // True once a client has connected, waiting up to timeoutMs. Connections
  // stay queued, so this stays true afterwards.
  bool waitForClient(int timeoutMs) const
  {
    pollfd waiting{mFd, POLLIN, 0};
    return ::poll(&waiting, 1, timeoutMs) == 1;
  }

private:
  int mFd = -1;
  int mPort = 0;
};

} // namespace grtest
