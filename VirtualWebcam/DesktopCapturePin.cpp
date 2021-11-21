#include "pch.h"
#include "DesktopCapturePin.h"

#include "DibHelper.h"


// UNITS = 10 ^ 7  
// UNITS / 30 = 30 fps;
// UNITS / 20 = 20 fps, etc
const REFERENCE_TIME FPS_30 = UNITS / 30;
const REFERENCE_TIME FPS_20 = UNITS / 20;
const REFERENCE_TIME FPS_10 = UNITS / 10;
const REFERENCE_TIME FPS_5 = UNITS / 5;
const REFERENCE_TIME FPS_4 = UNITS / 4;
const REFERENCE_TIME FPS_3 = UNITS / 3;
const REFERENCE_TIME FPS_2 = UNITS / 2;
const REFERENCE_TIME FPS_1 = UNITS / 1;

const REFERENCE_TIME rtDefaultFrameLength = FPS_10;

CPushPinDesktop::CPushPinDesktop(HRESULT* phr, CSource* pFilter)
	: CSourceStream(NAME("Push Source Desktop"), phr, pFilter, L"Preview"),
	m_FramesWritten(0),
	m_bZeroMemory(0),
	m_iFrameNumber(0),
	m_rtFrameLength(FPS_5), // Capture and display desktop 5 times per second
	m_nCurrentBitDepth(32)
{
	// The main point of this sample is to demonstrate how to take a DIB
	// in host memory and insert it into a video stream. 

	// To keep this sample as simple as possible, we just read the desktop image
	// from a file and copy it into every frame that we send downstream.
	//
	// In the filter graph, we connect this filter to the AVI Mux, which creates 
	// the AVI file with the video frames we pass to it. In this case, 
	// the end result is a screen capture video (GDI images only, with no
	// support for overlay surfaces).

	// Get the device context of the main display
	HDC hDC;
	hDC = CreateDC(TEXT("DISPLAY"), NULL, NULL, NULL);

	// Get the dimensions of the main desktop window
	m_rScreen.left = m_rScreen.top = 0;
	m_rScreen.right = GetDeviceCaps(hDC, HORZRES);
	m_rScreen.bottom = GetDeviceCaps(hDC, VERTRES);

	// Save dimensions for later use in FillBuffer()
	m_iImageWidth = m_rScreen.right - m_rScreen.left;
	m_iImageHeight = m_rScreen.bottom - m_rScreen.top;

	// Release the device context
	DeleteDC(hDC);
    // fill media format type with default format type
    GetMediaType(0, &m_mt);
}

CPushPinDesktop::~CPushPinDesktop()
{
	DbgLog((LOG_TRACE, 3, TEXT("Frames written %d"), m_iFrameNumber));
}


