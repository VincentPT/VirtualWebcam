#include "pch.h"
#include "VirtualCamera2.h"


CVirtualCamera2::CVirtualCamera2(__inout_opt LPUNKNOWN punk) :
    CTransformFilter(g_wszVirtualCamera, punk, CLSID_VIRTUALCAMERAFILTER)
{
}


CVirtualCamera2::~CVirtualCamera2()
{
}

HRESULT CVirtualCamera2::Transform(IMediaSample * pIn, IMediaSample *pOut) {
    return E_NOTIMPL;
}

// check if you can support mtIn
HRESULT CVirtualCamera2::CheckInputType(const CMediaType* mtIn) {
    return E_NOTIMPL;
}

// check if you can support the transform from this input to this output
HRESULT CVirtualCamera2::CheckTransform(const CMediaType* mtIn, const CMediaType* mtOut) {
    return E_NOTIMPL;
}

// this goes in the factory template table to create new instances
// static CCOMObject * CreateInstance(__inout_opt LPUNKNOWN, HRESULT *);

// call the SetProperties function with appropriate arguments
HRESULT CVirtualCamera2::DecideBufferSize(
    IMemAllocator * pAllocator,
    __inout ALLOCATOR_PROPERTIES *pprop) {
    return E_NOTIMPL;
}

// override to suggest OUTPUT pin media types
HRESULT CVirtualCamera2::GetMediaType(int iPosition, __inout CMediaType *pMediaType) {
    return E_NOTIMPL;
}

CUnknown * WINAPI CVirtualCamera2::CreateInstance(IUnknown *pUnk, HRESULT *phr)
{
    DbgSetModuleLevel(LOG_TRACE, 5);
    CVirtualCamera2 *pNewFilter = new CVirtualCamera2(pUnk);

    if (phr)
    {
        if (pNewFilter == NULL)
            *phr = E_OUTOFMEMORY;
        else
            *phr = S_OK;
    }
    return pNewFilter;

}