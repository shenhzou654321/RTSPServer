#include "StdAfx.h"
#include "NALUnit.h"
#include "RTSPRenderer.h"


Logger theLogger(TEXT("RTSPServer.txt"));

//static 
const AMOVIESETUP_MEDIATYPE RTSPRenderer::m_sudType[] = 
{
	{&MEDIATYPE_Video, &GUID_NULL},
};

//static 
const AMOVIESETUP_PIN RTSPRenderer::m_sudPin[] =
{
	{
		L"Video",           // pin name
			TRUE,              // is rendered?    
			FALSE,               // is output?
			FALSE,              // zero instances allowed?
			FALSE,              // many instances allowed?
			&CLSID_NULL,        // connects to filter (for bridge pins)
			NULL,               // connects to pin (for bridge pins)
			1,                  // count of registered media types
			&m_sudType[0]       // list of registered media types    
	},
};

//static 
const AMOVIESETUP_FILTER RTSPRenderer::m_sudFilter = 
{
	&__uuidof(RTSPRenderer),				// filter clsid
	L"RTSP Server",							// filter name
	MERIT_DO_NOT_USE,						// do not pull in automatically
	1,										// count of registered pins
	m_sudPin								// list of pins to register
};

// --- COM factory table and registration code --------------

// DirectShow base class COM factory requires this table, 
// declaring all the COM objects in this DLL
CFactoryTemplate g_Templates[] = {
	// one entry for each CoCreate-able object
	{
		RTSPRenderer::m_sudFilter.strName,
			RTSPRenderer::m_sudFilter.clsID,
			RTSPRenderer::CreateInstance,
			NULL,
			&RTSPRenderer::m_sudFilter
	},
};

int g_cTemplates = sizeof(g_Templates) / sizeof(g_Templates[0]);

// self-registration entrypoint
STDAPI DllRegisterServer()
{
	// base classes will handle registration using the factory template table
	HRESULT hr = AMovieDllRegisterServer2(true);

	return hr;
}

STDAPI DllUnregisterServer()
{
	// base classes will handle de-registration using the factory template table
	HRESULT hr = AMovieDllRegisterServer2(false);

	return hr;
}

// if we declare the correct C runtime entrypoint and then forward it to the DShow base
// classes we will be sure that both the C/C++ runtimes and the base classes are initialized
// correctly
extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);
BOOL WINAPI DllMain(HANDLE hDllHandle, DWORD dwReason, LPVOID lpReserved)
{
	return DllEntryPoint(reinterpret_cast<HINSTANCE>(hDllHandle), dwReason, lpReserved);
}


//static
CUnknown *WINAPI RTSPRenderer::CreateInstance(LPUNKNOWN pUnk, HRESULT *phr)
{
    return new RTSPRenderer(pUnk, phr);
}


RTSPRenderer::RTSPRenderer(LPUNKNOWN pUnk, HRESULT *phr) :
    m_VideoAtEOS(false),
    m_AudioAtEOS(false),
    CBaseRenderer(__uuidof(RTSPRenderer), NAME("RTSPSourceFilter"), pUnk, phr)
{
    m_pAudio = new AudioInput(this, &m_InterfaceLock, phr);
}

RTSPRenderer::~RTSPRenderer()
{
    delete m_pAudio;
}

HRESULT RTSPRenderer::CheckMediaType(const CMediaType *pmt)
{
    if ((*pmt->Type() == MEDIATYPE_Video) &&
            RTSPServer::CanSupport(pmt))
    {
        return S_OK;
    }
    return VFW_E_TYPE_NOT_ACCEPTED;
}

HRESULT RTSPRenderer::SetMediaType(const CMediaType *pmt)
{
    HRESULT hr = CheckMediaType(pmt);
    if (FAILED(hr))
    {
        return hr;
    }

    m_mt = *pmt;
    return __super::SetMediaType(pmt);
}

