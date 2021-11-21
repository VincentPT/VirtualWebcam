#pragma once
#include <Dshow.h>
#include <streams.h>

class CVirtualCamera2 : public CTransformFilter
{
public:
    CVirtualCamera2(__inout_opt LPUNKNOWN);
    ~CVirtualCamera2();

    // =================================================================
    // CTransformFilter interfaces
    // =================================================================
    virtual HRESULT Transform(IMediaSample * pIn, IMediaSample *pOut);

    // check if you can support mtIn
    virtual HRESULT CheckInputType(const CMediaType* mtIn);

    // check if you can support the transform from this input to this output
    virtual HRESULT CheckTransform(const CMediaType* mtIn, const CMediaType* mtOut);

    // this goes in the factory template table to create new instances
    // static CCOMObject * CreateInstance(__inout_opt LPUNKNOWN, HRESULT *);

    // call the SetProperties function with appropriate arguments
    virtual HRESULT DecideBufferSize(
        IMemAllocator * pAllocator,
        __inout ALLOCATOR_PROPERTIES *pprop);

    // override to suggest OUTPUT pin media types
    virtual HRESULT GetMediaType(int iPosition, __inout CMediaType *pMediaType);

    // =================================================================
    // create filter instance
    // =================================================================
    static CUnknown * WINAPI CreateInstance(IUnknown *pUnk, HRESULT *phr);
};

