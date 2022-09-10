#pragma once
#include <dshow.h>

extern HWND ghApp;

HRESULT CaptureVideo(IBaseFilter* captureDevice, GUID captureMode, int selectedFormat);
void ResizeVideoWindow(void);
HRESULT HandleGraphEvent(void);
void CloseInterfaces(void);
HRESULT ChangePreviewState(int nShow);