//
// GetMediaType
//
// Prefer 5 formats - 8, 16 (*2), 24 or 32 bits per pixel
//
// Prefered types should be ordered by quality, with zero as highest quality.
// Therefore, iPosition =
//      0    Return a 32bit mediatype
//      1    Return a 24bit mediatype
//      2    Return 16bit RGB565
//      3    Return a 16bit mediatype (rgb555)
//      4    Return 8 bit palettised format
//      >4   Invalid
//
HRESULT CPushPinDesktop::GetMediaType(int iPosition, CMediaType* pmt)
{
	CheckPointer(pmt, E_POINTER);
	CAutoLock cAutoLock(m_pFilter->pStateLock());

	if (iPosition < 0)
		return E_INVALIDARG;

	// Have we run off the end of types?
	if (iPosition > 4)
		return VFW_S_NO_MORE_ITEMS;

	VIDEOINFO* pvi = (VIDEOINFO*)pmt->AllocFormatBuffer(sizeof(VIDEOINFO));
	if (NULL == pvi)
		return(E_OUTOFMEMORY);

	// Initialize the VideoInfo structure before configuring its members
	ZeroMemory(pvi, sizeof(VIDEOINFO));

	switch (iPosition)
	{
	case 0:
	{
		// Return our highest quality 32bit format

		// Since we use RGB888 (the default for 32 bit), there is
		// no reason to use BI_BITFIELDS to specify the RGB
		// masks. Also, not everything supports BI_BITFIELDS
		pvi->bmiHeader.biCompression = BI_RGB;
		pvi->bmiHeader.biBitCount = 32;
		break;
	}

	case 1:
	{   // Return our 24bit format
		pvi->bmiHeader.biCompression = BI_RGB;
		pvi->bmiHeader.biBitCount = 24;
		break;
	}

	case 2:
	{
		// 16 bit per pixel RGB565

		// Place the RGB masks as the first 3 doublewords in the palette area
		for (int i = 0; i < 3; i++)
			pvi->TrueColorInfo.dwBitMasks[i] = bits565[i];

		pvi->bmiHeader.biCompression = BI_BITFIELDS;
		pvi->bmiHeader.biBitCount = 16;
		break;
	}

	case 3:
	{   // 16 bits per pixel RGB555

		// Place the RGB masks as the first 3 doublewords in the palette area
		for (int i = 0; i < 3; i++)
			pvi->TrueColorInfo.dwBitMasks[i] = bits555[i];

		pvi->bmiHeader.biCompression = BI_BITFIELDS;
		pvi->bmiHeader.biBitCount = 16;
		break;
	}

	case 4:
	{   // 8 bit palettised

		pvi->bmiHeader.biCompression = BI_RGB;
		pvi->bmiHeader.biBitCount = 8;
		pvi->bmiHeader.biClrUsed = iPALETTE_COLORS;
		break;
	}
	}

	// Adjust the parameters common to all formats
	pvi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	pvi->bmiHeader.biWidth = m_iImageWidth;
	pvi->bmiHeader.biHeight = m_iImageHeight;
	pvi->bmiHeader.biPlanes = 1;
	pvi->bmiHeader.biSizeImage = GetBitmapSize(&pvi->bmiHeader);
	pvi->bmiHeader.biClrImportant = 0;

	SetRectEmpty(&(pvi->rcSource)); // we want the whole image area rendered.
	SetRectEmpty(&(pvi->rcTarget)); // no particular destination rectangle

	pmt->SetType(&MEDIATYPE_Video);
	pmt->SetFormatType(&FORMAT_VideoInfo);
	pmt->SetTemporalCompression(FALSE);

	// Work out the GUID for the subtype from the header info.
	const GUID SubTypeGUID = GetBitmapSubtype(&pvi->bmiHeader);
	pmt->SetSubtype(&SubTypeGUID);
	pmt->SetSampleSize(pvi->bmiHeader.biSizeImage);

	return NOERROR;

} // GetMediaType


//
// CheckMediaType
//
// We will accept 8, 16, 24 or 32 bit video formats, in any
// image size that gives room to bounce.
// Returns E_INVALIDARG if the mediatype is not acceptable
//
HRESULT CPushPinDesktop::CheckMediaType(const CMediaType * pMediaType)
{
	CheckPointer(pMediaType, E_POINTER);

	if ((*(pMediaType->Type()) != MEDIATYPE_Video) ||   // we only output video
		!(pMediaType->IsFixedSize()))                  // in fixed size samples
	{
		return E_INVALIDARG;
	}

	// Check for the subtypes we support
	const GUID* SubType = pMediaType->Subtype();
	if (SubType == NULL)
		return E_INVALIDARG;

	if ((*SubType != MEDIASUBTYPE_RGB8)
		&& (*SubType != MEDIASUBTYPE_RGB565)
		&& (*SubType != MEDIASUBTYPE_RGB555)
		&& (*SubType != MEDIASUBTYPE_RGB24)
		&& (*SubType != MEDIASUBTYPE_RGB32))
	{
		return E_INVALIDARG;
	}

	// Get the format area of the media type
	VIDEOINFO* pvi = (VIDEOINFO*)pMediaType->Format();

	if (pvi == NULL)
		return E_INVALIDARG;

	// Check if the image width & height have changed
	if (pvi->bmiHeader.biWidth != m_iImageWidth ||
		abs(pvi->bmiHeader.biHeight) != m_iImageHeight)
	{
		// If the image width/height is changed, fail CheckMediaType() to force
		// the renderer to resize the image.
		return E_INVALIDARG;
	}

	// Don't accept formats with negative height, which would cause the desktop
	// image to be displayed upside down.
	if (pvi->bmiHeader.biHeight < 0)
		return E_INVALIDARG;

	return S_OK;  // This format is acceptable.

} // CheckMediaType


