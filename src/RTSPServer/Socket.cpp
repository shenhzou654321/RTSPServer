#include "StdAfx.h"
#include "Socket.h"


CSocket::CSocket(SOCKET s) :
    m_s(s),
    m_ev(WSA_INVALID_EVENT)
{
}

CSocket::CSocket(int af, int type, int protocol) :
    m_ev(WSA_INVALID_EVENT),
    m_pNotify(NULL)
{
    m_s = socket(af, type, protocol);
}

CSocket::~CSocket()
{
    StopThread();
    if (m_ev != WSA_INVALID_EVENT)
    {
        WSACloseEvent(m_ev);
    }
    if (m_s != INVALID_SOCKET)
    {
        closesocket(m_s);
    }
}

HRESULT CSocket::SetEvents(long lEvents, ISocketEvent *pNotify)
{
    m_pNotify = pNotify;
    if (m_ev == WSA_INVALID_EVENT)
    {
        m_ev = WSACreateEvent();
    }
    if (lEvents == 0)
    {
        StopThread();
        return S_OK;
    }
    lEvents |= FD_CLOSE;
    int e = WSAEventSelect(m_s, m_ev, lEvents);
    if (!e)
    {
        StartThread();
        return S_OK;
    }
    return HRESULT_FROM_WIN32(WSAGetLastError());
}

HRESULT CSocket::setAddress(unsigned int addr, short port)
{
    struct sockaddr_in sa;
    sa.sin_addr.s_addr = addr;
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    if (!bind(m_s, (sockaddr *) &sa, sizeof(sa)))
    {
        listen(m_s, SOMAXCONN);
        return S_OK;
    }
    return HRESULT_FROM_WIN32(WSAGetLastError());
}

HRESULT CSocket::acceptConnection(CSocket **pps)
{
    sockaddr_in sa;
    int len = sizeof(sa);
    SOCKET s = accept(m_s, (sockaddr *)&sa, &len);
    if (s == INVALID_SOCKET)
    {
        *pps = NULL;
        return HRESULT_FROM_WIN32(WSAGetLastError());
    }
    *pps = new CSocket(s);
    return S_OK;
}

ByteArrayT CSocket::readData()
{
    int c = recv(m_s, m_readbuffer, sizeof(m_readbuffer), 0);
    if (c <= 0)
    {
        return ByteArrayT(NULL, 0);
    }

    ByteArrayT a((BYTE *)m_readbuffer, c);
    return a;
}

HRESULT CSocket::sendData(const BYTE *pData, int cBytes)
{
    if (send(m_s, (const char *) pData, cBytes, 0) == SOCKET_ERROR)
    {
        return HRESULT_FROM_WIN32(WSAGetLastError());
    }
    return S_OK;
}

HRESULT CSocket::sendTo(const BYTE *pData, int cBytes, unsigned int addr, int port)
{
    sockaddr_in to;
    to.sin_family = AF_INET;
    to.sin_port = htons((unsigned short)port);
    to.sin_addr.S_un.S_addr = addr;
    if (sendto(m_s, (const char *)pData, cBytes, 0, (sockaddr *)&to, sizeof(to)) == SOCKET_ERROR)
    {
        return HRESULT_FROM_WIN32(WSAGetLastError());
    }
    return S_OK;
}


DWORD CSocket::ThreadProc()
{
    HANDLE hev[2] = {ExitEvent(), m_ev};
    while (!ShouldExit())
    {
        WSAWaitForMultipleEvents(2, hev, false, INFINITE, true);
        if (ShouldExit())
        {
            break;
        }
        WSANETWORKEVENTS netevents;
        if (!WSAEnumNetworkEvents(m_s, m_ev, &netevents))
        {
            if (m_pNotify)
            {
                if ((netevents.lNetworkEvents & FD_ACCEPT) &&
                        (netevents.iErrorCode[FD_ACCEPT_BIT] == 0))
                {
                    m_pNotify->OnAcceptReady(this);
                }
                if ((netevents.lNetworkEvents & FD_READ) &&
                        (netevents.iErrorCode[FD_READ_BIT] == 0))
                {
                    m_pNotify->OnReadReady(this);
                }
                if (netevents.lNetworkEvents & FD_CLOSE)
                {
                    m_pNotify->OnClose(this);
                    break;
                }
            }
        }
    }
    return 0;
}
