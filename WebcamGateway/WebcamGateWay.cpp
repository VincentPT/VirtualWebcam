#include "pch.h"
#include "WebcamGateWay.h"
#include "GatewayOutputPin.h"

/************************************************************************************************************
* 
* CWebcamGateWay implementation
* 
************************************************************************************************************/

#if defined(_WIN64)
// {C9AB7CC5-8D37-405D-A5BC-4247EC061F5D}
const GUID CLSID_VIRTUALCAMERAFILTER =
{ 0xc9ab7cc5, 0x8d37, 0x405d,{ 0xa5, 0xbc, 0x42, 0x47, 0xec, 0x6, 0x1f, 0x5d } };

#else
// {EA1C3242-A38E-4E2F-9877-B4FB79522E59}
const GUID CLSID_VIRTUALCAMERAFILTER =
{ 0xea1c3242, 0xa38e, 0x4e2f,{ 0x98, 0x77, 0xb4, 0xfb, 0x79, 0x52, 0x2e, 0x59 } };
#endif


CWebcamGateWay::CWebcamGateWay(LPUNKNOWN punk, HRESULT *phr)
    : CSource(g_wszVirtualCamera, punk, CLSID_VIRTUALCAMERAFILTER),
    _pVideoInputFilter(NULL)
{
    *phr = OpenOriginalVideoDevice(L"HP HD Camera");

    if(SUCCEEDED(*phr)) {
        auto previewPin = new GatewayOutputPin(phr, this, _pVideoInputFilter);
        if (previewPin == NULL)
            *phr = E_OUTOFMEMORY;
        else
            *phr = S_OK;
    }
}


CWebcamGateWay::~CWebcamGateWay()
{
    SAFE_RELEASE(_pVideoInputFilter);
}


STDMETHODIMP CWebcamGateWay::NonDelegatingQueryInterface(REFIID riid,
    __deref_out void** ppv)
{
    if (riid == CLSID_VIRTUALCAMERAFILTER) {
        return GetInterface((IBaseFilter*)this, ppv);
    }

    return CSource::NonDelegatingQueryInterface(riid, ppv);
}

CUnknown * WINAPI CWebcamGateWay::CreateInstance(IUnknown *pUnk, HRESULT *phr)
{
    DbgSetModuleLevel(LOG_TRACE, 5);
    CWebcamGateWay *pNewFilter = new CWebcamGateWay(pUnk, phr);

    if (phr)
    {
        if (pNewFilter == NULL)
            *phr = E_OUTOFMEMORY;
        else
            *phr = S_OK;
    }
    return pNewFilter;
}

HRESULT FindCaptureDevice(LPCTSTR deviceName, IBaseFilter** ppVideoDevice)
{
    HRESULT hr = S_OK;
    IMoniker* pMoniker = NULL;
    ICreateDevEnum *pDevEnum = NULL;
    IEnumMoniker *pClassEnum = NULL;
    *ppVideoDevice = NULL;

    // Create the system device enumerator
    hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC,
        IID_ICreateDevEnum, (void **)&pDevEnum);
    if (FAILED(hr))
    {
        DbgLog((LOG_TRACE, 5, TEXT("Couldn't create system enumerator!  hr=0x%x"), hr));
        return hr;
    }

    // Create an enumerator for the video capture devices
    hr = pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pClassEnum, 0);
    if (FAILED(hr))
    {
        DbgLog((LOG_TRACE, 5, TEXT("Couldn't create class enumerator!  hr=0x%x"), hr));
        return hr;
    }

    // If there are no enumerators for the requested type, then 
    // CreateClassEnumerator will succeed, but pClassEnum will be NULL.
    if (pClassEnum == NULL)
    {
        DbgLog((LOG_TRACE, 5, TEXT("No video capture device was detected")));
        return E_FAIL;
    }
    pClassEnum->Reset();
    ULONG cFetched;
    IMoniker *pM;

    for (hr = pClassEnum->Next(1, &pM, &cFetched); hr == S_OK && pMoniker == NULL; hr = pClassEnum->Next(1, &pM, &cFetched))
    {
        IPropertyBag *pBag = 0;

        hr = pM->BindToStorage(0, 0, IID_IPropertyBag, (void **)&pBag);
        if (SUCCEEDED(hr))
        {
            VARIANT var;
            var.vt = VT_BSTR;
            hr = pBag->Read(L"FriendlyName", &var, NULL);
            if (hr == NOERROR)
            {
                if(wcscmp(var.bstrVal, deviceName) == 0) {
                    pMoniker = pM;
                    pM = NULL;
                }
                
                SysFreeString(var.bstrVal);
            }
            pBag->Release();
        }

        SAFE_RELEASE(pM);
    }
    if(pMoniker) {
        auto hr = pMoniker->BindToObject(0, 0, IID_IBaseFilter, (void**)ppVideoDevice);
        if (FAILED(hr))
        {
            DbgLog((LOG_TRACE, 5, TEXT("Couldn't bind moniker to filter object!  hr=0x%x"), hr));
        }
    }
    else {
        DbgLog((LOG_TRACE, 5, TEXT("Video Device %s not found"), deviceName));
    }

    SAFE_RELEASE(pDevEnum);
    SAFE_RELEASE(pClassEnum);

    return S_OK;
}

HRESULT CWebcamGateWay::OpenOriginalVideoDevice(LPCTSTR deviceName) {
    return FindCaptureDevice(deviceName, &_pVideoInputFilter);
}

HRESULT CWebcamGateWay::CloseVideoDevice() {
    return E_NOTIMPL;
}