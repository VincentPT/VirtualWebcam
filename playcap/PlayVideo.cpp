#include "stdafx.h"
#include "PlayVideo.h"
#include <streams.h>

#include <windows.h>
#include <dshow.h>
#include <stdio.h>
#include <strsafe.h>
#include <comdef.h>

// An application can advertise the existence of its filter graph
// by registering the graph with a global Running Object Table (ROT).
// The GraphEdit application can detect and remotely view the running
// filter graph, allowing you to 'spy' on the graph with GraphEdit.
//
// To enable registration in this sample, define REGISTER_FILTERGRAPH.
//
#define REGISTER_FILTERGRAPH

// {89B6E287-6752-45A5-A4EC-106B2C178A72}
static const GUID CLSID_MYTRANFORMFILTER =
{ 0x89b6e287, 0x6752, 0x45a5,{ 0xa4, 0xec, 0x10, 0x6b, 0x2c, 0x17, 0x8a, 0x72 } };


class CMyTranformFilter : public CTransformFilter
{
public:
    CMyTranformFilter(__inout_opt LPUNKNOWN punk, HRESULT* hr) :
        CTransformFilter("My Transform filter", punk, CLSID_MYTRANFORMFILTER)
    {

    }
    ~CMyTranformFilter() {
    }


    HRESULT Transform(IMediaSample * pIn, IMediaSample *pOut) {
        DbgLog((LOG_TRACE, 3, TEXT(__FUNCTION__)));
        return E_NOTIMPL;
    }

    // check if you can support mtIn
    HRESULT CheckInputType(const CMediaType* mtIn) {
        DbgLog((LOG_TRACE, 3, TEXT(__FUNCTION__)));
        return E_NOTIMPL;
    }

    // check if you can support the transform from this input to this output
    HRESULT CheckTransform(const CMediaType* mtIn, const CMediaType* mtOut) {
        DbgLog((LOG_TRACE, 3, TEXT(__FUNCTION__)));
        return E_NOTIMPL;
    }

    // this goes in the factory template table to create new instances
    // static CCOMObject * CreateInstance(__inout_opt LPUNKNOWN, HRESULT *);

    // call the SetProperties function with appropriate arguments
    HRESULT DecideBufferSize(
        IMemAllocator * pAllocator,
        __inout ALLOCATOR_PROPERTIES *pprop) {
        DbgLog((LOG_TRACE, 3, TEXT(__FUNCTION__)));
        return E_NOTIMPL;
    }

    // override to suggest OUTPUT pin media types
    HRESULT GetMediaType(int iPosition, __inout CMediaType *pMediaType) {
        DbgLog((LOG_TRACE, 3, TEXT(__FUNCTION__)));
        return E_NOTIMPL;
    }
};
/////////////////////////////////////////

//
// Global data
//
HWND ghApp = 0;
DWORD g_dwGraphRegister = 0;

IVideoWindow  * g_pVW = NULL;
IMediaControl * g_pMC = NULL;
IMediaEventEx * g_pME = NULL;
IGraphBuilder * g_pGraph = NULL;
ICaptureGraphBuilder2 * g_pCapture = NULL;
PLAYSTATE g_psCurrent = Stopped;

HRESULT GetInterfaces(void);
HRESULT SetupVideoWindow(void);
HRESULT ChangePreviewState(int nShow);
HRESULT HandleGraphEvent(void);
HRESULT AddGraphToRot(IUnknown *pUnkGraph, DWORD *pdwRegister);
void RemoveGraphFromRot(DWORD pdwRegister);

