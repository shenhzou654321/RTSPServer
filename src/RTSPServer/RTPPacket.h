#pragma once

const int max_packet_size = 1514 - 42; // observed from Wireshark
const int rtp_header_size = 12;

void tonet_short(BYTE *p, unsigned short s);
void tonet_long(BYTE *p, unsigned long l);

class RTPPacket
{
public:
    RTPPacket(long ssrc, unsigned int payloadtype) :
        m_length(0),
        m_fragmentHeader(0),
        m_bFirst(false),
        m_ssrc(ssrc),
        m_payloadtype(payloadtype)
    {}

    static int SinglePacketSize()
    {
        return max_packet_size - rtp_header_size;
    }

    static int FragmentSize()
    {
        return SinglePacketSize() - 2;
    }

    int Length()
    {
        return m_length;
    }

    const BYTE *Packet()
    {
        return m_packet;
    }

    HRESULT SinglePacket(const BYTE *pData, int cBytes, bool bMarker, long seq, LONGLONG rtpTime);
    HRESULT StartFragments(BYTE nalu);
    HRESULT Fragment(const BYTE *pData, int cBytes, bool bEndFragment, bool bMarker, long seq, LONGLONG rtpTime);
    HRESULT Append(const BYTE *pData, int cBytes);

private:
    void WriteHeader(bool bMarker, long seq, LONGLONG rtpTime);

private:
    BYTE m_packet[max_packet_size];
    int m_length;
    BYTE m_fragmentHeader;
    bool m_bFirst;
    long m_ssrc;
    unsigned int m_payloadtype;
};
