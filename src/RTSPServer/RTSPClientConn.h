#pragma once

#include "Socket.h"
#include "SmartPtr.h"
#include "RTSPMessage.h"
#include "RTPPacket.h"
#include "TypeHandler.h"
#include "RTSPStream.h"

class RTSPServer;

LONGLONG Now();
ULONGLONG GetNTP(LONGLONG now);


class RTSPClientConnection : public ISocketEvent
{
public:
    RTSPClientConnection();

    void SetSocket(SOCKET s, RTSPServer *server);

    void Shutdown();
    void OnData(int trackid, IMediaSample *pSample);

    void OnReadReady(CSocket *s);
    void OnClose(CSocket *s);
    string GetSessionID()
    {
        return m_session;
    }

private:
    bool MakeSession(int rtp, int rtcp);
    string MakeSDP();
    LONGLONG BaselineAdjust(REFERENCE_TIME tStart);

private:
    CCritSec m_csConnection;
    SmartPtr<CSocket> m_s;
    RTSPServer *m_pServer;
    string m_session;

    vector<RTSPStreamPtr> m_Streams;

    ULONGLONG m_ntpBase;
    LONGLONG m_ntpBaseAsReftime;
    LONGLONG m_ptsBase;
};

typedef SmartPtr<RTSPClientConnection> RTSPClientConnectionPtr;
