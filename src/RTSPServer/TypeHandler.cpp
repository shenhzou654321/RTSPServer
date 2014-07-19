#include "StdAfx.h"
#include "TypeHandler.h"
#include <dvdmedia.h>
#include "NALUnit.h"
#include "RTPPacket.h"
#include "RTSPMessage.h"
#include "RTSPStream.h"


class H264Handler : public TypeHandler
{
public:
    H264Handler(const CMediaType *pmt);
    HRESULT Scan(const BYTE *pData, int cBytes, REFERENCE_TIME tStart);
    bool IsReady();
    string GetSDP(int i);
    HRESULT WriteData(const BYTE *pData, int cBytes, LONGLONG rtp, RTSPStream *pSender);

    static bool IsValid(const CMediaType *pmt);

private:
    CMediaType m_mt;

    bool m_bBSF;
    int m_lengthSize;
    ByteArrayT m_sps;
    ByteArrayT m_pps;
};

class AACHandler : public TypeHandler
{
public:
    AACHandler(const CMediaType *pmt);

    string GetSDP(int i);
    HRESULT WriteData(const BYTE *pData, int cBytes, LONGLONG rtp, RTSPStream *pSender);
    static bool IsValid(const CMediaType *pmt);


private:
    CMediaType m_mt;
    bool m_bADTS;
    BYTE m_config[2];
};

//static
TypeHandler *TypeHandler::MakeHandler(const CMediaType *mt)
{
    if (H264Handler::IsValid(mt))
    {
        return new H264Handler(mt);
    }
    if (AACHandler::IsValid(mt))
    {
        return new AACHandler(mt);
    }
    return NULL;
}

TypeHandler::TypeHandler() :
    m_bitrate(0),
    m_first(-1),
    m_payloadtype(96)
{
}

HRESULT TypeHandler::Scan(const BYTE *pData, int cBytes, REFERENCE_TIME tStart)
{
    if (m_bitrate == 0)
    {
        if (tStart != 0)
        {
            if (m_first == -1)
            {
                m_first = tStart;
                m_cBytesSinceFirst = cBytes;
            }
            else if ((tStart - m_first) < UNITS)
            {
                m_cBytesSinceFirst += cBytes;
            }
            else
            {
                double bytes = m_cBytesSinceFirst;
                double bits = 8 * bytes * UNITS / (tStart - m_first);
                m_bitrate = (int)bits;
            }
        }
    }
    return S_OK;
}

bool TypeHandler::IsReady()
{
    if (m_bitrate == 0)
    {
        return false;
    }
    return true;
}

string TypeHandler::GetSDPCommon(int track)
{
    int packets = Bitrate() / (RTPPacket::FragmentSize() * 8) + 1;

    ostringstream ssSDP;
    ssSDP << "b=TIAS:" << Bitrate() << "\r\n";
    ssSDP << "a=maxprate:" << packets << ".000\r\n";
    ssSDP << "a=control:streamid=" << track + 1 << "\r\n";

    return ssSDP.str();
}


// --- h264 ---------------

// Broadcom/Cyberlink Byte-Stream H264 subtype
// CLSID_H264
class DECLSPEC_UUID("8D2D71CB-243F-45E3-B2D8-5FD7967EC09B") CLSID_H264_BSF;

// static
bool H264Handler::IsValid(const CMediaType *pmt)
{

    FOURCCMap x264(DWORD('462x'));
    FOURCCMap H264(DWORD('462H'));
    FOURCCMap h264(DWORD('462h'));
    FOURCCMap AVC1(DWORD('1CVA'));
    FOURCCMap avc1(DWORD('1cva'));

    // H264
    if ((*pmt->Subtype() == x264) ||
            (*pmt->Subtype() == H264) ||
            (*pmt->Subtype() == h264) ||
            (*pmt->Subtype() == AVC1) ||
            (*pmt->Subtype() == avc1) ||
            (*pmt->Subtype() == __uuidof(CLSID_H264_BSF)))
    {
        // BSF
        if ((*pmt->FormatType() == FORMAT_VideoInfo) || (*pmt->FormatType() == FORMAT_VideoInfo2))
        {
            return true;
        }
        // length-prepended
        if (*pmt->FormatType() == FORMAT_MPEG2Video)
        {
            return true;
        }
    }
    return false;

}

