#include "pch.h"
#include "GatewayOutputPin.h"


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

#define DECLARE_PTR(type, ptr, expr) type* ptr = (type*)(expr);



/************************************************************************************************************
*
* Sample callback class
*
************************************************************************************************************/
class SampleGrabberCallback : public ISampleGrabberCB {
public:

    //------------------------------------------------
    SampleGrabberCallback() {
        InitializeCriticalSection(&critSection);
        latestBufferLength = 0;

        hEvent = CreateEvent(NULL, true, false, NULL);
        pixels = NULL;
        numBytes = 0;        
    }


    //------------------------------------------------
    virtual ~SampleGrabberCallback() {
        CloseHandle(hEvent);
        if (pixels) {
            delete[] pixels;
        }

        DeleteCriticalSection(&critSection);
    }


    //------------------------------------------------
    bool setupBuffer(int numBytesIn) {
        if (pixels) {
            delete[] pixels;
        }
        numBytes = numBytesIn;
        pixels = new unsigned char[numBytes];
        latestBufferLength = 0;
        return true;
    }


    //------------------------------------------------
    STDMETHODIMP_(ULONG) AddRef() { return 1; }
    STDMETHODIMP_(ULONG) Release() { return 2; }


    //------------------------------------------------
    STDMETHODIMP QueryInterface(REFIID, void **ppvObject) {
        *ppvObject = static_cast<ISampleGrabberCB*>(this);
        return S_OK;
    }


    //This method is meant to have less overhead
    //------------------------------------------------
    STDMETHODIMP SampleCB(double, IMediaSample *pSample) {
        //if (WaitForSingleObject(hEvent, 0) == WAIT_OBJECT_0) return S_OK;

        BYTE* ptrBuffer;
        HRESULT hr = pSample->GetPointer(&ptrBuffer);

        if (hr == S_OK) {
            latestBufferLength = pSample->GetActualDataLength();
            if (latestBufferLength == numBytes) {
                EnterCriticalSection(&critSection);
                memcpy(pixels, ptrBuffer, latestBufferLength);
                LeaveCriticalSection(&critSection);
                SetEvent(hEvent);
            }
            else {
                DbgLog((LOG_TRACE, 5, TEXT("ERROR: SampleCB() - buffer sizes do not match")));
            }
        }

        return S_OK;
    }


    //This method is meant to have more overhead
    STDMETHODIMP BufferCB(double, BYTE *, long) {
        return E_NOTIMPL;
    }

    STDMETHODIMP requestBuffer(BufferProcessFunc& bufferProcessing) {
        if (WaitForSingleObject(hEvent, 1000) == WAIT_OBJECT_0) {
            EnterCriticalSection(&critSection);
            auto hr = bufferProcessing(pixels, numBytes);
            LeaveCriticalSection(&critSection);
            ResetEvent(hEvent);

            return hr;
        }

        return E_FAIL;
    }

private:
    int latestBufferLength;
    int numBytes;
    unsigned char * pixels;
    CRITICAL_SECTION critSection;
    HANDLE hEvent;
};

/************************************************************************************************************
*
* GatewayOutputPin implementation
*
************************************************************************************************************/
GatewayOutputPin::GatewayOutputPin(HRESULT* phr, CSource* pFilter, IBaseFilter* videoInputDevice)
	: CSourceStream(NAME("Webcam gateway stream"), phr, pFilter, L"~Capture"),
	m_FramesWritten(0),
	m_bZeroMemory(0),
	m_iFrameNumber(0),
	m_rtFrameLength(FPS_5), // Capture and display desktop 5 times per second
	m_nCurrentBitDepth(32),
    _pixels(NULL), _bufferSize(0),
    _streamCaps(0), _formatChanged(false), _lastStartResult(E_FAIL)
{
    _sgCallback = new SampleGrabberCallback();
    *phr = SetupStream(videoInputDevice);

    if (SUCCEEDED(*phr)) {
        // fill media format type with default format type
        *phr = GetMediaType(0, &m_mt);
    }
}

