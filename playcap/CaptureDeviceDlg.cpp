// CaptureDeviceDlg.cpp : implementation file
//

#include "stdafx.h"
#include "playcap.h"
#include "CaptureDeviceDlg.h"
#include "afxdialogex.h"
#include <streams.h>
#include <strsafe.h>
#include <comdef.h>

// CCaptureDeviceDlg dialog

IMPLEMENT_DYNAMIC(CCaptureDeviceDlg, CDialogEx)

CCaptureDeviceDlg::CCaptureDeviceDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(IDD_CAPTURE_DEVICES, pParent)
	, streamCaptureMode(FALSE)
{
    m_captureDevice = NULL;
	m_CaptureMode = PIN_CATEGORY_PREVIEW;
	m_selectedFormat = -1;
}

CCaptureDeviceDlg::~CCaptureDeviceDlg()
{
    ClearDeviceList();
    SAFE_RELEASE(m_captureDevice);
}

void CCaptureDeviceDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_COMBO1, cbDeviceList);
	DDX_Control(pDX, IDC_COMBO2, cbVideoCaps);
	DDX_Check(pDX, IDC_CHECK1, streamCaptureMode);
}


BEGIN_MESSAGE_MAP(CCaptureDeviceDlg, CDialogEx)
    ON_BN_CLICKED(IDOK, &CCaptureDeviceDlg::OnBnClickedOk)
	ON_CBN_SELCHANGE(IDC_COMBO1, &CCaptureDeviceDlg::OnCbnSelchangeCombo1)
	ON_CBN_SELCHANGE(IDC_COMBO2, &CCaptureDeviceDlg::OnCbnSelchangeCombo2)
END_MESSAGE_MAP()


// CCaptureDeviceDlg message handlers


BOOL CCaptureDeviceDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    ListCaptureDevices();

    return TRUE;
}


void Msg(TCHAR *szFormat, ...)
{
    TCHAR szBuffer[1024];  // Large buffer for long filenames or URLs
    const size_t NUMCHARS = sizeof(szBuffer) / sizeof(szBuffer[0]);
    const int LASTCHAR = NUMCHARS - 1;

    // Format the input string
    va_list pArgs;
    va_start(pArgs, szFormat);

    // Use a bounded buffer size to prevent buffer overruns.  Limit count to
    // character size minus one to allow for a NULL terminating character.
    (void)StringCchVPrintf(szBuffer, NUMCHARS - 1, szFormat, pArgs);
    va_end(pArgs);

    // Ensure that the formatted string is NULL-terminated
    szBuffer[LASTCHAR] = TEXT('\0');

    MessageBox(NULL, szBuffer, TEXT("PlayCap Message"), MB_OK | MB_ICONERROR);
}


void CCaptureDeviceDlg::ListCaptureDevices()
{
    HRESULT hr = S_OK;
    IMoniker* pMoniker = NULL;
    ICreateDevEnum *pDevEnum = NULL;
    IEnumMoniker *pClassEnum = NULL;


    // Create the system device enumerator
    hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC,
        IID_ICreateDevEnum, (void **)&pDevEnum);
    if (FAILED(hr))
    {
        Msg(TEXT("Couldn't create system enumerator!  hr=0x%x"), hr);
    }

    // Create an enumerator for the video capture devices

    if (SUCCEEDED(hr))
    {
        hr = pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pClassEnum, 0);
        if (FAILED(hr))
        {
            Msg(TEXT("Couldn't create class enumerator!  hr=0x%x"), hr);
        }
    }

    if (SUCCEEDED(hr))
    {
        // If there are no enumerators for the requested type, then 
        // CreateClassEnumerator will succeed, but pClassEnum will be NULL.
        if (pClassEnum == NULL)
        {
            MessageBox(TEXT("No video capture device was detected.\r\n\r\n")
                TEXT("This sample requires a video capture device, such as a USB WebCam,\r\n")
                TEXT("to be installed and working properly.  The sample will now close."),
                TEXT("No Video Capture Hardware"), MB_OK | MB_ICONINFORMATION);
            hr = E_FAIL;
        }
    }
    ClearDeviceList();
    if (SUCCEEDED(hr))
    {
        pClassEnum->Reset();
        ULONG cFetched;
        IMoniker *pM;

        for (hr = pClassEnum->Next(1, &pM, &cFetched); hr == S_OK; hr = pClassEnum->Next(1, &pM, &cFetched))
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
                    int idx = cbDeviceList.AddString(var.bstrVal);
                    cbDeviceList.SetItemDataPtr(idx, pM);
                    m_deviceReferences.Add(pM);
                    pM = NULL;
                    SysFreeString(var.bstrVal);
                }
                pBag->Release();
            }

            SAFE_RELEASE(pM);
        }
    }
    if (m_deviceReferences.GetCount()) {
        cbDeviceList.SetCurSel(0);
		OnCbnSelchangeCombo1();
    }

    SAFE_RELEASE(pMoniker);
    SAFE_RELEASE(pDevEnum);
    SAFE_RELEASE(pClassEnum);
}

void CCaptureDeviceDlg::ClearDeviceList() {
    for (int i = 0; i < m_deviceReferences.GetCount(); ++i) {
        m_deviceReferences[i]->Release();
    }
    m_deviceReferences.RemoveAll();
}

IBaseFilter* CCaptureDeviceDlg::getCaptureDevice() {
    if (m_captureDevice) {
        m_captureDevice->AddRef();
    }

    return m_captureDevice;
}

