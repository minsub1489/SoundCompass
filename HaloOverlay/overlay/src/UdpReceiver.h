#pragma once

#include <string>
#include <vector>

#include <winsock2.h>

namespace halo
{
/** Non-blocking UDP socket on 127.0.0.1. Event() is signalled when datagrams arrive. */
class UdpReceiver
{
public:
    explicit UdpReceiver(int port);
    ~UdpReceiver();
    UdpReceiver(const UdpReceiver&) = delete;
    UdpReceiver& operator=(const UdpReceiver&) = delete;

    bool Ok() const { return socket_ != INVALID_SOCKET; }
    HANDLE Event() const { return event_; }
    std::vector<std::string> Drain();

private:
    SOCKET socket_ = INVALID_SOCKET;
    WSAEVENT event_ = WSA_INVALID_EVENT;
    bool started_ = false;
};
}