GatewayOutputPin::~GatewayOutputPin()
{
	DbgLog((LOG_TRACE, 3, TEXT("Frames written %d"), m_iFrameNumber));

    if (_sgCallback && (_pGrabber))
    {
        _pGrabber->SetCallback(NULL, 0);
        delete _sgCallback;
    }

    SAFE_RELEASE(_pCaptureGraph);    // Capture graph builder object
    SAFE_RELEASE(_pGraph);                    // Graph builder object
    SAFE_RELEASE(_pMediaEvent);
    SAFE_RELEASE(_pControl);                // Media control object
    SAFE_RELEASE(_pGrabberF);
    SAFE_RELEASE(_pDestFilter);
    SAFE_RELEASE(_streamConf);
    SAFE_RELEASE(_pVideoInputFilter);          // Video Capture filter

    SAFE_RELEASE(_pGrabber);                // Grabs frame
    if (_pixels) {
        delete[] _pixels;
    }
}

//
// GetMediaType
//
HRESULT GatewayOutputPin::GetMediaType(int iPosition, CMediaType* pmt)
{
    DbgLog((LOG_TRACE, 3, TEXT(__FUNCTION__)));
	CheckPointer(pmt, E_POINTER);
	CAutoLock cAutoLock(m_pFilter->pStateLock());

    if (iPosition < 0) return E_INVALIDARG;
    if (iPosition >= _streamCaps) return VFW_S_NO_MORE_ITEMS;
   
    AM_MEDIA_TYPE* pMediaType = NULL;
    VIDEO_STREAM_CONFIG_CAPS scc;
    HRESULT hr = _streamConf->GetStreamCaps(iPosition, &pMediaType, (BYTE*)&scc);
    if (FAILED(hr)) {
        DbgLog((LOG_TRACE, 5, TEXT("ERROR: GetStreamCaps return hr=0x%x"), hr));
        return VFW_S_NO_MORE_ITEMS;
    }

    *pmt = *pMediaType;
    ConvertToSupportedFormat(pmt, pMediaType);
    DECLARE_PTR(VIDEOINFOHEADER, pvio, pmt->pbFormat);

    if (checkSingleByteFormat(pmt->subtype))
    {
        pvio->bmiHeader.biBitCount = 8;
    }
    else if (pmt->subtype == MEDIASUBTYPE_Y16)
    {
        pvio->bmiHeader.biBitCount = 16;
    }
    else
    {
        pvio->bmiHeader.biBitCount = 24;
    }

    pvio->bmiHeader.biCompression = BI_RGB;
    pvio->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    pvio->bmiHeader.biPlanes = 1;
    pvio->bmiHeader.biSizeImage = GetBitmapSize(&pvio->bmiHeader);
    pvio->bmiHeader.biClrImportant = 0;

    CoTaskMemFree(pMediaType);
    return NOERROR;

} // GetMediaType


//
// CheckMediaType
//
// We will accept 8, 16, 24 or 32 bit video formats, in any
// image size that gives room to bounce.
// Returns E_INVALIDARG if the mediatype is not acceptable
//
HRESULT GatewayOutputPin::CheckMediaType(const CMediaType * pmt)
{
    DbgLog((LOG_TRACE, 3, TEXT(__FUNCTION__)));
	CheckPointer(pmt, E_POINTER);

    HRESULT hr = CheckFormat(pmt);

    DECLARE_PTR(VIDEOINFOHEADER, pvio, pmt->pbFormat);
    DbgLog((LOG_TRACE, 3, TEXT("size = %dx%d, bit count = %d"), HEADER(pvio)->biWidth, HEADER(pvio)->biHeight, HEADER(pvio)->biBitCount));
    return hr;
} // CheckMediaType


