#include "StdAfx.h"
#include "RTSPClientConn.h"
#include "NALUnit.h"
#include "RTSPServer.h"

RTSPClientConnection::RTSPClientConnection() :
    m_pServer(NULL),
    m_ntpBase(0)
{
}

void RTSPClientConnection::Shutdown()
{
    LOG(("Client shutdown"));

    m_s->SetEvents(0, NULL);
    m_s = NULL;
    for (vector<RTSPStreamPtr>::iterator it = m_Streams.begin(); it != m_Streams.end(); it++)
    {
        RTSPStreamPtr p = *it;
        p->Teardown();
    }
    m_Streams.clear();
}

void RTSPClientConnection::SetSocket(SOCKET s, RTSPServer *server)
{
    CAutoLock lock(&m_csConnection);
    sockaddr_in addr;
    int cAddr = sizeof(addr);
    getpeername(s, (sockaddr *)&addr, &cAddr);
    unsigned int peer = addr.sin_addr.S_un.S_addr;

    ostringstream strm;
    strm << rand();
    m_session = strm.str();

    int tracks = server->GetStreamCount();
    for (int i = 0; i < tracks; i++)
    {
        TypeHandler *pHandler = server->GetHandler(i);
        RTSPStream *pStream = new RTSPStream(pHandler, peer, m_session);
        m_Streams.push_back(pStream);
    }

    m_s = new CSocket(s);
    m_pServer = server;
    m_s->SetEvents(FD_READ | FD_CLOSE, this);
}

void RTSPClientConnection::OnClose(CSocket *s)
{
    Shutdown();
    m_pServer->ShutdownConnection(this);
}

void RTSPClientConnection::OnReadReady(CSocket *s)
{
    CAutoLock lock(&m_csConnection);
    ByteArrayT data = s->readData();
    if (data.Length() == 0)
    {
        LOG(("zero-length read"));
        return;
    }
    string str((const char *) data.Data(), data.Length());
    RTSPMessage msg(str);

    LOG(("Request: %s", str.c_str()));

    ostringstream response;
    if (caselesscompare(msg.Request(), string("options")))
    {
        response << msg.CreateResponse(200, "OK");
        response << "Server: GDCL DShowRTSP/1.0\r\nPublic: DESCRIBE, SETUP, TEARDOWN, PLAY, OPTIONS\r\n\r\n";
    }
    else if (caselesscompare(msg.Request(), string("describe")))
    {
        string sdp = MakeSDP();
        // !! content-base, date
        response << msg.CreateResponse(200, "OK") << "Content-Type: application/sdp\r\nContent-Length: ";
        response << sdp.length() << "\r\n\r\n" << sdp;
    }
    else if (caselesscompare(msg.Request(), "setup"))
    {
        if (msg.URLTarget() == 0)
        {
            // must setup per-stream
            response << msg.CreateResponse(459, "Aggregate Operation not allowed");
        }
        else if (msg.URLTarget() > (int)m_Streams.size())
        {
            response << msg.CreateResponse(451, "Invalid URL target");
        }
        else
        {
            RTSPStream *stream = m_Streams[msg.URLTarget() - 1];
            response << stream->Setup(&msg);
        }
    }
    else if (caselesscompare(msg.Request(), "play"))
    {
        if (msg.URLTarget() <= (int) m_Streams.size())
        {
            int result = 451;
            if (msg.URLTarget() == 0)
            {
                for (vector<RTSPStreamPtr>::iterator it = m_Streams.begin(); it != m_Streams.end(); it++)
                {
                    RTSPStreamPtr p = *it;
                    result = p->Play(&msg);
                    if (result != 200)
                    {
                        break;
                    }
                }
            }
            else
            {
                RTSPStream *stream = m_Streams[msg.URLTarget() - 1];
                result = stream->Play(&msg);
            }
            if (result != 200)
            {
                response << msg.CreateResponse(result, "Wrong State");
            }
            else
            {
                response << msg.CreateResponse(result, "OK") << "Session: " << m_session << "\r\n\r\n";
            }
        }
        else
        {
            // per-stream play for invalid stream
            response << msg.CreateResponse(451, "Invalid URL target");
        }
    }
    else if (caselesscompare(msg.Request(), "teardown"))
    {
        if (msg.URLTarget() == 0)
        {
            int res = 200;
            for (vector<RTSPStreamPtr>::iterator it = m_Streams.begin(); it != m_Streams.end(); it++)
            {
                RTSPStreamPtr p = *it;
                res = p->Teardown();
                if (res != 200)
                {
                    break;
                }
            }
            response << msg.CreateResponse(res, (res == 200) ? "OK" : "Wrong state");
        }
        else if (msg.URLTarget() > (int)m_Streams.size())
        {
            response << msg.CreateResponse(451, "Invalid URL target");
        }
        else
        {
            RTSPStream *stream = m_Streams[msg.URLTarget() - 1];
            int ret = stream->Teardown();
            response << msg.CreateResponse(ret, (ret == 200) ? "OK" : "Invalid state");
        }
    }
    else
    {
        response << msg.CreateResponse(451, "Method not recognised");
    }
    str = response.str();
    if (str.length() > 0)
    {
        LOG(("Response: %s", str.c_str()));
        send(m_s->getSocket(), str.c_str(), str.length(), 0);
    }
}