H264Handler::H264Handler(const CMediaType *pmt)
    : m_mt(*pmt),
      m_bBSF(true),
      m_lengthSize(0)
{
    // length-prepended
    if (*pmt->FormatType() == FORMAT_MPEG2Video)
    {
        // check that the length field is 1-4. This is stored in dwFlags
        MPEG2VIDEOINFO *pvi = (MPEG2VIDEOINFO *)pmt->Format();
        if ((pvi->dwFlags > 0) && (pvi->dwFlags <= 4))
        {
            m_bBSF = false;
            // this is not an avcC. The lengthSize is stored in dwFlags, and
            // the dwSequenceHeader field contains param sets with two-byte lengths
            m_lengthSize = pvi->dwFlags;
            const BYTE *pSrc = (const BYTE *)(&pvi->dwSequenceHeader);
            const BYTE *pEnd = pSrc + pvi->cbSequenceHeader;

            int cSPS = (pSrc[0] << 8) + pSrc[1];
            pSrc += 2;
            if ((pSrc + cSPS + 2) < pEnd)
            {
                m_sps.Assign(pSrc, cSPS);
                pSrc += cSPS;
                int cPPS =  (pSrc[0] << 8) + pSrc[1];
                pSrc += 2;
                if ((pSrc + cPPS) <= pEnd)
                {
                    m_pps.Assign(pSrc, cPPS);
                }
            }
        }
    }
}
HRESULT H264Handler::Scan(const BYTE *pData, int cBytes, REFERENCE_TIME tStart)
{
    const BYTE *pStart = pData;
    int cPacket = cBytes;

    if ((m_sps.Length() == 0) || (m_pps.Length() == 0))
    {
        // I think we can assume that each sample buffer contains a whole number
        // of NALUs.
        NALUnit nalu;
        while (nalu.Parse(pData, cBytes, m_lengthSize, true))
        {
            if (nalu.Type() == NALUnit::NAL_Sequence_Params)
            {
                m_sps.Assign(nalu.Start(), nalu.Length());
            }
            else if (nalu.Type() == NALUnit::NAL_Picture_Params)
            {
                m_pps.Assign(nalu.Start(), nalu.Length());
            }
            const BYTE *pNext = nalu.Start() + nalu.Length();
            cBytes -= long(pNext - pData);
            pData = pNext;
        }
    }

    return __super::Scan(pStart, cPacket, tStart);
}

bool H264Handler::IsReady()
{
    if ((m_sps.Length() == 0) || (m_pps.Length() == 0))
    {
        return false;
    }
    return __super::IsReady();
}

string H264Handler::GetSDP(int track)
{
    m_payloadtype = 96 + track;

    NALUnit naluSPS(m_sps.Data(), m_sps.Length());
    SeqParamSet seq;
    seq.Parse(&naluSPS);

    // getting ostringstream to do this is like pulling teeth
    char ach[7];
    StringCbPrintfA(ach, sizeof(ach), "%02x%02x%02x", seq.Profile(), seq.Compat(), seq.Level());
    string profile_level_id = ach;
    string spsEnc = encodeToBase64(m_sps.Data(), m_sps.Length());
    string ppsEnc = encodeToBase64(m_pps.Data(), m_pps.Length());



    ostringstream ssSDP;

    ssSDP << "m=video 0 RTP/AVP " << m_payloadtype << "\r\n";

    ssSDP << GetSDPCommon(track);

    ssSDP << "a=rtpmap:" << m_payloadtype << " H264/90000\r\n";
    int cx = seq.EncodedWidth();
    int cy = seq.EncodedHeight();
    ssSDP << "a=framesize:" << m_payloadtype << " " << cx << "-" << cy << "\r\na=Width:integer;" << cx << "\r\na=Height:integer;" << cy << "\r\n";

    ssSDP << "a=fmtp:" << m_payloadtype << " packetization-mode=1;profile-level-id=" << profile_level_id << ";";
    ssSDP << "sprop-parameter-sets=" << spsEnc << "," << ppsEnc << "\r\n";

    return ssSDP.str();
}

HRESULT H264Handler::WriteData(const BYTE *pData, int cBytes, LONGLONG rtp, RTSPStream *pSender)
{
    RTPPacket packet(pSender->GetSSRC(), m_payloadtype);

    NALUnit nalu;
    while (nalu.Parse(pData, cBytes, m_lengthSize, true))
    {
        const BYTE *pNext = nalu.Start() + nalu.Length();
        cBytes -= long(pNext - pData);
        pData = pNext;
        bool bLast = (cBytes <= 0);

        if (nalu.Length() <= packet.SinglePacketSize())
        {
            packet.SinglePacket(nalu.Start(), nalu.Length(), bLast, (long) pSender->PacketCount(), rtp);
            pSender->SendPacket(&packet);
        }
        else
        {
            packet.StartFragments(*nalu.Start());
            int cBytes = nalu.Length() - 1;
            const BYTE *pData = nalu.Start() + 1;
            while (cBytes)
            {
                int cThis = min(cBytes, packet.FragmentSize());
                packet.Fragment(pData, cThis, (cThis == cBytes), bLast, (long)pSender->PacketCount(), rtp);
                cBytes -= cThis;
                pData += cThis;
                pSender->SendPacket(&packet);
            }
        }
    }

    return S_OK;
}

// --- AAC audio -----------

const int WAVE_FORMAT_AAC = 0x00ff;
const int WAVE_FORMAT_AACEncoder = 0x1234;
class DECLSPEC_UUID("2476CBD2-51B6-4593-B26F-637D90378FCA") AAC_ADTS_AUDIO;
const int WAVE_FORMAT_ADTS_AAC = 0x1600;