//
// DecideBufferSize
//
// This will always be called after the format has been sucessfully
// negotiated. So we have a look at m_mt to see what size image we agreed.
// Then we can ask for buffers of the correct size to contain them.
//
HRESULT CPushPinDesktop::DecideBufferSize(IMemAllocator * pAlloc,
	ALLOCATOR_PROPERTIES * pProperties)
{
	CheckPointer(pAlloc, E_POINTER);
	CheckPointer(pProperties, E_POINTER);

	CAutoLock cAutoLock(m_pFilter->pStateLock());
	HRESULT hr = NOERROR;

	VIDEOINFO* pvi = (VIDEOINFO*)m_mt.Format();
	pProperties->cBuffers = 1;
	pProperties->cbBuffer = pvi->bmiHeader.biSizeImage;

	ASSERT(pProperties->cbBuffer);

	// Ask the allocator to reserve us some sample memory. NOTE: the function
	// can succeed (return NOERROR) but still not have allocated the
	// memory that we requested, so we must check we got whatever we wanted.
	ALLOCATOR_PROPERTIES Actual;
	hr = pAlloc->SetProperties(pProperties, &Actual);
	if (FAILED(hr))
	{
		return hr;
	}

	// Is this allocator unsuitable?
	if (Actual.cbBuffer < pProperties->cbBuffer)
	{
		return E_FAIL;
	}

	// Make sure that we have only 1 buffer (we erase the ball in the
	// old buffer to save having to zero a 200k+ buffer every time
	// we draw a frame)
	ASSERT(Actual.cBuffers == 1);
	return NOERROR;

} // DecideBufferSize


//
// SetMediaType
//
// Called when a media type is agreed between filters
//
HRESULT CPushPinDesktop::SetMediaType(const CMediaType * pMediaType)
{
	CAutoLock cAutoLock(m_pFilter->pStateLock());

	// Pass the call up to my base class
	HRESULT hr = CSourceStream::SetMediaType(pMediaType);

	if (SUCCEEDED(hr))
	{
		VIDEOINFO* pvi = (VIDEOINFO*)m_mt.Format();
		if (pvi == NULL)
			return E_UNEXPECTED;

		switch (pvi->bmiHeader.biBitCount)
		{
		case 8:     // 8-bit palettized
		case 16:    // RGB565, RGB555
		case 24:    // RGB24
		case 32:    // RGB32
			// Save the current media type and bit depth
			m_MediaType = *pMediaType;
			m_nCurrentBitDepth = pvi->bmiHeader.biBitCount;
			hr = S_OK;
			break;

		default:
			// We should never agree any other media types
			ASSERT(FALSE);
			hr = E_INVALIDARG;
			break;
		}
	}

	return hr;

} // SetMediaType


// This is where we insert the DIB bits into the video stream.
// FillBuffer is called once for every sample in the stream.
HRESULT CPushPinDesktop::FillBuffer(IMediaSample * pSample)
{
	BYTE* pData;
	long cbData;

	CheckPointer(pSample, E_POINTER);

	CAutoLock cAutoLockShared(&m_cSharedState);

	// Access the sample's data buffer
	pSample->GetPointer(&pData);
	cbData = pSample->GetSize();

	// Check that we're still using video
	ASSERT(m_mt.formattype == FORMAT_VideoInfo);

	VIDEOINFOHEADER * pVih = (VIDEOINFOHEADER*)m_mt.pbFormat;

	// Copy the DIB bits over into our filter's output buffer.
	// Since sample size may be larger than the image size, bound the copy size.
	int nSize = min(pVih->bmiHeader.biSizeImage, (DWORD)cbData);
	HDIB hDib = CopyScreenToBitmap(&m_rScreen, pData, (BITMAPINFO*) & (pVih->bmiHeader));

	if (hDib)
		DeleteObject(hDib);

	// Set the timestamps that will govern playback frame rate.
	// If this file is getting written out as an AVI,
	// then you'll also need to configure the AVI Mux filter to 
	// set the Average Time Per Frame for the AVI Header.
	// The current time is the sample's start.
	REFERENCE_TIME rtStart = m_iFrameNumber * m_rtFrameLength;
	REFERENCE_TIME rtStop = rtStart + m_rtFrameLength;

	pSample->SetTime(&rtStart, &rtStop);
	m_iFrameNumber++;

	// Set TRUE on every sample for uncompressed frames
	pSample->SetSyncPoint(TRUE);

	return S_OK;
}