HRESULT CaptureVideo(IBaseFilter* captureDevice, GUID captureMode, int selectedFormat)
{
    HRESULT hr;
    IBaseFilter *pSrcFilter = captureDevice;

    // Get DirectShow interfaces
    hr = GetInterfaces();
    if (FAILED(hr))
    {
        Msg(TEXT("Failed to get video interfaces!  hr=0x%x"), hr);
        return hr;
    }

    // Attach the filter graph to the capture graph
    hr = g_pCapture->SetFiltergraph(g_pGraph);
    if (FAILED(hr))
    {
        Msg(TEXT("Failed to set capture filter graph!  hr=0x%x"), hr);
        return hr;
    }

    // Add Capture filter to our graph.
    hr = g_pGraph->AddFilter(pSrcFilter, L"Video Capture");
    if (FAILED(hr))
    {
        Msg(TEXT("Couldn't add the capture filter to the graph!  hr=0x%x\r\n\r\n")
            TEXT("If you have a working video capture device, please make sure\r\n")
            TEXT("that it is connected and is not being used by another application.\r\n\r\n")
            TEXT("The sample will now close."), hr);
        pSrcFilter->Release();
        return hr;
    }

    //IBaseFilter* filter = NULL;
    //hr = g_pGraph->FindFilterByName(L"My Transform filter", &filter);
    //if (filter) {
    //    filter->Release();
    //}

    //CMyTranformFilter* myFilter = new CMyTranformFilter(NULL, &hr);
    //hr = g_pGraph->AddFilter(myFilter, L"My Transform filter");
		
	IAMStreamConfig *_streamConf = NULL;

	// get stream again prefer to preview mode
	hr = g_pCapture->FindInterface(&captureMode, &MEDIATYPE_Video, captureDevice, IID_IAMStreamConfig, (void **)&_streamConf);
	if (FAILED(hr)) {
		return hr;
	}
	if (!_streamConf) {
		return E_FAIL;
	}

	int streamFormatSize = 0;
	int _streamCaps = 0;
	hr = _streamConf->GetNumberOfCapabilities(&_streamCaps, &streamFormatSize);
	if (FAILED(hr)) {
		return hr;
	}

	if (streamFormatSize != sizeof(VIDEO_STREAM_CONFIG_CAPS)) {
		return E_FAIL;
	}

	HRESULT tmpHr;
	VIDEO_STREAM_CONFIG_CAPS scc;
	AM_MEDIA_TYPE* pmt;
	bool formatSet = false;
	for (int i = 0; i < _streamCaps; ++i) {
		pmt = NULL;
		tmpHr = _streamConf->GetStreamCaps(i, &pmt, (BYTE*)&scc);
		if (SUCCEEDED(tmpHr)) {
			if (i == selectedFormat) {
				tmpHr = _streamConf->SetFormat(pmt);
				if (SUCCEEDED(hr)) {
					formatSet = true;
				}
				break;
			}

			DeleteMediaType(pmt);
		}
	}

	SAFE_RELEASE(_streamConf);

	if (!formatSet) {
		Msg(TEXT("Couldn't not set the selected format, let the stream be\r\n"));
	}

    // Render the preview pin on the video capture filter
    // Use this instead of g_pGraph->RenderFile
    hr = g_pCapture->RenderStream(&captureMode, &MEDIATYPE_Video,
        pSrcFilter, NULL, NULL);
    if (FAILED(hr))
    {
        Msg(TEXT("Couldn't render the video capture stream.  hr=0x%x\r\n")
            TEXT("The capture device may already be in use by another application.\r\n\r\n")
            TEXT("The sample will now close."), hr);
        pSrcFilter->Release();
        return hr;
    }

    // Now that the filter has been added to the graph and we have
    // rendered its stream, we can release this reference to the filter.
    pSrcFilter->Release();

    // Set video window style and position
    hr = SetupVideoWindow();
    if (FAILED(hr))
    {
        Msg(TEXT("Couldn't initialize video window!  hr=0x%x"), hr);
        return hr;
    }

#ifdef REGISTER_FILTERGRAPH
    // Add our graph to the running object table, which will allow
    // the GraphEdit application to "spy" on our graph
    hr = AddGraphToRot(g_pGraph, &g_dwGraphRegister);
    if (FAILED(hr))
    {
        Msg(TEXT("Failed to register filter graph with ROT!  hr=0x%x"), hr);
        g_dwGraphRegister = 0;
    }
#endif

    // Start previewing video data
    hr = g_pMC->Run();
    if (FAILED(hr))
    {
        Msg(TEXT("Couldn't run the graph!  hr=0x%x"), hr);
        return hr;
    }

    // Remember current state
    g_psCurrent = Running;

    return S_OK;
}


HRESULT GetInterfaces(void)
{
    HRESULT hr;

    // Create the filter graph
    hr = CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC,
        IID_IGraphBuilder, (void **)&g_pGraph);
    if (FAILED(hr))
        return hr;

    // Create the capture graph builder
    hr = CoCreateInstance(CLSID_CaptureGraphBuilder2, NULL, CLSCTX_INPROC,
        IID_ICaptureGraphBuilder2, (void **)&g_pCapture);
    if (FAILED(hr))
        return hr;

    // Obtain interfaces for media control and Video Window
    hr = g_pGraph->QueryInterface(IID_IMediaControl, (LPVOID *)&g_pMC);
    if (FAILED(hr))
        return hr;

    hr = g_pGraph->QueryInterface(IID_IVideoWindow, (LPVOID *)&g_pVW);
    if (FAILED(hr))
        return hr;

    hr = g_pGraph->QueryInterface(IID_IMediaEventEx, (LPVOID *)&g_pME);
    if (FAILED(hr))
        return hr;

    // Set the window handle used to process graph events
    hr = g_pME->SetNotifyWindow((OAHWND)ghApp, WM_GRAPHNOTIFY, 0);

    return hr;
}


