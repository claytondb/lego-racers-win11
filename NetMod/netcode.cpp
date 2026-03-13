#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <mmsystem.h>`n#include <cstring>
#include "netcode.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winmm.lib")

extern void Log(const char* fmt, ...);

NetSession* g_netSession = nullptr;

static bool g_wsaInit = false;

NetSession::NetSession()
    : sock(INVALID_SOCKET), connected(false), isHost(false),
      seqSend(0), seqRecvLast(0), latencyMs(0), lastRecvTime(0),
      hasNewRemote(false)
{
    memset(&remoteAddr, 0, sizeof(remoteAddr));
    memset(&latestRemote, 0, sizeof(latestRemote));
}

NetSession::~NetSession() {
    Stop();
}

bool NetSession::InitWinsock() {
    if (g_wsaInit) return true;
    WSADATA wsa;
    int r = WSAStartup(MAKEWORD(2,2), &wsa);
    if (r != 0) {
        Log("[Net] WSAStartup failed: %d", r);
        return false;
    }
    g_wsaInit = true;
    return true;
}

bool NetSession::StartHost(uint16_t port) {
    if (!InitWinsock()) return false;

    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        Log("[Net] socket() failed: %d", WSAGetLastError());
        return false;
    }

    // Bind to the port
    sockaddr_in bindAddr = {};
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_port   = htons(port);
    bindAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (sockaddr*)&bindAddr, sizeof(bindAddr)) == SOCKET_ERROR) {
        Log("[Net] bind(%d) failed: %d", port, WSAGetLastError());
        closesocket(sock);
        sock = INVALID_SOCKET;
        return false;
    }

    // Non-blocking
    u_long nb = 1;
    ioctlsocket(sock, FIONBIO, &nb);

    isHost    = true;
    connected = false; // will set to true when we receive first client packet
    Log("[Net] Host listening on UDP port %d", port);
    return true;
}

bool NetSession::Connect(const char* ip, uint16_t port) {
    if (!InitWinsock()) return false;

    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        Log("[Net] socket() failed: %d", WSAGetLastError());
        return false;
    }

    memset(&remoteAddr, 0, sizeof(remoteAddr));
    remoteAddr.sin_family = AF_INET;
    remoteAddr.sin_port   = htons(port);
    inet_pton(AF_INET, ip, &remoteAddr.sin_addr);

    // Also bind locally (any port)
    sockaddr_in local = {};
    local.sin_family      = AF_INET;
    local.sin_port        = 0;
    local.sin_addr.s_addr = INADDR_ANY;
    bind(sock, (sockaddr*)&local, sizeof(local));

    u_long nb = 1;
    ioctlsocket(sock, FIONBIO, &nb);

    isHost    = false;
    connected = true;
    Log("[Net] Client ready, sending to %s:%d", ip, port);
    return true;
}

void NetSession::Stop() {
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
        sock = INVALID_SOCKET;
    }
    connected = false;
}

void NetSession::SendState(const PlayerState& state, uint8_t playerSlot) {
    if (sock == INVALID_SOCKET) return;
    if (!isHost && !connected) return;

    NetPacket pkt = {};
    pkt.magic      = NET_MAGIC;
    pkt.version    = NET_VERSION;
    pkt.playerSlot = playerSlot;
    pkt.seq        = seqSend++;
    pkt.timestamp  = timeGetTime();
    pkt.posX = state.posX; pkt.posY = state.posY; pkt.posZ = state.posZ;
    pkt.spdX = state.spdX; pkt.spdY = state.spdY; pkt.spdZ = state.spdZ;
    pkt.fwdX = state.fwdX; pkt.fwdY = state.fwdY; pkt.fwdZ = state.fwdZ;
    pkt.upX  = state.upX;  pkt.upY  = state.upY;  pkt.upZ  = state.upZ;
    pkt.brick       = state.brick;
    pkt.whiteBricks = state.whiteBricks;
    pkt.flags       = state.flags;

    if (isHost && connected) {
        sendto(sock, (char*)&pkt, sizeof(pkt), 0, (sockaddr*)&remoteAddr, sizeof(remoteAddr));
    } else if (!isHost) {
        sendto(sock, (char*)&pkt, sizeof(pkt), 0, (sockaddr*)&remoteAddr, sizeof(remoteAddr));
    }
}

bool NetSession::RecvState(PlayerState* out) {
    if (sock == INVALID_SOCKET || !out) return false;

    char buf[512];
    sockaddr_in sender = {};
    int senderLen = sizeof(sender);

    int r = recvfrom(sock, buf, sizeof(buf), 0, (sockaddr*)&sender, &senderLen);
    if (r == SOCKET_ERROR) {
        int err = WSAGetLastError();
        if (err != WSAEWOULDBLOCK)
            Log("[Net] recvfrom error: %d", err);
        return false;
    }

    if (r < (int)sizeof(NetPacket)) {
        Log("[Net] Short packet: %d bytes", r);
        return false;
    }

    NetPacket* pkt = (NetPacket*)buf;
    if (pkt->magic != NET_MAGIC || pkt->version != NET_VERSION) {
        Log("[Net] Bad magic/version: 0x%X v%d", pkt->magic, pkt->version);
        return false;
    }

    // If we're the host and don't have a client yet, learn their address
    if (isHost && !connected) {
        remoteAddr = sender;
        connected  = true;
        char ipbuf[64] = {};
        inet_ntop(AF_INET, &sender.sin_addr, ipbuf, sizeof(ipbuf));
        Log("[Net] Client connected from %s:%d", ipbuf, ntohs(sender.sin_port));
    }

    // Update latency estimate
    uint32_t now = timeGetTime();
    latencyMs = now - pkt->timestamp;
    lastRecvTime = now;

    // Fill PlayerState
    out->posX = pkt->posX; out->posY = pkt->posY; out->posZ = pkt->posZ;
    out->spdX = pkt->spdX; out->spdY = pkt->spdY; out->spdZ = pkt->spdZ;
    out->fwdX = pkt->fwdX; out->fwdY = pkt->fwdY; out->fwdZ = pkt->fwdZ;
    out->upX  = pkt->upX;  out->upY  = pkt->upY;  out->upZ  = pkt->upZ;
    out->brick       = pkt->brick;
    out->whiteBricks = pkt->whiteBricks;
    out->flags       = pkt->flags;
    return true;
}