//static
bool AACHandler::IsValid(const CMediaType *pmt)
{
    if (*pmt->Type() == MEDIATYPE_Audio)
    {
        if (*pmt->FormatType() == FORMAT_WaveFormatEx)
        {
            WAVEFORMATEX *pwfx = (WAVEFORMATEX *)pmt->Format();
            if ((pwfx->wFormatTag == WAVE_FORMAT_AAC) ||
                    (pwfx->wFormatTag == WAVE_FORMAT_AACEncoder) ||
                    (pwfx->wFormatTag == WAVE_FORMAT_ADTS_AAC)
               )
            {
                return true;
            }

            // Intel Media SDK uses the 0xFF- aac subtype guid, but
            // the wFormatTag does not match
            FOURCCMap aac(WAVE_FORMAT_AAC);
            if ((*pmt->Subtype() == aac) ||
                    (*pmt->Subtype() == __uuidof(AAC_ADTS_AUDIO)))
            {
                return true;
            }
        }
    }
    return false;
}

const DWORD AACSamplingFrequencies[] =
{
    96000,
    88200,
    64000,
    48000,
    44100,
    32000,
    24000,
    22050,
    16000,
    12000,
    11025,
    8000,
    7350,
    0,
    0,
    0,
};
#ifndef SIZEOF_ARRAY
#define SIZEOF_ARRAY(a) (sizeof(a) / sizeof(a[0]))
#endif

AACHandler::AACHandler(const CMediaType *pmt)
    : m_mt(*pmt),
      m_bADTS(false)
{
    WAVEFORMATEX *pwfx = (WAVEFORMATEX *)m_mt.Format();
    if ((*pmt->Subtype() == __uuidof(AAC_ADTS_AUDIO)) ||
            (pwfx->wFormatTag == WAVE_FORMAT_ADTS_AAC))
    {
        m_bADTS = true;
    }

    BYTE *pExtra = m_mt.Format() + sizeof(WAVEFORMATEX);
    long cExtra = m_mt.FormatLength() - sizeof(WAVEFORMATEX);
    if (cExtra == 2)
    {
        CopyMemory(m_config, pExtra, 2);
    }
    else
    {
        long ObjectType = 2;
        long ChannelIndex = pwfx->nChannels;
        if (ChannelIndex == 8)
        {
            ChannelIndex = 7;
        }
        long RateIndex = 0;
        for (long i = 0; i < SIZEOF_ARRAY(AACSamplingFrequencies); i++)
        {
            if (AACSamplingFrequencies[i] == pwfx->nSamplesPerSec)
            {
                RateIndex = i;
                break;
            }
        }
        m_config[0] = (BYTE) ((ObjectType << 3) | ((RateIndex >> 1) & 7));
        m_config[1] = (BYTE) (((RateIndex & 1) << 7) | (ChannelIndex << 3));
    }
}

string AACHandler::GetSDP(int track)
{
    m_payloadtype = 96 + track;

    ostringstream sstrm;

    sstrm << "m=audio 0 RTP/AVP " << m_payloadtype << "\r\n";

    sstrm << GetSDPCommon(track);

    // two questions about this: should we use /90000 or /samplespersec/channels?
    // and: should the profile-level-id be the top 5 bits of config[0]?

    sstrm << "a=rtpmap:" << m_payloadtype << " mpeg4-generic/90000\r\n";
    sstrm << "a=fmtp:" << m_payloadtype << " profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;";

    char ach[5];
    StringCbPrintfA(ach, sizeof(ach), "%02x%02x", m_config[0], m_config[1]);
    sstrm << "config=" << ach << "\r\n";


    return sstrm.str();
}

HRESULT AACHandler::WriteData(const BYTE *pData, int cBytes, LONGLONG rtp, RTSPStream *pSender)
{
    RTPPacket packet(pSender->GetSSRC(), m_payloadtype);

    if (m_bADTS)
    {
        if ((pData[0] != 0xFF) || ((pData[1] & 0xF0) != 0xF0))
        {
            // should be a whole ADTS packet?
            LOG(("ADTS header missing"));
            return S_FALSE;
        }
        int len = ((pData[3] & 0x3) << 11) +
                  (pData[4] << 3) +
                  ((pData[5] & 0xe0) >> 5);
        int header = 7;
        if ((pData[1] & 1) == 0)
        {
            header = 9;
        }
        pData += header;
        if (len > cBytes)
        {
            LOG(("ADTS length invalid"));
            return S_FALSE;
        }
        cBytes = len - header;
    }
    BYTE AUHeader[4];
    AUHeader[0] = 0;
    AUHeader[1] = 16;	// header length in bytes
    AUHeader[2] = (cBytes >> 5) & 0xff;
    AUHeader[3] = (cBytes << 3) & 0xF8;
    int space = packet.SinglePacketSize() - 4;
    HRESULT hr = S_OK;
    while (cBytes)
    {
        int cThis = min(space, cBytes);
        bool bMarker = (cThis == cBytes);
        hr = packet.SinglePacket(AUHeader, 4, bMarker, (long) pSender->PacketCount(), rtp);
        if (SUCCEEDED(hr))
        {
            hr = packet.Append(pData, cThis);
        }

        if (SUCCEEDED(hr))
        {
            pSender->SendPacket(&packet);
        }
        cBytes -= cThis;
        pData += cThis;
    }
    return hr;
}

