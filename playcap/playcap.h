
// playcap.h : main header file for the playcap application
//
#pragma once

#ifndef __AFXWIN_H__
	#error "include 'stdafx.h' before including this file for PCH"
#endif

#include "resource.h"       // main symbols


// CplaycapApp:
// See playcap.cpp for the implementation of this class
//

class CplaycapApp : public CWinApp
{
public:
	CplaycapApp();


// Overrides
public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();

// Implementation

public:
	afx_msg void OnAppAbout();
	DECLARE_MESSAGE_MAP()
};

extern CplaycapApp theApp;
