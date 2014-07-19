#include "StdAfx.h"
#include "RTSPStream.h"
#include "RTSPClientConn.h"


RTSPStream::RTSPStream(TypeHandler *pHandler, unsigned int peer, string session) :
    m_pHandler(pHandler),
    m_peerAddress(peer),
    m_session(session),
    m_state(ServerIdle),
    m_cPackets(0),
    m_ntpBase(0),
    m_rtpBase(0)
{
}

string RTSPStream::GetSDP(int track)
{
    return m_pHandler->GetSDP(track);
}

string RTSPStream::Setup(RTSPMessage *msg)
{
    ostringstream response;

    // find the client port in the transport params
    string transport = msg->ValueForOption("transport");
    vector<string> components = divideBy(transport, ";");
    vector<string> ports;
    for (vector<string>::iterator it = components.begin(); it != components.end(); it++)
    {
        string label = it->substr(0, 12);
        if (caselesscompare(label, "client_port="))
        {
            ports = divideBy(it->substr(12), "-");
            break;
        }
    }
    if (ports.size() == 2)
    {
        int portRTP = stoi(ports[0]);
        int portRTCP = stoi(ports[1]);
        if (MakeSession(portRTP, portRTCP))
        {
            response << msg->CreateResponse(200, "OK") << "Session: " << m_session  << "\r\n";
            response << "Transport: RTP/AVP;unicast;client_port=" << portRTP << "-" << portRTCP;
            response << ";server_port=" << m_portBase << "-" << m_portBase + 1 << "\r\n\r\n";
        }
    }
    if (response.str().length() == 0)
    {
        response << msg->CreateResponse(451, "transport setup failed");
    }
    return response.str();
}

int RTSPStream::Play(RTSPMessage *msg)
{
    if (m_state != ServerSetup)
    {
        return 451;
    }
    else
    {
        m_cPackets = 0;
        m_state = ServerPlaying;
        return 200;
    }
}

int RTSPStream::Teardown()
{
    int ret = 200;
    if (m_state != ServerIdle)
    {
        m_sRTP = NULL;
        m_sRTCP = NULL;
        m_sRecvRTCP = NULL;
        m_state = ServerIdle;
    }
    return ret;
}

HRESULT RTSPStream::OnData(const BYTE *pData, int cBytes, LONGLONG pts, LONGLONG ntpBase)
{
    if (m_state != ServerPlaying)
    {
        return S_OK;
    }

    while ((m_rtpBase == 0) || (ntpBase != m_ntpBase))
    {
        m_rtpBase = rand();
        m_ntpBase = ntpBase;
    }
    LONGLONG rtp = (pts * m_pHandler->ClockRate() / UNITS);
    rtp += m_rtpBase;

    return m_pHandler->WriteData(pData, cBytes, rtp, this);
}

bool RTSPStream::MakeSession(int rtp, int rtcp)
{
    m_sRTP = new CSocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    m_portRTP = rtp;

    m_sRTCP = new CSocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    m_portRTCP = rtcp;

    // we need to have a socket to receive RTCP receiver reports at
    // or some players refuse to play, but we don't need to do anything
    // with the data.
    m_sRecvRTCP = new CSocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    // by convention we supply a pair of consecutive port numbers, with the
    // odd ones used for RTCP. Keep trying until we find one that is not occupied
    HRESULT hr = E_FAIL;
    m_portBase = 6970;
    while (hr != S_OK)
    {
        hr = m_sRecvRTCP->setAddress(INADDR_ANY, (short)m_portBase + 1);
        if (FAILED(hr))
        {
            m_portBase += 2;
        }
    }

    m_state = ServerSetup;
    m_ssrc = rand();
    m_rtpBase = 0;

    m_cPackets = 0;
    m_bytesSent = 0;
    m_timeReported = 0;

    return true;
}

void RTSPStream::SendPacket(RTPPacket *pkt)
{
    if (m_sRTP)
    {
        m_sRTP->sendTo(pkt->Packet(), pkt->Length(), m_peerAddress, m_portRTP);
    }
    m_cPackets ++;
    m_bytesSent += pkt->Length();
    LONGLONG now = Now();
    if ((m_timeReported == 0) || ((now - m_timeReported) > (1000 * 10000)))
    {
        // create corresponding times for now, in NTP and RTP format
        LONGLONG ntpNow = GetNTP(now);
        double diff = double(ntpNow - m_ntpBase) / (1LL << 32);
        LONGLONG relative = static_cast<LONGLONG>(diff * UNITS);
        LONGLONG rtpNow = (relative * m_pHandler->ClockRate() / UNITS);
        rtpNow += m_rtpBase;

        const int sizeRTCP = 7 * sizeof(DWORD);
        BYTE buf[sizeRTCP];
        buf[0] = 0x80;
        buf[1] = 200; // SR
        tonet_short(buf + 2, 6); // length (count of uint32_t minus 1)
        tonet_long(buf + 4, m_ssrc);
        tonet_long(buf + 8, ULONG(ntpNow >> 32));
        tonet_long(buf + 12, ULONG(ntpNow));
        tonet_long(buf + 16, ULONG(rtpNow));
        tonet_long(buf + 20, ULONG(m_cPackets));
        tonet_long(buf + 24, ULONG(m_bytesSent));
        if (m_sRTCP)
        {
            m_sRTCP->sendTo(buf, sizeRTCP, m_peerAddress, m_portRTCP);
        }
        m_timeReported = now;
    }
}
