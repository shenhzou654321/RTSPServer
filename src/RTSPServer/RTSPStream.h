#pragma once

#include "SmartPtr.h"
#include "TypeHandler.h"
#include "Socket.h"
#include "RTSPMessage.h"
#include "RTPPacket.h"

class RTSPServer;

enum ServerState
{
    ServerIdle,
    ServerSetup,
    ServerPlaying,
};


class RTSPStream
{
public:
    RTSPStream(TypeHandler *pHandler, unsigned int peerAddress, string session);

    string GetSDP(int track);
    string Setup(RTSPMessage *msg);
    int Play(RTSPMessage *msg);
    int Teardown();
    HRESULT OnData(const BYTE *pData, int cBytes, LONGLONG pts, LONGLONG baseNTP);

    void SendPacket(RTPPacket *pkt);
    LONGLONG PacketCount()
    {
        return m_cPackets;
    }

    long GetSSRC()
    {
        return m_ssrc;
    }

private:
    bool MakeSession(int rtp, int rtcp);

private:
    TypeHandler *m_pHandler; // weak
    LONGLONG m_rtpBase;
    LONGLONG m_ntpBase;
    long m_ssrc;
    string m_session;

    SmartPtr<CSocket> m_sRTP;
    SmartPtr<CSocket> m_sRTCP;
    SmartPtr<CSocket> m_sRecvRTCP;
    unsigned int m_peerAddress;
    int m_portRTP;
    int m_portRTCP;
    int m_portBase;

    ServerState m_state;

    LONGLONG m_cPackets;
    LONGLONG m_bytesSent;
    LONGLONG m_timeReported;
};
typedef SmartPtr<RTSPStream> RTSPStreamPtr;
