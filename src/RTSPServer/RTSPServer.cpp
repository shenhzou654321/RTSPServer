#include "StdAfx.h"
#include "RTSPServer.h"


RTSPServer::RTSPServer() :
    m_ps(NULL),
    m_bActive(false),
    m_bRunning(false)
{
    srand(timeGetTime());
}

bool RTSPServer::IsReady()
{
    CAutoLock lock(&m_csServer);
    for (size_t i = 0; i < m_Streams.size(); i++)
    {
        if (!m_Streams[i]->IsReady())
        {
            return false;
        }
    }
    return true;
}

HRESULT RTSPServer::StartServer()
{
    CAutoLock lock(&m_csServer);
    m_bActive = true;
    HRESULT hr = S_OK;
    if (IsReady())
    {
        LOG(("Starting server on active"));
        hr = SocketStart();
    }
    return hr;
}

HRESULT RTSPServer::SocketStart()
{
    m_ps = new CSocket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    int t = 1;
    setsockopt(m_ps->getSocket(), SOL_SOCKET, SO_REUSEADDR, (const char *)&t, sizeof(t));
    m_ps->setAddress(INADDR_ANY, 554);
    m_ps->SetEvents(FD_ACCEPT, this);
    m_bRunning = true;
    return S_OK;
}

HRESULT RTSPServer::ShutdownServer()
{
    CAutoLock lock(&m_csServer);
    LOG(("Server shutdown"));
    m_bActive = false;
    if (m_bRunning)
    {
        m_ps->SetEvents(0, NULL);
        m_ps = NULL;
        for (conn_list_t::iterator it = m_conns.begin(); it != m_conns.end(); it++)
        {
            LOG(("Connection shutdown"));
            RTSPClientConnectionPtr pConn = *it;
            pConn->Shutdown();
        }

        m_conns.clear();
        m_bRunning = false;
    }
    return S_OK;
}

HRESULT RTSPServer::DeleteStreams()
{
    CAutoLock lock(&m_csServer);
    if (m_bActive || (m_conns.size() > 0))
    {
        return VFW_E_WRONG_STATE;
    }
    m_Streams.clear();
    return S_OK;
}

HRESULT RTSPServer::AddStream(CMediaType *pmt, int *pTrackID)
{
    TypeHandlerPtr pH = TypeHandler::MakeHandler(pmt);
    if (!pH)
    {
        return VFW_E_TYPE_NOT_ACCEPTED;
    }
    m_Streams.push_back(pH);
    *pTrackID = (int)m_Streams.size() - 1;
    return S_OK;
}

HRESULT RTSPServer::OnData(int trackid, IMediaSample *pSample)
{
    CAutoLock lock(&m_csServer);
    if (!m_bActive)
    {
        return S_FALSE;
    }
    if (trackid > (int)m_Streams.size())
    {
        return E_INVALIDARG;
    }
    BYTE *pData;
    pSample->GetPointer(&pData);
    int cBytes = pSample->GetActualDataLength();
    REFERENCE_TIME tStart = 0, tEnd;
    pSample->GetTime(&tStart, &tEnd);

    if (!m_bRunning)
    {
        m_Streams[trackid]->Scan(pData, cBytes, tStart);
        if (IsReady())
        {
            LOG(("Server start on data parse complete"));
            SocketStart();
        }
    }
    if (m_bRunning)
    {
        for (conn_list_t::iterator it = m_conns.begin(); it != m_conns.end(); it++)
        {
            RTSPClientConnectionPtr pConn = *it;
            pConn->OnData(trackid, pSample);
        }
    }
    return S_OK;
}

HRESULT RTSPServer::ShutdownConnection(RTSPClientConnection *conn)
{
    CAutoLock lock(&m_csServer);

    // can't call Remove(object) since the smartptrs will not match
    for (conn_list_t::iterator it = m_conns.begin(); it != m_conns.end(); it++)
    {
        RTSPClientConnectionPtr pConn = *it;
        if (pConn == conn)
        {
            LOG(("Remove connection on close"));
            m_conns.erase(it);
            break;
        }
    }
    return S_OK;
}

void RTSPServer::OnAcceptReady(CSocket *s)
{
    CSocket *pClient;
    HRESULT hr = m_ps->acceptConnection(&pClient);
    if (SUCCEEDED(hr))
    {
        // construct RTSPClientConnection
        RTSPClientConnectionPtr pConn = new RTSPClientConnection();
        m_conns.push_back(pConn);
        m_conns.back()->SetSocket(pClient->Detach(), this);
    }
}

