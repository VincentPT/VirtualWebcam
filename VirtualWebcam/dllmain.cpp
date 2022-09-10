// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"


#include <streams.h>
#include <initguid.h>

#define CAPTURE_SOURCE

#ifdef CAPTURE_SOURCE
#include "VirtualVideoCamera.h"

// Filter setup data
const AMOVIESETUP_MEDIATYPE sudOpPinTypes =
{
    &MEDIATYPE_Video,       // Major type
    &MEDIASUBTYPE_NULL      // Minor type
};

const AMOVIESETUP_PIN sudOutputPinDesktop =
{
    L"Output",      // Obsolete, not used.
    FALSE,          // Is this pin rendered?
    TRUE,           // Is it an output pin?
    FALSE,          // Can the filter create zero instances?
    FALSE,          // Does the filter create multiple instances?
    &CLSID_NULL,    // Obsolete.
    NULL,           // Obsolete.
    1,              // Number of media types.
    &sudOpPinTypes  // Pointer to media types.
};

const AMOVIESETUP_FILTER sudPushSourceDesktop =
{
    &CLSID_VIRTUALCAMERAFILTER,// Filter CLSID
    g_wszVirtualCamera,     // String name
    MERIT_DO_NOT_USE,       // Filter merit
    1,                      // Number pins
    &sudOutputPinDesktop    // Pin details
};

REGFILTER2 rf2FilterDesktopReg = {
    1,              // Version 1 (no pin mediums or pin category).
    MERIT_NORMAL,   // Merit.
    1,              // Number of pins.
    (REGFILTERPINS*)&sudOutputPinDesktop        // Pointer to pin information.
};

// List of class IDs and creator functions for the class factory. This
// provides the link between the OLE entry point in the DLL and an object
// being created. The class factory will call the static CreateInstance.
// We provide a set of filters in this one DLL.

CFactoryTemplate g_Templates[] =
{
    {
        g_wszVirtualCamera,               // Name
        &CLSID_VIRTUALCAMERAFILTER,       // CLSID
        CVirtualVideoCamera::CreateInstance, // Method to create an instance of MyComponent
        NULL,                           // Initialization function
        &sudPushSourceDesktop           // Set-up information (for filters)
    },
};

int g_cTemplates = sizeof(g_Templates) / sizeof(g_Templates[0]);



////////////////////////////////////////////////////////////////////////
//
// Exported entry points for registration and unregistration 
// (in this case they only call through to default implementations).
//
////////////////////////////////////////////////////////////////////////

STDAPI DllRegisterServer()
{
    HRESULT hr;
    IFilterMapper2 *pFM2 = NULL;

    hr = AMovieDllRegisterServer2(TRUE);
    if (FAILED(hr))
        return hr;

    hr = CoCreateInstance(CLSID_FilterMapper2, NULL, CLSCTX_INPROC_SERVER,
        IID_IFilterMapper2, (void **)&pFM2);

    if (FAILED(hr))
        return hr;

    hr = pFM2->RegisterFilter(
        CLSID_VIRTUALCAMERAFILTER,                // Filter CLSID. 
        g_wszVirtualCamera,                       // Filter name.
        NULL,                            // Device moniker. 
        &CLSID_VideoInputDeviceCategory,  // Video compressor category.
        g_wszVirtualCamera,                       // Instance data.
        &rf2FilterDesktopReg                    // Pointer to filter information.
    );
    pFM2->Release();
    return hr;
}

STDAPI DllUnregisterServer()
{
    //return AMovieDllRegisterServer2( FALSE );
    HRESULT hr;
    IFilterMapper2 *pFM2 = NULL;

    hr = AMovieDllRegisterServer2(FALSE);
    if (FAILED(hr))
        return hr;

    hr = CoCreateInstance(CLSID_FilterMapper2, NULL, CLSCTX_INPROC_SERVER,
        IID_IFilterMapper2, (void **)&pFM2);

    if (FAILED(hr))
        return hr;

    hr = pFM2->UnregisterFilter(&CLSID_VideoInputDeviceCategory,
        g_wszVirtualCamera, CLSID_VIRTUALCAMERAFILTER);

    pFM2->Release();
    return hr;
}