void CCaptureDeviceDlg::OnBnClickedOk()
{
    int idx = cbDeviceList.GetCurSel();
    if (idx >= 0) {
        IMoniker* pMoniker = (IMoniker*)cbDeviceList.GetItemDataPtr(idx);
        // Bind Moniker to a filter object
        SAFE_RELEASE(m_captureDevice);

        auto hr = pMoniker->BindToObject(0, 0, IID_IBaseFilter, (void**)&m_captureDevice);
        if (FAILED(hr))
        {
            Msg(TEXT("Couldn't bind moniker to filter object!  hr=0x%x"), hr);
        }
    }
    // TODO: Add your control notification handler code here
    CDialogEx::OnOK();
}

HRESULT CCaptureDeviceDlg::ObtainVideoCapbilities(IBaseFilter* pVideoInputDevice) {
	cbVideoCaps.ResetContent();
	streamCaptureMode = TRUE;

	GUID _captureMode = PIN_CATEGORY_CAPTURE;
	ICaptureGraphBuilder2* _pCaptureGraph;
	IGraphBuilder* _pGraph;                    // Graph builder object
	IAMStreamConfig* _streamConf;

	// CREATE THE GRAPH BUILDER //
	// Create the filter graph manager and query for interfaces.
	HRESULT hr = CoCreateInstance(CLSID_CaptureGraphBuilder2, NULL, CLSCTX_INPROC_SERVER, IID_ICaptureGraphBuilder2, (void **)&_pCaptureGraph);
	if (FAILED(hr))    // FAILED is a macro that tests the return value
	{
		return hr;
	}

	//FILTER GRAPH MANAGER//
	// Create the Filter Graph Manager.
	hr = CoCreateInstance(CLSID_FilterGraph, 0, CLSCTX_INPROC_SERVER, IID_IGraphBuilder, (void**)&_pGraph);
	if (FAILED(hr))
	{
		return hr;
	}

	//SET THE FILTERGRAPH//
	hr = _pCaptureGraph->SetFiltergraph(_pGraph);
	if (FAILED(hr))
	{
		return hr;
	}

	hr = _pGraph->AddFilter(pVideoInputDevice, TEXT("Temp filter"));
	if (FAILED(hr)) {		
		return hr;
	}

	//LOOK FOR PREVIEW PIN IF THERE IS NONE THEN WE USE CAPTURE PIN AND THEN SMART TEE TO PREVIEW
	IAMStreamConfig *streamConfTest = NULL;
	hr = _pCaptureGraph->FindInterface(&PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video, pVideoInputDevice, IID_IAMStreamConfig, (void **)&streamConfTest);
	if (SUCCEEDED(hr)) {
		_captureMode = PIN_CATEGORY_PREVIEW;
		streamCaptureMode = FALSE;		
		streamConfTest->Release();
		streamConfTest = NULL;
	}
	// update data to control
	UpdateData(FALSE);
	m_CaptureMode = _captureMode;

	// get stream again prefer to preview mode
	hr = _pCaptureGraph->FindInterface(&_captureMode, &MEDIATYPE_Video, pVideoInputDevice, IID_IAMStreamConfig, (void **)&_streamConf);
	if (FAILED(hr)) {		
		return hr;
	}
	if (!_streamConf) {
		return E_FAIL;
	}

	int streamFormatSize = 0;
	int _streamCaps = 0;
	hr = _streamConf->GetNumberOfCapabilities(&_streamCaps, &streamFormatSize);
	if (FAILED(hr)) {		
		return hr;
	}

	if (streamFormatSize != sizeof(VIDEO_STREAM_CONFIG_CAPS)) {
		return E_FAIL;
	}
	VIDEO_STREAM_CONFIG_CAPS scc;
	AM_MEDIA_TYPE* pmt;
	for (int i = 0; i < _streamCaps; ++i) {
		pmt = NULL;
		hr = _streamConf->GetStreamCaps(i, &pmt, (BYTE*)&scc);
		if (SUCCEEDED(hr)) {
			DECLARE_PTR(VIDEOINFOHEADER, pvio, pmt->pbFormat);

			CString frameFormatStr;			
			frameFormatStr.Format(TEXT("%dx%d(%d bits) %dfps"), HEADER(pvio)->biWidth, HEADER(pvio)->biHeight, HEADER(pvio)->biBitCount, UNITS/pvio->AvgTimePerFrame);

			int idx = cbVideoCaps.AddString(frameFormatStr);
			//cbVideoCaps.SetItemDataPtr(idx, nullptr);
			
			DeleteMediaType(pmt);
		}
	}

	if (_streamCaps) {
		cbVideoCaps.SetCurSel(0);
		OnCbnSelchangeCombo2();
	}

	SAFE_RELEASE(_pCaptureGraph);    // Capture graph builder object
	SAFE_RELEASE(_pGraph);                    // Graph builder object
	SAFE_RELEASE(_streamConf);
	return S_OK;
}


void CCaptureDeviceDlg::OnCbnSelchangeCombo1()
{
	int idx = cbDeviceList.GetCurSel();
	if (idx >= 0) {
		IMoniker* pMoniker = (IMoniker*)cbDeviceList.GetItemDataPtr(idx);
		// Bind Moniker to a filter object
		IBaseFilter* captureDevice = nullptr;

		auto hr = pMoniker->BindToObject(0, 0, IID_IBaseFilter, (void**)&captureDevice);
		if (SUCCEEDED(hr))
		{			
			ObtainVideoCapbilities(captureDevice);
			SAFE_RELEASE(captureDevice);
		}		
	}
}

GUID CCaptureDeviceDlg::GetCaptureMode() const {
	return m_CaptureMode;
}

int CCaptureDeviceDlg::GetSelectedFormat() const {
	return m_selectedFormat;
}


void CCaptureDeviceDlg::OnCbnSelchangeCombo2()
{
	int idx = cbVideoCaps.GetCurSel();
	m_selectedFormat = idx;
}
