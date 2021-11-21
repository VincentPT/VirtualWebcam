#pragma once
#include <combaseapi.h>
#include <dshow.h>


extern const CLSID MEDIASUBTYPE_GREY;
extern const CLSID MEDIASUBTYPE_Y8;
extern const CLSID MEDIASUBTYPE_Y800;
extern const CLSID MEDIASUBTYPE_Y16;
extern const CLSID MEDIASUBTYPE_BY8;

#ifdef __cplusplus
extern "C" {
#endif
    extern const CLSID CLSID_SampleGrabber;
    extern const CLSID CLSID_NullRenderer;
    extern const CLSID IID_ISampleGrabber;
#ifdef __cplusplus
}
#endif
/************************************************************************************************************
*
* Grabbfer callback interface
*
************************************************************************************************************/
interface ISampleGrabberCB : public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE SampleCB(
        double SampleTime,
        IMediaSample *pSample) = 0;

    virtual HRESULT STDMETHODCALLTYPE BufferCB(
        double SampleTime,
        BYTE *pBuffer,
        LONG BufferLen) = 0;
};

/************************************************************************************************************
*
* Grabbfer interface
*
************************************************************************************************************/
interface ISampleGrabber : public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE SetOneShot(
        BOOL OneShot) = 0;

    virtual HRESULT STDMETHODCALLTYPE SetMediaType(
        const AM_MEDIA_TYPE *pType) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetConnectedMediaType(
        AM_MEDIA_TYPE *pType) = 0;

    virtual HRESULT STDMETHODCALLTYPE SetBufferSamples(
        BOOL BufferThem) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetCurrentBuffer(
        LONG *pBufferSize,
        LONG *pBuffer) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetCurrentSample(
        IMediaSample **ppSample) = 0;

    virtual HRESULT STDMETHODCALLTYPE SetCallback(
        ISampleGrabberCB *pCallback,
        LONG WhichMethodToCallback) = 0;
};

/************************************************************************************************************
*
* Common functions
*
************************************************************************************************************/
bool checkSingleByteFormat(GUID formatType);