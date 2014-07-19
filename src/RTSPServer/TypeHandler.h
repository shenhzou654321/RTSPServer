#pragma once


#include "SmartPtr.h"

class RTSPStream;

class TypeHandler
{
public:
    static TypeHandler *MakeHandler(const CMediaType *mt);

    TypeHandler();
    virtual ~TypeHandler() {}

    virtual HRESULT Scan(const BYTE *pData, int cBytes, REFERENCE_TIME tStart);
    virtual bool IsReady();
    virtual int Bitrate()
    {
        return m_bitrate;
    }

    virtual string GetSDP(int track) = 0;
    virtual HRESULT WriteData(const BYTE *pData, int cBytes, LONGLONG rtp, RTSPStream *pSender) = 0;
    virtual unsigned int ClockRate()
    {
        return 90000;
    }

    virtual unsigned int PayloadType()
    {
        return m_payloadtype;
    }

protected:
    string GetSDPCommon(int track);

protected:
    int m_bitrate;
    unsigned int m_payloadtype;

    // estimation of bitrate from first second
    REFERENCE_TIME m_first;
    long m_cBytesSinceFirst;
};

typedef SmartPtr<TypeHandler> TypeHandlerPtr;
