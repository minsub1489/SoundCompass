#include "Check.h"
#include "UdpReceiver.h"

#include <ws2tcpip.h>

using namespace halo;

static void Send(int port, const char* text)
{
    SOCKET s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in to{};
    to.sin_family = AF_INET;
    to.sin_port = htons(static_cast<u_short>(port));
    inet_pton(AF_INET, "127.0.0.1", &to.sin_addr);
    ::sendto(s, text, static_cast<int>(strlen(text)), 0, reinterpret_cast<sockaddr*>(&to), sizeof(to));
    closesocket(s);
}

static void round_trip()
{
    UdpReceiver receiver(47891);
    CHECK(receiver.Ok());
    CHECK(receiver.Drain().empty());
    Send(47891, "one");
    Send(47891, "two");
    // Loopback delivery can lag under load (seen once right after a build), so collect for up to 1 s.
    std::vector<std::string> got;
    for (int waited = 0; got.size() < 2 && waited < 1000; waited += 10)
    {
        WaitForSingleObject(receiver.Event(), 10);
        for (std::string& datagram : receiver.Drain()) { got.push_back(std::move(datagram)); }
    }
    CHECK(got.size() == 2 && got[0] == "one" && got[1] == "two");
}

static void port_in_use_reports()
{
    UdpReceiver first(47892);
    UdpReceiver second(47892);
    CHECK(first.Ok());
    CHECK(!second.Ok());
}

int main()
{
    RUN(round_trip);
    RUN(port_in_use_reports);
    return g_failures;
}
