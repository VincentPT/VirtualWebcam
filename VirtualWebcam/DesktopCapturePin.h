#pragma once
#include <strsafe.h>
#include <streams.h>

class CPushPinDesktop : public CSourceStream, public IKsPropertySet, public IAMStreamConfig
{
protected:

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

	CMediaType m_MediaType;
	CCritSec m_cSharedState;            // Protects our internal state
	CImageDisplay m_Display;            // Figures out our media type for us

public:

	CPushPinDesktop(HRESULT* phr, CSource* pFilter);
	~CPushPinDesktop();

	// Override the version that offers exactly one media type
	HRESULT DecideBufferSize(IMemAllocator* pAlloc, ALLOCATOR_PROPERTIES* pRequest);
	HRESULT FillBuffer(IMediaSample* pSample);

	// Set the agreed media type and set up the necessary parameters
	HRESULT SetMediaType(const CMediaType* pMediaType);

	// Support multiple display formats
	HRESULT CheckMediaType(const CMediaType* pMediaType);
	HRESULT GetMediaType(int iPosition, CMediaType* pmt);

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