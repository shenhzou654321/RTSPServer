#pragma once

#include "Socket.h"
#include "SmartPtr.h"
#include "RTSPStream.h"
#include "RTSPClientConn.h"

typedef std::list<RTSPClientConnectionPtr> conn_list_t;

class RTSPServer : public ISocketEvent
{
public:
    RTSPServer();

    HRESULT DeleteStreams();
    HRESULT AddStream(CMediaType *pmt, int *pTrackID);
    static bool CanSupport(const CMediaType *pmt)
    {
        TypeHandlerPtr pH = TypeHandler::MakeHandler(pmt);
        if (pH)
        {
            return true;
        }
        return false;
    }

    HRESULT StartServer();
    HRESULT ShutdownServer();
    HRESULT OnData(int TrackID, IMediaSample *pSample);

    HRESULT ShutdownConnection(RTSPClientConnection *conn);

    int GetStreamCount()
    {
        return m_Streams.size();
    }
    TypeHandler *GetHandler(int n)
    {
        return m_Streams[n];
    }

    virtual void OnAcceptReady(CSocket *s);

private:
    bool IsReady();
    HRESULT SocketStart();

private:
    WSAInit m_wsa_init;
    CCritSec m_csServer;
    bool m_bActive;
    bool m_bRunning;
    CSocket *m_ps;
    conn_list_t m_conns;
    vector<TypeHandlerPtr> m_Streams;
};
