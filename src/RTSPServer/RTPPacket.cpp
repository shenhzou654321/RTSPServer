#include "StdAfx.h"
#include "RTPPacket.h"

void tonet_short(BYTE *p, unsigned short s)
{
    p[0] = (s >> 8) & 0xff;
    p[1] = s & 0xff;
}
void tonet_long(BYTE *p, unsigned long l)
{
    p[0] = (l >> 24) & 0xff;
    p[1] = (l >> 16) & 0xff;
    p[2] = (l >> 8) & 0xff;
    p[3] = l & 0xff;
}

void RTPPacket::WriteHeader(bool bMarker, long seq, LONGLONG rtpTime)
{
    m_packet[0] = 0x80;	 // v=2
    m_packet[1] = BYTE(m_payloadtype) | (bMarker ? 0x80 : 0);
    tonet_short(m_packet + 2, (short)seq);
    tonet_long(m_packet + 4, (ULONG)rtpTime);
    tonet_long(m_packet + 8, m_ssrc);
}

HRESULT RTPPacket::SinglePacket(const BYTE *pData, int cBytes, bool bMarker, long seq, LONGLONG rtpTime)
{
    if (cBytes > SinglePacketSize())
    {
        return VFW_E_BUFFER_OVERFLOW;
    }

    WriteHeader(bMarker, seq, rtpTime);
    CopyMemory(m_packet + rtp_header_size, pData, cBytes);
    m_length = rtp_header_size + cBytes;
    return S_OK;
}

HRESULT RTPPacket::Append(const BYTE *pData, int cBytes)
{
    if ((cBytes + m_length) > max_packet_size)
    {
        LOG(("RTP Packet overflow"));
        return VFW_E_BUFFER_OVERFLOW;
    }

    CopyMemory(m_packet + m_length, pData, cBytes);
    m_length += cBytes;
    return S_OK;
}

HRESULT RTPPacket::StartFragments(BYTE nalu)
{
    m_fragmentHeader = nalu;
    m_bFirst = true;
    return S_OK;
}

HRESULT RTPPacket::Fragment(const BYTE *pData, int cBytes, bool bEndFragment, bool bMarker, long seq, LONGLONG rtpTime)
{
    if (cBytes > FragmentSize())
    {
        LOG(("RTP Packet overflow"));
        return VFW_E_BUFFER_OVERFLOW;
    }

    WriteHeader((bMarker && bEndFragment), seq, rtpTime);
    BYTE *pDest = m_packet + rtp_header_size;
    pDest[0] = (m_fragmentHeader & 0xe0) + 28; // FU_A packet type
    BYTE naltype = m_fragmentHeader & 0x1f;

    if (m_bFirst)
    {
        m_bFirst = false;
        naltype |= 0x80;
    }
    else if (bEndFragment)
    {
        naltype |= 0x40;
    }

    pDest[1] = naltype;
    CopyMemory(pDest + 2, pData, cBytes);
    m_length = rtp_header_size + 2 + cBytes;
    return S_OK;
}