HRESULT RTSPRenderer::DoRenderSample(IMediaSample *pSample)
{
    // feed any audio that's ready
    REFERENCE_TIME tStart, tEnd;
    if (pSample->GetTime(&tStart, &tEnd) == S_OK)
    {
        for (;;)
        {
            IMediaSamplePtr pAudio;
            {
                CAutoLock lock(&m_csAudio);
                if (!m_AudioSamples.empty())
                {
                    REFERENCE_TIME tAudioStart, tAudioEnd;
                    IMediaSamplePtr p = m_AudioSamples.front();
                    if ((p->GetTime(&tAudioStart, &tAudioEnd) == S_OK) && (tEnd >= tAudioStart))
                    {
                        pAudio = p;
                        m_AudioSamples.pop_front();
                    }
                }
            }
            if (pAudio)
            {
                m_rtsp.OnData(m_audioID, pAudio);
                pAudio = NULL;
            }
            else
            {
                break;
            }
        }
    }

    // feed video to server
    m_rtsp.OnData(m_videoID, pSample);
    return S_OK;
}

HRESULT RTSPRenderer::OnStartStreaming()
{
    m_rtsp.DeleteStreams();
    HRESULT hr = m_rtsp.AddStream(&m_mt, &m_videoID);
    if (SUCCEEDED(hr) && m_pAudio->IsConnected())
    {
        CMediaType mt = m_pAudio->CurrentMediaType();
        hr = m_rtsp.AddStream(&mt, &m_audioID);
    }
    if (FAILED(hr))
    {
        return hr;
    }
    hr = m_rtsp.StartServer();
    if (FAILED(hr))
    {
        return hr;
    }
    return S_OK;
}

HRESULT RTSPRenderer::OnStopStreaming()
{
    m_rtsp.ShutdownServer();
    m_AudioSamples.clear();

    return S_OK;
}

int RTSPRenderer::GetPinCount()
{
    return 2;
}

CBasePin *RTSPRenderer::GetPin(int n)
{
    if (n == 1)
    {
        return m_pAudio;
    }
    return __super::GetPin(n);
}

STDMETHODIMP RTSPRenderer::Stop()
{
    m_VideoAtEOS = false;
    m_AudioAtEOS = false;
    FlushAudio();

    return __super::Stop();
}

HRESULT RTSPRenderer::SendEndOfStream()
{
    // this method is misnamed. It should be called SendEOS_IfNeeded -- it is called every frame.
    CAutoLock lock(&m_RendererLock);
    if (!m_bEOS || m_bEOSDelivered || m_EndOfStreamTimer)
    {
        return S_OK;
    }

    m_VideoAtEOS = true;
    if (!m_AudioAtEOS)
    {
        // wait for audio
        return S_OK;
    }
    return __super::SendEndOfStream();
}

void RTSPRenderer::FlushAudio()
{
    CAutoLock lock(&m_csAudio);
    m_AudioSamples.clear();
}

void RTSPRenderer::AudioEOS()
{
    CAutoLock lock(&m_RendererLock);
    m_AudioAtEOS = true;
    if (m_VideoAtEOS)
    {
        __super::SendEndOfStream();
    }
}

HRESULT RTSPRenderer::OnAudioSample(IMediaSample *pSample)
{
    CAutoLock lock(&m_csAudio);
    IMediaSamplePtr ptr = pSample;
    m_AudioSamples.push_back(ptr);
    return S_OK;
}

// ----- audio pin methods

AudioInput::AudioInput(RTSPRenderer *pFilter, CCritSec *pLock, HRESULT *phr) :
    m_pRenderer(pFilter),
    CBaseInputPin(NAME("AudioInput"), pFilter, pLock, phr, L"Audio In")
{
}

HRESULT AudioInput::CheckMediaType(const CMediaType *pmt)
{
    if ((*pmt->Type() == MEDIATYPE_Audio) &&
            RTSPServer::CanSupport(pmt))
    {
        return S_OK;
    }
    return VFW_E_TYPE_NOT_ACCEPTED;
}

STDMETHODIMP AudioInput::EndOfStream()
{
    m_pRenderer->AudioEOS();
    return S_OK;
}

STDMETHODIMP AudioInput::BeginFlush()
{
    m_pRenderer->FlushAudio();
    return __super::BeginFlush();
}

STDMETHODIMP AudioInput::Receive(IMediaSample *pSample)
{
    HRESULT hr = __super::Receive(pSample);
    if (SUCCEEDED(hr))
    {
        hr = m_pRenderer->OnAudioSample(pSample);
    }
    return hr;
}
