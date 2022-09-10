#include "pch.h"
#include "VirtualVideoCamera.h"
#include "DesktopCapturePin.h"
#include "RandomCapturePin.h"

#if defined(_WIN64)
// {C07EE3A7-4EA4-4A25-BB8E-0FFEE7AEB140}
const GUID CLSID_VIRTUALCAMERAFILTER =
{ 0x9b140680, 0x43e5, 0x45e8,{ 0xb5, 0x42, 0x49, 0x17, 0xc0, 0x8f, 0xae, 0x69 } };

#else
// {7DEDD193-2410-48DB-BBC5-A6B817FC796F}
const GUID CLSID_VIRTUALCAMERAFILTER =
{ 0x7dedd193, 0x2410, 0x48db,{ 0xbb, 0xc5, 0xa6, 0xb8, 0x17, 0xfc, 0x79, 0x6f } };
#endif

CVirtualVideoCamera::CVirtualVideoCamera(LPUNKNOWN punk, HRESULT *phr)
    : CSource(g_wszVirtualCamera, punk, CLSID_VIRTUALCAMERAFILTER)
{
	// The pin magically adds itself to our pin array.
	auto previewPin = new RandomCapturePin(phr, this);

	if (phr)
	{
		if (previewPin == NULL)
			* phr = E_OUTOFMEMORY;
		else
			*phr = S_OK;
	}
}


CVirtualVideoCamera::~CVirtualVideoCamera()
{
}


STDMETHODIMP CVirtualVideoCamera::NonDelegatingQueryInterface(REFIID riid,
	__deref_out void** ppv)
{
	if (riid == CLSID_VIRTUALCAMERAFILTER) {
		return GetInterface((IBaseFilter*)this, ppv);
	}

	if (riid == IID_IAMVideoProcAmp) {
		return CSource::NonDelegatingQueryInterface(riid, ppv);
	}
	
	return CSource::NonDelegatingQueryInterface(riid, ppv);
}

//////////////////////////////////////////

// {D2DAB11D-E3AE-475E-81AC-7B48F75F1CFD}
static const GUID CLSID_MYTRANFORMFILTER =
{ 0xd2dab11d, 0xe3ae, 0x475e,{ 0x81, 0xac, 0x7b, 0x48, 0xf7, 0x5f, 0x1c, 0xfd } };

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
        return E_NOTIMPL;
    }

    // override to suggest OUTPUT pin media types
    HRESULT GetMediaType(int iPosition, __inout CMediaType *pMediaType) {
        return E_NOTIMPL;
    }

    STDMETHODIMP JoinFilterGraph(__inout_opt IFilterGraph * pGraph,
        __in_opt LPCWSTR pName)
    {
        auto res1 = GetPin(0)->IsConnected();
        auto res2 = GetPin(1)->IsConnected();
        HRESULT hr = CTransformFilter::JoinFilterGraph(pGraph, pName);
        return hr;
    }
};
/////////////////////////////////////////

STDMETHODIMP CVirtualVideoCamera::JoinFilterGraph(
    __inout_opt IFilterGraph * pGraph,
    __in_opt LPCWSTR pName) {

    DbgLog((LOG_TRACE, 3, TEXT(__FUNCTION__)));
    HRESULT hr = CBaseFilter::JoinFilterGraph(pGraph, pName);
    if (pGraph) {
        CMyTranformFilter* myFilter = new CMyTranformFilter(NULL, &hr);
        hr = pGraph->AddFilter(myFilter, L"My Transform filter");
    }

    return hr;
}

CUnknown * WINAPI CVirtualVideoCamera::CreateInstance(IUnknown *pUnk, HRESULT *phr)
{
    DbgSetModuleLevel(LOG_TRACE, 5);
    CVirtualVideoCamera *pNewFilter = new CVirtualVideoCamera(pUnk, phr);

    if (phr)
    {
        if (pNewFilter == NULL)
            *phr = E_OUTOFMEMORY;
        else
            *phr = S_OK;
    }
    return pNewFilter;

}