void CloseInterfaces(void)
{
    // Stop previewing data
    if (g_pMC)
        g_pMC->StopWhenReady();

    g_psCurrent = Stopped;

    // Stop receiving events
    if (g_pME)
        g_pME->SetNotifyWindow(NULL, WM_GRAPHNOTIFY, 0);

    // Relinquish ownership (IMPORTANT!) of the video window.
    // Failing to call put_Owner can lead to assert failures within
    // the video renderer, as it still assumes that it has a valid
    // parent window.
    if (g_pVW)
    {
        g_pVW->put_Visible(OAFALSE);
        g_pVW->put_Owner(NULL);
    }

#ifdef REGISTER_FILTERGRAPH
    // Remove filter graph from the running object table   
    if (g_dwGraphRegister)
        RemoveGraphFromRot(g_dwGraphRegister);
#endif

    // Release DirectShow interfaces
    SAFE_RELEASE(g_pMC);
    SAFE_RELEASE(g_pME);
    SAFE_RELEASE(g_pVW);
    SAFE_RELEASE(g_pGraph);
    SAFE_RELEASE(g_pCapture);
}


HRESULT SetupVideoWindow(void)
{
    HRESULT hr;

    // Set the video window to be a child of the main window
    hr = g_pVW->put_Owner((OAHWND)ghApp);
    if (FAILED(hr))
        return hr;

    // Set video window style
    hr = g_pVW->put_WindowStyle(WS_CHILD | WS_CLIPCHILDREN);
    if (FAILED(hr))
        return hr;

    // Use helper function to position video window in client rect 
    // of main application window
    ResizeVideoWindow();

    // Make the video window visible, now that it is properly positioned
    hr = g_pVW->put_Visible(OATRUE);
    if (FAILED(hr))
        return hr;

    return hr;
}


void ResizeVideoWindow(void)
{
    // Resize the video preview window to match owner window size
    if (g_pVW)
    {
        RECT rc;

        // Make the preview video fill our window
        GetClientRect(ghApp, &rc);
        g_pVW->SetWindowPosition(0, 0, rc.right, rc.bottom);
    }
}


HRESULT ChangePreviewState(int nShow)
{
    HRESULT hr = S_OK;

    // If the media control interface isn't ready, don't call it
    if (!g_pMC)
        return S_OK;

    if (nShow)
    {
        if (g_psCurrent != Running)
        {
            // Start previewing video data
            hr = g_pMC->Run();
            g_psCurrent = Running;
        }
    }
    else
    {
        // Stop previewing video data
        hr = g_pMC->StopWhenReady();
        g_psCurrent = Stopped;
    }

    return hr;
}


#ifdef REGISTER_FILTERGRAPH

HRESULT AddGraphToRot(IUnknown *pUnkGraph, DWORD *pdwRegister)
{
    IMoniker * pMoniker;
    IRunningObjectTable *pROT;
    WCHAR wsz[128];
    HRESULT hr;

    if (!pUnkGraph || !pdwRegister)
        return E_POINTER;

    if (FAILED(GetRunningObjectTable(0, &pROT)))
        return E_FAIL;

    hr = StringCchPrintfW(wsz, NUMELMS(wsz), L"FilterGraph %08x pid %08x\0", (DWORD_PTR)pUnkGraph,
        GetCurrentProcessId());

    hr = CreateItemMoniker(L"!", wsz, &pMoniker);
    if (SUCCEEDED(hr))
    {
        // Use the ROTFLAGS_REGISTRATIONKEEPSALIVE to ensure a strong reference
        // to the object.  Using this flag will cause the object to remain
        // registered until it is explicitly revoked with the Revoke() method.
        //
        // Not using this flag means that if GraphEdit remotely connects
        // to this graph and then GraphEdit exits, this object registration 
        // will be deleted, causing future attempts by GraphEdit to fail until
        // this application is restarted or until the graph is registered again.
        hr = pROT->Register(ROTFLAGS_REGISTRATIONKEEPSALIVE, pUnkGraph,
            pMoniker, pdwRegister);
        pMoniker->Release();
    }

    pROT->Release();
    return hr;
}


// Removes a filter graph from the Running Object Table
void RemoveGraphFromRot(DWORD pdwRegister)
{
    IRunningObjectTable *pROT;

    if (SUCCEEDED(GetRunningObjectTable(0, &pROT)))
    {
        pROT->Revoke(pdwRegister);
        pROT->Release();
    }
}

#endif


HRESULT HandleGraphEvent(void)
{
    LONG evCode;
    LONG_PTR evParam1, evParam2;
    HRESULT hr = S_OK;

    if (!g_pME)
        return E_POINTER;

    while (SUCCEEDED(g_pME->GetEvent(&evCode, &evParam1, &evParam2, 0)))
    {
        //
        // Free event parameters to prevent memory leaks associated with
        // event parameter data.  While this application is not interested
        // in the received events, applications should always process them.
        //
        hr = g_pME->FreeEventParams(evCode, evParam1, evParam2);

        // Insert event processing code here, if desired
    }

    return hr;
}