STDMETHODIMP CPushPinDesktop::NonDelegatingQueryInterface(REFIID riid,
	__deref_out void** ppv)
{
	if (riid == IID_IKsPropertySet) {
		return GetInterface((IKsPropertySet*)this, ppv);
	}
    if (riid == IID_IAMStreamConfig) {
        return GetInterface((IAMStreamConfig*)this, ppv);
    }

	return CSourceStream::NonDelegatingQueryInterface(riid, ppv);
}

/************************************************************************************************************
*
* IKsPropertySet interface implementation
*
*
*************************************************************************************************************/

// Set: Cannot set any properties.
HRESULT CPushPinDesktop::Set(REFGUID guidPropSet, DWORD dwID,
	void* pInstanceData, DWORD cbInstanceData, void* pPropData,
	DWORD cbPropData)
{
	return E_NOTIMPL;
}

// Get: Return the pin category (our only property). 
HRESULT CPushPinDesktop::Get(
	REFGUID guidPropSet,   // Which property set.
	DWORD dwPropID,        // Which property in that set.
	void* pInstanceData,   // Instance data (ignore).
	DWORD cbInstanceData,  // Size of the instance data (ignore).
	void* pPropData,       // Buffer to receive the property data.
	DWORD cbPropData,      // Size of the buffer.
	DWORD* pcbReturned     // Return the size of the property.
)
{
	if (guidPropSet != AMPROPSETID_Pin)
		return E_PROP_SET_UNSUPPORTED;
	if (dwPropID != AMPROPERTY_PIN_CATEGORY)
		return E_PROP_ID_UNSUPPORTED;
	if (pPropData == NULL && pcbReturned == NULL)
		return E_POINTER;
	if (pcbReturned)
		* pcbReturned = sizeof(GUID);
	if (pPropData == NULL)  // Caller just wants to know the size.
		return S_OK;
	if (cbPropData < sizeof(GUID)) // The buffer is too small.
		return E_UNEXPECTED;
	*(GUID*)pPropData = PIN_CATEGORY_CAPTURE;
	return S_OK;
}

// QuerySupported: Query whether the pin supports the specified property.
HRESULT CPushPinDesktop::QuerySupported(REFGUID guidPropSet, DWORD dwPropID,
	DWORD * pTypeSupport)
{
	if (guidPropSet != AMPROPSETID_Pin)
		return E_PROP_SET_UNSUPPORTED;
	if (dwPropID != AMPROPERTY_PIN_CATEGORY)
		return E_PROP_ID_UNSUPPORTED;
	if (pTypeSupport)
		// We support getting this property, but not setting it.
		* pTypeSupport = KSPROPERTY_SUPPORT_GET;
	return S_OK;
}