#else //CAPTURE_SOURCE
#include "VirtualCamera2.h"

// Declare media type information.
FOURCCMap fccMap;
REGPINTYPES sudInputTypes = { &MEDIATYPE_Video, &GUID_NULL };
REGPINTYPES sudOutputTypes = { &MEDIATYPE_Video, (GUID*)&fccMap };

// Declare pin information.
REGFILTERPINS sudPinReg[] = {
    // Input pin.
    { 0, FALSE, // Rendered?
    FALSE, // Output?
    FALSE, // Zero?
    FALSE, // Many?
    0, 0,
    1, &sudInputTypes  // Media types.
    },
    // Output pin.
    { 0, FALSE, // Rendered?
    TRUE, // Output?
    FALSE, // Zero?
    FALSE, // Many?
    0, 0,
    1, &sudOutputTypes      // Media types.
    }
};

// Declare filter information.
REGFILTER2 rf2FilterReg = {
    1,                // Version number.
    MERIT_DO_NOT_USE, // Merit.
    2,                // Number of pins.
    sudPinReg         // Pointer to pin information.
};

// List of class IDs and creator functions for the class factory. This
// provides the link between the OLE entry point in the DLL and an object
// being created. The class factory will call the static CreateInstance.
// We provide a set of filters in this one DLL.

CFactoryTemplate g_Templates[] =
{
    {
        g_wszVirtualCamera,
        &CLSID_VIRTUALCAMERAFILTER,
        CVirtualCamera2::CreateInstance,
        NULL,
        NULL
    }
};

int g_cTemplates = sizeof(g_Templates) / sizeof(g_Templates[0]);



////////////////////////////////////////////////////////////////////////
//
// Exported entry points for registration and unregistration 
// (in this case they only call through to default implementations).
//
////////////////////////////////////////////////////////////////////////

STDAPI DllRegisterServer()
{
    HRESULT hr;
    IFilterMapper2 *pFM2 = NULL;

    hr = AMovieDllRegisterServer2(TRUE);
    if (FAILED(hr))
        return hr;

    hr = CoCreateInstance(CLSID_FilterMapper2, NULL, CLSCTX_INPROC_SERVER,
        IID_IFilterMapper2, (void **)&pFM2);

    if (FAILED(hr))
        return hr;

    hr = pFM2->RegisterFilter(
        CLSID_VIRTUALCAMERAFILTER,                // Filter CLSID. 
        g_wszVirtualCamera,                       // Filter name.
        NULL,                            // Device moniker. 
        &CLSID_VideoInputDeviceCategory,  // Video compressor category.
        g_wszVirtualCamera,                       // Instance data.
        &rf2FilterReg                    // Pointer to filter information.
    );
    pFM2->Release();
    return hr;
}

STDAPI DllUnregisterServer()
{
    //return AMovieDllRegisterServer2( FALSE );
    HRESULT hr;
    IFilterMapper2 *pFM2 = NULL;

    hr = AMovieDllRegisterServer2(FALSE);
    if (FAILED(hr))
        return hr;

    hr = CoCreateInstance(CLSID_FilterMapper2, NULL, CLSCTX_INPROC_SERVER,
        IID_IFilterMapper2, (void **)&pFM2);

    if (FAILED(hr))
        return hr;

    hr = pFM2->UnregisterFilter(&CLSID_VideoInputDeviceCategory,
        g_wszVirtualCamera, CLSID_VIRTUALCAMERAFILTER);

    pFM2->Release();
    return hr;
}
#endif
//
// DllEntryPoint
//
extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);

BOOL APIENTRY DllMain(HANDLE hModule,
    DWORD  dwReason,
    LPVOID lpReserved)
{
	OutputDebugString(L"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX DllMain XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
    return DllEntryPoint((HINSTANCE)(hModule), dwReason, lpReserved);
}

