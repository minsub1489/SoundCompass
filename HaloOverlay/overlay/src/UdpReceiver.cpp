#include "UdpReceiver.h"

#include <ws2tcpip.h>

namespace halo
{
UdpReceiver::UdpReceiver(int port)
{
    WSADATA data;
    started_ = WSAStartup(MAKEWORD(2, 2), &data) == 0;
    if (!started_) { return; }
    socket_ = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_ == INVALID_SOCKET) { return; }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<u_short>(port));
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
    event_ = WSACreateEvent();
    if (::bind(socket_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
        WSAEventSelect(socket_, event_, FD_READ) != 0) // also makes the socket non-blocking
    {
        closesocket(socket_);
        socket_ = INVALID_SOCKET;
    }
}

UdpReceiver::~UdpReceiver()
{
    if (socket_ != INVALID_SOCKET) { closesocket(socket_); }
    if (event_ != WSA_INVALID_EVENT) { WSACloseEvent(event_); }
    if (started_) { WSACleanup(); }
}

std::vector<std::string> UdpReceiver::Drain()
{
    std::vector<std::string> out;
    if (!Ok()) { return out; }
    WSAResetEvent(event_);
    static thread_local char buffer[65536];
    for (;;)
    {
        const int n = ::recv(socket_, buffer, sizeof(buffer), 0);
        if (n > 0) { out.emplace_back(buffer, static_cast<size_t>(n)); continue; }
        // WSAEMSGSIZE: an oversize datagram was truncated; drop it and keep reading.
        if (n < 0 && WSAGetLastError() == WSAEMSGSIZE) { continue; }
        break;
    }
    return out;
}
}
