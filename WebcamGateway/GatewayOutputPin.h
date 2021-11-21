#pragma once
#include "CommonInterfaces.h"
#include <strsafe.h>
#include <streams.h>
#include <functional>

typedef std::function<HRESULT(const unsigned char* buffer, int size)> BufferProcessFunc;

class SampleGrabberCallback;

class GatewayOutputPin : public CSourceStream, public IKsPropertySet, public IAMStreamConfig
{
protected:
    ICaptureGraphBuilder2* _pCaptureGraph;    // Capture graph builder object
    IGraphBuilder* _pGraph;                    // Graph builder object
    IMediaEventEx * _pMediaEvent;
    IMediaControl* _pControl;                // Media control object
    IBaseFilter* _pGrabberF;
    IBaseFilter* _pDestFilter;
    IAMStreamConfig* _streamConf;
    ISampleGrabber* _pGrabber;                // Grabs frame
    SampleGrabberCallback* _sgCallback;
    GUID _captureMode;
    IBaseFilter* _pVideoInputFilter;          // Video Capture filter


	int m_FramesWritten;				// To track where we are in the file
	BOOL m_bZeroMemory;                 // Do we need to clear the buffer?
	CRefTime m_rtSampleTime;	        // The time stamp for each sample

	int m_iFrameNumber;
	const REFERENCE_TIME m_rtFrameLength;

	RECT m_rScreen;                     // Rect containing entire screen coordinates

	int m_iImageHeight;                 // The current image height
	int m_iImageWidth;                  // And current image width
	int m_iRepeatTime;                  // Time in msec between frames
	int m_nCurrentBitDepth;             // Screen bit depth

	CCritSec m_cSharedState;            // Protects our internal state
    bool _formatChanged;
    unsigned char* _pixels;
    LONG _bufferSize;
    CMediaType _inputMediaType;
    int _streamCaps;
    HRESULT _lastStartResult;

private:
    HRESULT SetupStream(IBaseFilter* videoInputDevice);
    HRESULT StartStream();
    void UpdateSize();
    void ConvertToSupportedFormat(AM_MEDIA_TYPE* dst, const AM_MEDIA_TYPE* src);
    HRESULT CheckFormat(const AM_MEDIA_TYPE *pmt);
public:

	GatewayOutputPin(HRESULT* phr, CSource* pFilter, IBaseFilter* videoInputDevice);
	~GatewayOutputPin();

	// Override the version that offers exactly one media type
	HRESULT DecideBufferSize(IMemAllocator* pAlloc, ALLOCATOR_PROPERTIES* pRequest);
	HRESULT FillBuffer(IMediaSample* pSample);

	// Set the agreed media type and set up the necessary parameters
	HRESULT SetMediaType(const CMediaType* pMediaType);

	// Support multiple display formats
	HRESULT CheckMediaType(const CMediaType* pMediaType);
	HRESULT GetMediaType(int iPosition, CMediaType* pmt);

    HRESULT OnThreadCreate(void) override;
    HRESULT OnThreadDestroy(void) override;
    HRESULT OnThreadStartPlay(void) override;

	// Quality control
	// Not implemented because we aren't going in real time.
	// If the file-writing filter slows the graph down, we just do nothing, which means
	// wait until we're unblocked. No frames are ever dropped.
	STDMETHODIMP Notify(IBaseFilter* pSelf, Quality q)
	{
		return E_FAIL;
	}

	DECLARE_IUNKNOWN;
	// override this to say what interfaces we support where
	STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, __deref_out void** ppv);


	//////////////////////////////////////////////////////////////////////////
	//  IKsPropertySet
	//////////////////////////////////////////////////////////////////////////
	HRESULT STDMETHODCALLTYPE Set(REFGUID guidPropSet, DWORD dwID, void* pInstanceData, DWORD cbInstanceData, void* pPropData, DWORD cbPropData);
	HRESULT STDMETHODCALLTYPE Get(REFGUID guidPropSet, DWORD dwPropID, void* pInstanceData, DWORD cbInstanceData, void* pPropData, DWORD cbPropData, DWORD* pcbReturned);
	HRESULT STDMETHODCALLTYPE QuerySupported(REFGUID guidPropSet, DWORD dwPropID, DWORD* pTypeSupport);

    //////////////////////////////////////////////////////////////////////////
    //  IAMStreamConfig
    //////////////////////////////////////////////////////////////////////////
    HRESULT STDMETHODCALLTYPE SetFormat(
        /* [in] */ AM_MEDIA_TYPE *pmt);

    HRESULT STDMETHODCALLTYPE GetFormat(
        /* [annotation][out] */
        _Out_  AM_MEDIA_TYPE **ppmt);

    HRESULT STDMETHODCALLTYPE GetNumberOfCapabilities(
        /* [annotation][out] */
        _Out_  int *piCount,
        /* [annotation][out] */
        _Out_  int *piSize);

    HRESULT STDMETHODCALLTYPE GetStreamCaps(
        /* [in] */ int iIndex,
        /* [annotation][out] */
        _Out_  AM_MEDIA_TYPE **ppmt,
        /* [annotation][out] */
        _Out_  BYTE *pSCC);
};