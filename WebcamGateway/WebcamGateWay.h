#pragma once
#include <Dshow.h>
#include <streams.h>


class CWebcamGateWay : public CSource
{
    IBaseFilter* _pVideoInputFilter;          // Video Capture filter

private:
    HRESULT OpenOriginalVideoDevice(LPCTSTR deviceName);
    HRESULT CloseVideoDevice();
public:
    CWebcamGateWay(__inout_opt LPUNKNOWN, HRESULT* hr);
    ~CWebcamGateWay();
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, __deref_out void** ppv);
    // =================================================================
    // create filter instance
    // =================================================================
    static CUnknown * WINAPI CreateInstance(IUnknown *pUnk, HRESULT *phr);
};