//////////////////////////////////////////////////////////////////////////
//  IAMStreamConfig
//////////////////////////////////////////////////////////////////////////
HRESULT STDMETHODCALLTYPE CPushPinDesktop::SetFormat(
    /* [in] */ AM_MEDIA_TYPE *pmt) {
    if (!pmt) {
        return S_OK;
    }

    //if (CheckMediaType((CMediaType *)pmt) != S_OK) {
    //    return E_FAIL; // just in case :P [FME...]
    //}

    m_mt = *pmt;

    IPin* pin;
    ConnectedTo(&pin);
    if (pin)
    {
        IFilterGraph *pGraph = ((CSource*)m_pFilter)->GetFilterGraph();
        pGraph->Reconnect(this);
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE CPushPinDesktop::GetFormat(
    /* [annotation][out] */
    _Out_  AM_MEDIA_TYPE **ppmt) {
    *ppmt = CreateMediaType(&m_mt);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CPushPinDesktop::GetNumberOfCapabilities(
    /* [annotation][out] */
    _Out_  int *piCount,
    /* [annotation][out] */
    _Out_  int *piSize) {
    *piCount = 1; // only allow one type currently...
    *piSize = sizeof(VIDEO_STREAM_CONFIG_CAPS);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CPushPinDesktop::GetStreamCaps(
    /* [in] */ int iIndex,
    /* [annotation][out] */
    _Out_  AM_MEDIA_TYPE **ppMediaType,
    /* [annotation][out] */
    _Out_  BYTE *pSCC) {
    if (iIndex < 0)
        return E_INVALIDARG;
    if (iIndex > 0)
        return S_FALSE;
    if (pSCC == NULL)
        return E_POINTER;

    *ppMediaType = CreateMediaType(&m_mt);
    if (*ppMediaType == NULL) return E_OUTOFMEMORY;
    VIDEOINFOHEADER* pvi = (VIDEOINFOHEADER*)(*ppMediaType)->pbFormat;
    //
    AM_MEDIA_TYPE * pm = *ppMediaType;
    //
    //
    VIDEO_STREAM_CONFIG_CAPS* pASCC = (VIDEO_STREAM_CONFIG_CAPS*)pSCC;
    ZeroMemory(pSCC, sizeof(VIDEO_STREAM_CONFIG_CAPS));
    ///////////////////////////////////////////////////
    pvi->bmiHeader.biCompression = BI_RGB;
    pvi->bmiHeader.biBitCount = m_nCurrentBitDepth;
    pvi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    pvi->bmiHeader.biWidth = m_iImageWidth;
    pvi->bmiHeader.biHeight = m_iImageHeight;
    pvi->bmiHeader.biPlanes = 1;
    pvi->bmiHeader.biSizeImage = GetBitmapSize(&pvi->bmiHeader);
    pvi->bmiHeader.biClrImportant = 0;

    (*ppMediaType)->majortype = MEDIATYPE_Video;
    (*ppMediaType)->subtype = MEDIASUBTYPE_RGB32;
    (*ppMediaType)->formattype = FORMAT_VideoInfo;
    (*ppMediaType)->bTemporalCompression = FALSE;
    (*ppMediaType)->bFixedSizeSamples = FALSE;
    (*ppMediaType)->lSampleSize = pvi->bmiHeader.biSizeImage;
    (*ppMediaType)->cbFormat = sizeof(VIDEOINFOHEADER);
    //
    pASCC->guid = FORMAT_VideoInfo;
    pASCC->VideoStandard = AnalogVideo_None;

    pASCC->InputSize.cx = m_iImageWidth;
    pASCC->InputSize.cy = m_iImageHeight;
    pASCC->MinCroppingSize.cx = 80;
    pASCC->MinCroppingSize.cy = 60;
    pASCC->MaxCroppingSize.cx = m_iImageWidth;
    pASCC->MaxCroppingSize.cy = m_iImageHeight;
    pASCC->CropGranularityX = 80;
    pASCC->CropGranularityY = 60;

    pASCC->CropAlignX = 0;
    pASCC->CropAlignY = 0;

    pASCC->MinOutputSize.cx = pASCC->MinCroppingSize.cx;
    pASCC->MinOutputSize.cy = pASCC->MinCroppingSize.cy;
    pASCC->MaxOutputSize.cx = pASCC->MaxCroppingSize.cx;
    pASCC->MaxOutputSize.cy = pASCC->MaxCroppingSize.cy;
    pASCC->OutputGranularityX = 0;
    pASCC->OutputGranularityY = 0;
    pASCC->StretchTapsX = 0;
    pASCC->StretchTapsY = 0;
    pASCC->ShrinkTapsX = 0;
    pASCC->ShrinkTapsY = 0;

    pASCC->MinFrameInterval = 200000;
    pASCC->MaxFrameInterval = UNITS / 1;
    pASCC->MinBitsPerSecond = (pASCC->MinOutputSize.cx * pASCC->MinOutputSize.cy * m_nCurrentBitDepth) / 5;// * 1;
    pASCC->MaxBitsPerSecond = (pASCC->MaxOutputSize.cx * pASCC->MaxOutputSize.cy * m_nCurrentBitDepth) * 50;//* (UNITS / m_out_framerate);

    return S_OK;
}