string RTSPClientConnection::MakeSDP()
{
    sockaddr_in sa;
    int sz = sizeof(sa);
    getsockname(m_s->getSocket(), (sockaddr *)&sa, &sz);

    ostringstream ssSDP;
    ULONG verid = rand();
    ssSDP << "v=0\r\no=- " << verid << " " << verid << " IN IP4 ";
    ssSDP << inet_ntoa(sa.sin_addr) << "\r\n";
    ssSDP << "s=Live Stream from DShow\r\nc=IN IP4 0.0.0.0\r\nt=0 0\r\n";
    ssSDP << "a=control:*\r\n";
    for (size_t i = 0; i < m_Streams.size(); i++)
    {
        ssSDP << m_Streams[i]->GetSDP(i);
    }

    string sdp = ssSDP.str();
    return sdp;
}

void RTSPClientConnection::OnData(int trackid, IMediaSample *pSample)
{
    CAutoLock lock(&m_csConnection);

    const BYTE *pES;
    pSample->GetPointer(const_cast<BYTE **>(&pES));
    long cBytes = pSample->GetActualDataLength();

    REFERENCE_TIME tStart = 0, tStop;
    pSample->GetTime(&tStart, &tStop);
    LONGLONG pts = BaselineAdjust(tStart);

    m_Streams[trackid]->OnData(pES, cBytes, pts, m_ntpBase);
}

LONGLONG Now()
{
    // system time as 100ns since Jan 1, 1601
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER li;
    li.HighPart = ft.dwHighDateTime;
    li.LowPart = ft.dwLowDateTime;
    return li.QuadPart;
}

ULONGLONG GetNTP(LONGLONG now)
{
    // system time as 100ns since Jan 1, 1601
    // -- convert to seconds as double
    double dNTP = double(now) / (1000 * 10000);

    // 1601 to 1900 is 299*365 days, plus 72 leap years (299/4 minus 1700, 1800)
    const LONGLONG DAYS_FROM_FILETIME_TO_NTP = 109207;
    LONGLONG offsetSeconds = DAYS_FROM_FILETIME_TO_NTP * 24 * 60 * 60;
    dNTP -= offsetSeconds;

    // return as 64-bit 32.32 fixed point
    double fp = (dNTP * (1LL << 32));

    // simple cast of ULONGLONG ntp = static_cast<ULONGLONG>(fp) fails on Visual Studio if
    // the value is greater than INT64_MAX. So half the value before casting.
    ULONGLONG ntp = static_cast<ULONGLONG>(fp / 2);
    ntp += ntp;

    return ntp;
}

LONGLONG RTSPClientConnection::BaselineAdjust(REFERENCE_TIME pts)
{
    // the first pts we see from our source is recorded as a
    // baseline, and we map that to the current NTP time.
    // we convert the pts to a RTP time relative to that.
    // Each stream will then add on its own random offset.
    // It can report the mapping from RTP to NTP since the
    // NTP base established here maps to the RTP random baseline
    // that the stream adds on to all times.
    if (m_ntpBase == 0)
    {
        LOG(("Baseline %d ms", long(pts / 10000)));
        m_ptsBase = pts;
        m_ntpBaseAsReftime = Now();
        m_ntpBase = GetNTP(m_ntpBaseAsReftime);
    }

    pts -= m_ptsBase;

    // baseline rtp: needs conversion to clock rate and random addition for each stream
    return pts;
}