//
// DecideBufferSize
//
// This will always be called after the format has been sucessfully
// negotiated. So we have a look at m_mt to see what size image we agreed.
// Then we can ask for buffers of the correct size to contain them.
//
HRESULT GatewayOutputPin::DecideBufferSize(IMemAllocator * pAlloc,
	ALLOCATOR_PROPERTIES * pProperties)
{
    DbgLog((LOG_TRACE, 3, TEXT(__FUNCTION__)));
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
HRESULT GatewayOutputPin::SetMediaType(const CMediaType * pMediaType)
{
    DbgLog((LOG_TRACE, 3, TEXT(__FUNCTION__)));
	CAutoLock cAutoLock(m_pFilter->pStateLock());

	// Pass the call up to my base class
	HRESULT hr = CSourceStream::SetMediaType(pMediaType);
    if (SUCCEEDED(hr)) {
        _formatChanged = true;
    }
	return hr;

} // SetMediaType

// FillBuffer is called once for every sample in the stream.
HRESULT GatewayOutputPin::FillBuffer(IMediaSample * pSample)
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

    BufferProcessFunc bufferProcessFunc = [=](const unsigned char* inputBuffer, int size) {
        HRESULT hr = S_OK;

        if (size != cbData) {
            DbgLog((LOG_TRACE, 3, TEXT("Not support other kind of frame format")));
            return E_FAIL;
        }

        memcpy(pData, inputBuffer, size);
        return hr;
    };



    ASSERT(_sgCallback);
    HRESULT hr = _sgCallback->requestBuffer(bufferProcessFunc);
    if (FAILED(hr)) {
        memset(pData, 0x128, cbData);
    }

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

STDMETHODIMP GatewayOutputPin::NonDelegatingQueryInterface(REFIID riid,
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
HRESULT GatewayOutputPin::Set(REFGUID guidPropSet, DWORD dwID,
	void* pInstanceData, DWORD cbInstanceData, void* pPropData,
	DWORD cbPropData)
{
	return E_NOTIMPL;
}

// Get: Return the pin category (our only property). 
HRESULT GatewayOutputPin::Get(
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
	*(GUID*)pPropData = _captureMode;
	return S_OK;
}

// QuerySupported: Query whether the pin supports the specified property.
HRESULT GatewayOutputPin::QuerySupported(REFGUID guidPropSet, DWORD dwPropID,
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

HRESULT GatewayOutputPin::CheckFormat(const AM_MEDIA_TYPE *pmt) {
    // the new format must be same the support format
    if (pmt->formattype != _inputMediaType.formattype) {
        return VFW_E_INVALIDMEDIATYPE;
    }
    if (pmt->majortype != _inputMediaType.majortype) {
        return VFW_E_INVALIDMEDIATYPE;
    }
    if (pmt->subtype != _inputMediaType.subtype) {
        return VFW_E_INVALIDMEDIATYPE;
    }

    // get current input stream format...
    AM_MEDIA_TYPE* pAmMediaType = NULL;
    HRESULT hr = _streamConf->GetFormat(&pAmMediaType);
    if (FAILED(hr)) {
        DbgLog((LOG_TRACE, 5, TEXT("ERROR: GetFormat return hr=0x%x"), hr));
        return hr;
    }
    DECLARE_PTR(VIDEOINFOHEADER, pvii, pAmMediaType->pbFormat);
    DECLARE_PTR(VIDEOINFOHEADER, pvio, pmt->pbFormat);

    pvii->AvgTimePerFrame = pvio->AvgTimePerFrame;
    HEADER(pvii)->biWidth = HEADER(pvio)->biWidth;
    HEADER(pvii)->biHeight = HEADER(pvio)->biHeight;
    pvii->rcSource.top = pvii->rcSource.left = pvii->rcTarget.top = pvii->rcTarget.left = 0;
    pvii->rcSource.right = pvii->rcTarget.right = HEADER(pvii)->biWidth;
    pvii->rcSource.bottom = pvii->rcTarget.bottom = HEADER(pvii)->biHeight;

    hr = _streamConf->SetFormat(pAmMediaType);
    return hr;
}

//////////////////////////////////////////////////////////////////////////
//  IAMStreamConfig
//////////////////////////////////////////////////////////////////////////
HRESULT STDMETHODCALLTYPE GatewayOutputPin::SetFormat(
    /* [in] */ AM_MEDIA_TYPE *pmt) {
    
    HRESULT hr = CheckFormat(pmt);

    if (SUCCEEDED(hr)) {
        m_mt = *pmt;
        IPin* pin;
        ConnectedTo(&pin);
        if (pin)
        {
            IFilterGraph *pGraph = ((CSource*)m_pFilter)->GetFilterGraph();
            pGraph->Reconnect(this);
        }
        _formatChanged = true;
    }
    return hr;
}

HRESULT STDMETHODCALLTYPE GatewayOutputPin::GetFormat(
    /* [annotation][out] */
    _Out_  AM_MEDIA_TYPE **ppmt) {
    *ppmt = CreateMediaType(&m_mt);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE GatewayOutputPin::GetNumberOfCapabilities(
    /* [annotation][out] */
    _Out_  int *piCount,
    /* [annotation][out] */
    _Out_  int *piSize) {
    *piCount = _streamCaps;
    *piSize = sizeof(VIDEO_STREAM_CONFIG_CAPS);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE GatewayOutputPin::GetStreamCaps(
    /* [in] */ int iIndex,
    /* [annotation][out] */
    _Out_  AM_MEDIA_TYPE **ppmt,
    /* [annotation][out] */
    _Out_  BYTE *pSCC) {
    if (iIndex < 0) return E_INVALIDARG;
    if (iIndex >= _streamCaps) VFW_S_NO_MORE_ITEMS;
    
    CheckPointer(ppmt, E_POINTER);

    // get origninal stream cap then modify it later
    HRESULT hr = _streamConf->GetStreamCaps(iIndex, ppmt, pSCC);
    if (FAILED(hr)) {
        DbgLog((LOG_TRACE, 5, TEXT("ERROR: GetStreamCaps return hr=0x%x"), hr));
        return hr;
    }

    VIDEO_STREAM_CONFIG_CAPS& scc = *(VIDEO_STREAM_CONFIG_CAPS*)pSCC;

    AM_MEDIA_TYPE *pmt = *ppmt;
    ConvertToSupportedFormat(pmt, pmt);
    DECLARE_PTR(VIDEOINFOHEADER, pvio, pmt->pbFormat);

    //SetRectEmpty(&(pvi->rcSource)); // we want the whole image area rendered.
    //SetRectEmpty(&(pvi->rcTarget)); // no particular destination rectangle
    
    if (checkSingleByteFormat(pmt->subtype))
    {
        pvio->bmiHeader.biBitCount = 8;
    }
    else if (pmt->subtype == MEDIASUBTYPE_Y16)
    {
        pvio->bmiHeader.biBitCount = 16;
    }
    else
    {
        pvio->bmiHeader.biBitCount = 24;
    }
    // try to suppiled more video information to the client
    if(scc.MaxFrameInterval)
        scc.MinBitsPerSecond = (LONG)((scc.MinOutputSize.cx * scc.MinOutputSize.cy * pvio->bmiHeader.biBitCount) * (10000000 / scc.MaxFrameInterval));
    if(scc.MinFrameInterval)
        scc.MaxBitsPerSecond = (LONG)((scc.MaxOutputSize.cx * scc.MaxOutputSize.cy * pvio->bmiHeader.biBitCount) * (10000000 / scc.MinFrameInterval));

    pvio->bmiHeader.biCompression = BI_RGB;
    pvio->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    pvio->bmiHeader.biPlanes = 1;
    pvio->bmiHeader.biSizeImage = GetBitmapSize(&pvio->bmiHeader);
    pvio->bmiHeader.biClrImportant = 0;

    return S_OK;
}


/************************************************************************************************************
*
* CWebcamGateWay implementation
*
************************************************************************************************************/
HRESULT GatewayOutputPin::SetupStream(IBaseFilter* videoInputDevice) {
    _captureMode = PIN_CATEGORY_CAPTURE;
    _pVideoInputFilter = videoInputDevice;
    _pVideoInputFilter->AddRef();

    // CREATE THE GRAPH BUILDER //
    // Create the filter graph manager and query for interfaces.
    HRESULT hr = CoCreateInstance(CLSID_CaptureGraphBuilder2, NULL, CLSCTX_INPROC_SERVER, IID_ICaptureGraphBuilder2, (void **)&_pCaptureGraph);
    if (FAILED(hr))    // FAILED is a macro that tests the return value
    {
        DbgLog((LOG_TRACE, 5, TEXT("ERROR - Could not create the Filter Graph Manager")));
        return hr;
    }

    //FILTER GRAPH MANAGER//
    // Create the Filter Graph Manager.
    hr = CoCreateInstance(CLSID_FilterGraph, 0, CLSCTX_INPROC_SERVER, IID_IGraphBuilder, (void**)&_pGraph);
    if (FAILED(hr))
    {
        DbgLog((LOG_TRACE, 5, TEXT("ERROR - Could not add the graph builder!")));
        return hr;
    }

    //MEDIA EVENT//
    //Used to obtain event when capture device is disconnected
    hr = _pGraph->QueryInterface(IID_IMediaEventEx, (void**)&_pMediaEvent);
    if (FAILED(hr))
    {
        DbgLog((LOG_TRACE, 5, TEXT("ERROR - Could not create media event object")));
        return hr;
    }

    //SET THE FILTERGRAPH//
    hr = _pCaptureGraph->SetFiltergraph(_pGraph);
    if (FAILED(hr))
    {
        DbgLog((LOG_TRACE, 5, TEXT("ERROR - Could not set filtergraph")));
        return hr;
    }

    //MEDIA CONTROL (START/STOPS STREAM)//
    // Using QueryInterface on the graph builder,
    // Get the Media Control object.
    hr = _pGraph->QueryInterface(IID_IMediaControl, (void **)&_pControl);
    if (FAILED(hr))
    {
        DbgLog((LOG_TRACE, 5, TEXT("ERROR - Could not create the Media Control object")));
        return hr;
    }

    hr = _pGraph->AddFilter(videoInputDevice, m_pName);
    if (FAILED(hr)) {
        DbgLog((LOG_TRACE, 5, TEXT("Cannot add video input filter to the graph")));
        return hr;
    }

    //LOOK FOR PREVIEW PIN IF THERE IS NONE THEN WE USE CAPTURE PIN AND THEN SMART TEE TO PREVIEW
    IAMStreamConfig *streamConfTest = NULL;
    hr = _pCaptureGraph->FindInterface(&PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video, videoInputDevice, IID_IAMStreamConfig, (void **)&streamConfTest);
    if (FAILED(hr)) {
        DbgLog((LOG_TRACE, 3, TEXT("SETUP: Couldn't find preview pin using SmartTee")));
    }
    else {
        _captureMode = PIN_CATEGORY_PREVIEW;
        streamConfTest->Release();
        streamConfTest = NULL;
    }

    // get stream again prefer to preview mode
    hr = _pCaptureGraph->FindInterface(&_captureMode, &MEDIATYPE_Video, videoInputDevice, IID_IAMStreamConfig, (void **)&_streamConf);
    if (FAILED(hr) || !_streamConf) {
        DbgLog((LOG_TRACE, 3, TEXT("ERROR: Couldn't config the stream! hr=0x%x"), hr));
        return hr;
    }

    int streamFormatSize = 0;
    hr = _streamConf->GetNumberOfCapabilities(&_streamCaps, &streamFormatSize);
    if (FAILED(hr)) {
        DbgLog((LOG_TRACE, 5, TEXT("ERROR: GetNumberOfCapabilities return hr=0x%x"), hr));
        return hr;
    }
    if (streamFormatSize != sizeof(VIDEO_STREAM_CONFIG_CAPS)) {
        DbgLog((LOG_TRACE, 5, TEXT("ERROR: GetNumberOfCapabilities return size = %d"), streamFormatSize));
        return E_FAIL;
    }

    //SAMPLE GRABBER (ALLOWS US TO GRAB THE BUFFER)//
    // Create the Sample Grabber.
    hr = CoCreateInstance(CLSID_SampleGrabber, NULL, CLSCTX_INPROC_SERVER,IID_IBaseFilter, (void**)&_pGrabberF);
    if (FAILED(hr)){
        DbgLog((LOG_TRACE, 5, TEXT("ERROR: Could not Create Sample Grabber - CoCreateInstance() hr=0x%x"), hr));
        return hr;
    }

    hr = _pGraph->AddFilter(_pGrabberF, L"Sample Grabber");
    if (FAILED(hr)){
        DbgLog((LOG_TRACE, 5, TEXT("ERROR: Could not add Sample Grabber - AddFilter() hr=0x%x"), hr));
        return hr;
    }

    hr = _pGrabberF->QueryInterface(IID_ISampleGrabber, (void**)&_pGrabber);
    if (FAILED(hr)){
        DbgLog((LOG_TRACE, 5, TEXT("ERROR: Could not query SampleGrabber hr=0x%x"), hr));
        return hr;
    }

    //Set Params - One Shot should be false unless you want to capture just one buffer
    hr = _pGrabber->SetOneShot(FALSE);
    if (FAILED(hr)) {
        DbgLog((LOG_TRACE, 5, TEXT("ERROR: ISampleGrabber::SetOneShot return hr=0x%x"), hr));
        return hr;
    }

    hr = _pGrabber->SetBufferSamples(FALSE);
    if (FAILED(hr)) {
        DbgLog((LOG_TRACE, 5, TEXT("ERROR: ISampleGrabber::SetBufferSamples return hr=0x%x"), hr));
        return hr;
    }
    
    hr = _pGrabber->SetCallback(_sgCallback, 0);
    if (FAILED(hr)) {
        DbgLog((LOG_TRACE, 5, TEXT("ERROR: problem setting callback hr=0x%x"), hr));
        return hr;
    }

    //NULL RENDERER//
    //used to give the video stream somewhere to go to.
    hr = CoCreateInstance(CLSID_NullRenderer, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**)(&_pDestFilter));
    if (FAILED(hr)){
        DbgLog((LOG_TRACE, 5, TEXT("ERROR: Could not create filter - NullRenderer hr=0x%x"), hr));
        return hr;
    }

    hr = _pGraph->AddFilter(_pDestFilter, L"NullRenderer");
    if (FAILED(hr)){
        DbgLog((LOG_TRACE, 5, TEXT("ERROR: Could not add filter - NullRenderer hr=0x%x"), hr));
        return hr;
    }

    AM_MEDIA_TYPE* pAmMediaType = NULL;
    //NOW LETS DEAL WITH GETTING THE RIGHT SIZE
    hr = _streamConf->GetFormat(&pAmMediaType);
    if (FAILED(hr)) {
        DbgLog((LOG_TRACE, 5, TEXT("ERROR: Couldn't getFormat for pAmMediaType! hr=0x%x"), hr));
        return hr;
    }

    //MEDIA CONVERSION
    //Get video properties from the stream's mediatype and apply to the grabber (otherwise we don't get an RGB image)
    //zero the media type - lets try this :) - maybe this works?
    ConvertToSupportedFormat(&_inputMediaType, pAmMediaType);

    FreeMediaType(*pAmMediaType);
    CoTaskMemFree(pAmMediaType);

    hr = _pGrabber->SetMediaType(&_inputMediaType);
    if (FAILED(hr)) {
        DbgLog((LOG_TRACE, 5, TEXT("ERROR: ISampleGrabber::SetMediaType return hr=0x%x"), hr));
        return hr;
    }
    // at setup time, format change flag should be true
    _formatChanged = true;
    return S_OK;
}

void GatewayOutputPin::ConvertToSupportedFormat(AM_MEDIA_TYPE* dst, const AM_MEDIA_TYPE* src) {
    dst->majortype = MEDIATYPE_Video;

    // Disable format conversion if using 8/16-bit data (e-Con systems)
    if (checkSingleByteFormat(src->subtype) || (src->subtype == MEDIASUBTYPE_Y16)) {
        DbgLog((LOG_TRACE, 1, TEXT("SETUP: Not converting frames to RGB")));
        dst->subtype = src->subtype;
    }
    else
    {
        DbgLog((LOG_TRACE, 1, TEXT("SETUP: Converting frames to RGB")));
        dst->subtype = MEDIASUBTYPE_RGB24; //Making it RGB24, does conversion from YUV to RGB
    }
    //_inputMediaType.SetTemporalCompression(FALSE);
    //_inputMediaType.SetSampleSize(pAmMediaType->lSampleSize);
    dst->formattype = FORMAT_VideoInfo;
}

HRESULT GatewayOutputPin::StartStream() {
    //RENDER STREAM//
    //This is where the stream gets put together.
    auto hr = _pCaptureGraph->RenderStream(&_captureMode, &MEDIATYPE_Video, _pVideoInputFilter, _pGrabberF, _pDestFilter);

    if (FAILED(hr)) {
        DbgLog((LOG_TRACE, 5, TEXT("ERROR: Could not connect pins - RenderStream() hr=0x%x"), hr));
        return hr;
    }


    //EXP - lets try setting the sync source to null - and make it run as fast as possible
    {
        IMediaFilter *pMediaFilter = 0;
        hr = _pGraph->QueryInterface(IID_IMediaFilter, (void**)&pMediaFilter);
        if (FAILED(hr)) {
            DbgLog((LOG_TRACE, 5, TEXT("ERROR: Could not get IID_IMediaFilter interface hr=0x%x"), hr));
        }
        else {
            pMediaFilter->SetSyncSource(NULL);
            pMediaFilter->Release();
        }
    }

    return S_OK;
}

void GatewayOutputPin::UpdateSize() {
    if (!_formatChanged) {
        DbgLog((LOG_TRACE, 3, TEXT("SETUP: Error device size should not be set more than once")));
    }
    else
    {
        // eventually, the frame format of grabber callback is same as output frame format
        DECLARE_PTR(VIDEOINFOHEADER, pvio, m_mt.pbFormat);
        int w = pvio->bmiHeader.biWidth;
        int h = pvio->bmiHeader.biHeight;

        m_iImageWidth = w;
        m_iImageHeight = h;
        int frameSize = 0;
        if (checkSingleByteFormat(_inputMediaType.subtype))
        {
            frameSize = w * h;
        }
        else if (_inputMediaType.subtype == MEDIASUBTYPE_Y16)
        {
            frameSize = w * h * 2;
        }
        else
        {
            frameSize = w * h * 3;
        }

        _bufferSize = frameSize;

        if (_pixels) delete[] _pixels;
        _pixels = new unsigned char[frameSize];
        //pBuffer = new char[frameSize];

        memset(_pixels, 0, frameSize);
        _sgCallback->setupBuffer(frameSize);
        _formatChanged = false;
    }
}

HRESULT GatewayOutputPin::OnThreadCreate(void) {
    UpdateSize();

    // start the stream only once
    // if the last result is failed, try to start again
    if (FAILED(_lastStartResult)) {
         _lastStartResult = StartStream();
    }

    return _lastStartResult;
}

HRESULT GatewayOutputPin::OnThreadDestroy(void) {
    ASSERT(_pControl);
    return _pControl->Stop();
    //return NOERROR;
}

HRESULT GatewayOutputPin::OnThreadStartPlay(void) {
    ASSERT(_pControl);
    return _pControl->Run();
};