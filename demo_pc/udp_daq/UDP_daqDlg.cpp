/*

Copyright © 2009 Yuri Tiomkin
All rights reserved.

Permission to use, copy, modify, and distribute this software in source
and binary forms and its documentation for any purpose and without fee
is hereby granted, provided that the above copyright notice appear
in all copies and that both that copyright notice and this permission
notice appear in supporting documentation.

THIS SOFTWARE IS PROVIDED BY THE YURI TIOMKIN AND CONTRIBUTORS ``AS IS'' AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL YURI TIOMKIN OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
SUCH DAMAGE.
*/


// UDP_daqDlg.cpp : implementation file
//

#include "stdafx.h"
#include "UDP_daq.h"
#include "UDP_daqDlg.h"

#include "fc_daq.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

#define  UDP_DAQ_PORT_LOCAL     50000

HWND g_hMsgWnd;
CDaq * g_daq = NULL;
int g_num_packets = 0;
//----------------------------------------------------------------------------

// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
END_MESSAGE_MAP()


// CUDP_daqDlg dialog




CUDP_daqDlg::CUDP_daqDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CUDP_daqDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CUDP_daqDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CUDP_daqDlg, CDialog)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	//}}AFX_MSG_MAP
   ON_WM_TIMER()
   ON_WM_DESTROY()
   ON_BN_CLICKED(IDC_BUTTON_INFO,  OnBnClickedButtonInfo)
   ON_BN_CLICKED(IDC_BUTTON_START, OnBnClickedButtonStart)
   ON_BN_CLICKED(IDC_BUTTON_STOP,  OnBnClickedButtonStop)
   ON_MESSAGE(CM_THREAD_EXIT, OnThreadExit)
END_MESSAGE_MAP()


// CUDP_daqDlg message handlers

BOOL CUDP_daqDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// TODO: Add extra initialization here

   EnableButton(IDC_BUTTON_STOP, FALSE);

   g_daq = new CDaq;

   g_hMsgWnd = this->m_hWnd;

   SetTimer(1, 100, NULL);

	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CUDP_daqDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CUDP_daqDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CUDP_daqDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

//----------------------------------------------------------------------------
void CUDP_daqDlg::EnableButton(int id, BOOL state)
{
   CButton * pBtn = (CButton *)GetDlgItem(id);
   if(pBtn)
      pBtn->EnableWindow(state);

}

//------------ Timer ---------------------------------------------------------
void CUDP_daqDlg::OnTimer(UINT_PTR nIDEvent)
{
   CStatic * pStatic;
   CString s;
   static unsigned int prev_num_packets = 0xFFFFFFFF;

   if(prev_num_packets != g_num_packets)
   {
      s.Format("%u", g_num_packets);
      pStatic = (CStatic *)GetDlgItem(IDC_STATIC_NPKT);
      if(pStatic)
         pStatic->SetWindowText((LPCTSTR)s);
   }
   prev_num_packets = g_num_packets;


   CDialog::OnTimer(nIDEvent);
}
//----------------------------------------------------------------------------
void CUDP_daqDlg::OnDestroy()
{
   g_daq->Stop();
   delete g_daq;

   CDialog::OnDestroy();

}

//------------ Info ----------------------------------------------------------
void CUDP_daqDlg::OnBnClickedButtonInfo()
{
   CString cmdLine;
   STARTUPINFO si;
   PROCESS_INFORMATION pi;

   cmdLine  = "lister.exe /A /T1  tdtp_UDP_daq_info.txt";

  //--------------------------------
   ZeroMemory( &si, sizeof(si) );
  // si.dwFlags =  STARTF_USESHOWWINDOW;
   si.wShowWindow =  SW_SHOW;
   si.cb = sizeof(si);

  //-- Start the test DOS perf child process.
   if(!CreateProcess( NULL, // No module name (use command line).
       (LPTSTR)((LPCSTR)cmdLine),        // Command line.
          NULL,             // Process handle not inheritable.
          NULL,             // Thread handle not inheritable.
          FALSE,            // Set handle inheritance to FALSE.
          0,                // No creation flags.
          NULL,             // Use parent's environment block.
          NULL,
          &si,              // Pointer to STARTUPINFO structure.
          &pi ))             // Pointer to PROCESS_INFORMATION structure.
  {
      AfxMessageBox("Could not run info viewer.\n(file \"lister.exe\" does not exists ???)");
      return;
  }

   //-- Wait until child process exits.
  //WaitForSingleObject( pi.hProcess, INFINITE );
  //CloseHandle( pi.hThread );
  //CloseHandle( pi.hProcess );
}

//------------ Start ---------------------------------------------------------
void CUDP_daqDlg::OnBnClickedButtonStart()
{
   CString s;
   BOOL rc;

   EnableButton(IDC_BUTTON_START, FALSE);

   s = _T("");
   for(;;) //-- Single iteration loop
   {
      g_daq->m_s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
      if(g_daq->m_s == INVALID_SOCKET)
      {
         s.Format("Could not open socket - Error %d.", WSAGetLastError());
         break;
      }

      rc = g_daq->do_bind(UDP_DAQ_PORT_LOCAL);
      if(rc == FALSE)
         s = _T("Could not set local host.");

      break;
   }
   if(s != _T(""))
   {
      EnableButton(IDC_BUTTON_START, TRUE);
      AfxMessageBox(s, MB_ICONERROR);
      return;
   }

   g_daq->Start();

   EnableButton(IDC_BUTTON_STOP, TRUE);

}

//------------ Stop ----------------------------------------------------------
void CUDP_daqDlg::OnBnClickedButtonStop()
{
   EnableButton(IDC_BUTTON_STOP, FALSE);

   g_daq->Stop();

   EnableButton(IDC_BUTTON_START, TRUE);

}

//----------------------------------------------------------------------------
LRESULT CUDP_daqDlg::OnThreadExit(WPARAM wpar, LPARAM lpar)
{
   OnBnClickedButtonStop();

   return 0;
}
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
