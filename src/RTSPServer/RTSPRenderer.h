#pragma once

#include "RTSPServer.h"

class RTSPRenderer;

// audio input pin
class AudioInput : public CBaseInputPin
{
public:
    AudioInput(RTSPRenderer *pFilter, CCritSec *pLock, HRESULT *phr);

    HRESULT CheckMediaType(const CMediaType *pmt);
    STDMETHODIMP EndOfStream();
    STDMETHODIMP BeginFlush();
    STDMETHODIMP Receive(IMediaSample *pSample);
    CMediaType CurrentMediaType()
    {
        return m_mt;
    }

private:
    RTSPRenderer *m_pRenderer;
};

class DECLSPEC_UUID("EABF2C99-F9AD-43CD-8108-109D4E9FADC5")
    RTSPRenderer : public CBaseRenderer
{
public:
    static CUnknown *WINAPI CreateInstance(LPUNKNOWN pUnk, HRESULT *phr);

    // filter registration tables
    static const AMOVIESETUP_MEDIATYPE m_sudType[];
    static const AMOVIESETUP_PIN m_sudPin[];
    static const AMOVIESETUP_FILTER m_sudFilter;

    HRESULT DoRenderSample(IMediaSample *pSample);
    HRESULT CheckMediaType(const CMediaType *pmt);
    HRESULT SetMediaType(const CMediaType *pmt);

    HRESULT OnStartStreaming();
    HRESULT OnStopStreaming();

    // override to add an audio pin
    int GetPinCount();
    CBasePin *GetPin(int n);
    STDMETHODIMP Stop();
    HRESULT SendEndOfStream();

    // called from audio pin
    void FlushAudio();
    void AudioEOS();
    HRESULT OnAudioSample(IMediaSample *pSample);

private:
    RTSPRenderer(LPUNKNOWN pUnk, HRESULT *phr);
    ~RTSPRenderer();

private:
    RTSPServer m_rtsp;
    CMediaType m_mt;
    int m_videoID;

    int m_audioID;
    AudioInput *m_pAudio;
    bool m_VideoAtEOS;
    bool m_AudioAtEOS;
    CCritSec m_csAudio;
    sample_list_t m_AudioSamples;
};
