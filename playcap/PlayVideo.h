#pragma once
#include <dshow.h>

extern HWND ghApp;

HRESULT CaptureVideo(IBaseFilter* captureDevice);
void ResizeVideoWindow(void);
HRESULT HandleGraphEvent(void);
void CloseInterfaces(void);
HRESULT ChangePreviewState(int nShow);
