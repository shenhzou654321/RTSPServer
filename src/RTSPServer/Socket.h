#pragma once


#include "Thread.h"
#include "SmartPtr.h"

class WSAInit
{
public:
    WSAInit()
    {
        WSAData data;
        WSAStartup(MAKEWORD(2, 0), &data);
    }

    ~WSAInit()
    {
        WSACleanup();
    }
};

class CSocket;

class ISocketEvent
{
public:
    virtual void OnAcceptReady(CSocket *s) {}
    virtual void OnReadReady(CSocket *s) {}
    virtual void OnClose(CSocket *s) {}
};

class CSocket : public Thread
{
public:
    CSocket(SOCKET s);
    CSocket(int af, int type, int protocol);
    ~CSocket();

    HRESULT SetEvents(long lEvents, ISocketEvent *pNotify);
    SOCKET getSocket()
    {
        return m_s;
    }

    HRESULT setAddress(unsigned int addr, short port);
    HRESULT acceptConnection(CSocket **pps);
    ByteArrayT readData();
    HRESULT sendData(const BYTE *pData, int cBytes);
    HRESULT sendTo(const BYTE *pData, int cBytes, unsigned int addr, int port);

    SOCKET Detach()
    {
        SOCKET s = m_s;
        m_s = INVALID_SOCKET;
        return s;
    }

protected:
    DWORD ThreadProc();

protected:
    char m_readbuffer[32 * 1024];
    SOCKET m_s;
    WSAEVENT m_ev;
    ISocketEvent *m_pNotify;
};

