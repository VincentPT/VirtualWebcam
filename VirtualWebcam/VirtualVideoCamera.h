#pragma once
#include <Dshow.h>
#include <streams.h>

class CVirtualVideoCamera : public CSource
{
public:
    CVirtualVideoCamera(LPUNKNOWN punk, HRESULT *phr);
    ~CVirtualVideoCamera();

	// override this to say what interfaces we support where
	STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, __deref_out void** ppv);
    STDMETHODIMP JoinFilterGraph(
            __inout_opt IFilterGraph * pGraph,
            __in_opt LPCWSTR pName);
    static CUnknown * WINAPI CreateInstance(IUnknown *pUnk, HRESULT *phr);
};

