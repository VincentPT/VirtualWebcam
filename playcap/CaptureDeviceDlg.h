#pragma once
#include "afxwin.h"
#include <dshow.h>

// CCaptureDeviceDlg dialog

class CCaptureDeviceDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CCaptureDeviceDlg)

public:
	CCaptureDeviceDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CCaptureDeviceDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_CAPTURE_DEVICES };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
    CComboBox cbDeviceList;
    virtual BOOL OnInitDialog();
    IBaseFilter* getCaptureDevice();
private:
    void ListCaptureDevices();
    void ClearDeviceList();
	HRESULT ObtainVideoCapbilities(IBaseFilter* pVideoInputDevice);
private:
    IBaseFilter* m_captureDevice;
    CArray<IMoniker*> m_deviceReferences;
	int m_selectedFormat;
	GUID m_CaptureMode;
public:
    afx_msg void OnBnClickedOk();
	afx_msg void OnCbnSelchangeCombo1();
	afx_msg void OnCbnSelchangeCombo2();

	GUID GetCaptureMode() const;
	int GetSelectedFormat() const;

	CComboBox cbVideoCaps;
	BOOL streamCaptureMode;
};
