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


// UDP_serverDlg.cpp : implementation file
//

#include "stdafx.h"

#include "UDP_server.h"
#include "UDP_serverDlg.h"

#include "tdtp_tc_srv.h"
#include "tdtp_fc_srv.h"
#include "tdtp_fc_daq.h"

#pragma warning(disable: 4996)

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

//---------- UDP peer addr & ports (for this example only) -------------------

#define  UDP_RW_PORT_LOCAL     50001
#define  UDP_RW_PORT_REMOTE    11093

#define  UDP_DAQ_PORT_LOCAL    50000
#define  UDP_DAQ_PORT_REMOTE   11094

const char g_peer_ip_addr_def[] = _T("10.101.0.14");

//----------------------------------------------------------------------------

#define UDP_RW_RECV          1
#define UDP_RW_SEND          2


int g_op_mode = 0;

HWND g_hMsgWnd;

FILE * g_hFile_rw_fc = NULL;
FILE * g_hFile_rw_tc = NULL;

unsigned int g_nbytes_rx_fc   = 0;
unsigned int g_nbytes_rx_tc   = 0;
unsigned int g_packets_rx_daq = 0;
unsigned int g_packets_daq_repeats = 0;
unsigned int g_packets_daq_failed  = 0;


CTDTP_fc_srv * g_udp_rw_fc = NULL;
CTDTP_tc_srv * g_udp_rw_tc = NULL;
CTDTP_fc_srv_daq * g_udp_daq_fc = NULL;


//-----------

int srv_fc_prc_func_N1(TDTPSRVFC * psrv, int op_mode);
int srv_tc_prc_func_N1(TDTPSRVTC * psrv, int op_mode);
int srv_fc_daq_func_N1(TDTPSRVFC * psrv, int op_mode);


//----------------------------------------------------------------------------
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


// CUDP_serverDlg dialog




CUDP_serverDlg::CUDP_serverDlg(CWnd* pParent /*=NULL*/)
        : CDialog(CUDP_serverDlg::IDD, pParent)
{
        m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CUDP_serverDlg::DoDataExchange(CDataExchange* pDX)
{
   CDialog::DoDataExchange(pDX);
   DDX_Control(pDX, IDC_CHECK1, m_FileCheckBtn);
}

BEGIN_MESSAGE_MAP(CUDP_serverDlg, CDialog)
        ON_WM_SYSCOMMAND()
        ON_WM_PAINT()
        ON_WM_QUERYDRAGICON()
        ON_WM_TIMER()
        ON_WM_DESTROY()
        //}}AFX_MSG_MAP
   ON_BN_CLICKED(IDCANCEL, OnBnClickedCancel)
   ON_BN_CLICKED(IDC_BUTTON_FILE, OnBnClickedButtonFile)
   ON_BN_CLICKED(IDC_BUTTON_START_RW, OnBnClickedButtonStartRw)
   ON_BN_CLICKED(IDC_BUTTON_STOP_RW, OnBnClickedButtonStopRw)
   ON_BN_CLICKED(IDC_BUTTON_START_DAQ, OnBnClickedButtonStartDaq)
   ON_BN_CLICKED(IDC_BUTTON_STOP_DAQ, OnBnClickedButtonStopDaq)
   ON_BN_CLICKED(IDC_BUTTON_HELP, OnBnClickedButtonHelp)
   ON_BN_CLICKED(IDC_RADIO2, &CUDP_serverDlg::OnBnClickedRadio2)
   ON_BN_CLICKED(IDC_RADIO1, &CUDP_serverDlg::OnBnClickedRadio1)
   ON_MESSAGE(CM_THREAD_EXIT, OnThreadExit)

END_MESSAGE_MAP()


// CUDP_serverDlg message handlers

BOOL CUDP_serverDlg::OnInitDialog()
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
        SetIcon(m_hIcon, TRUE);                        // Set big icon
        SetIcon(m_hIcon, FALSE);                // Set small icon

 //--------------- extra initialization here --------------------------------

   CheckRadioButton(IDC_RADIO1, IDC_RADIO2, IDC_RADIO1);
   g_op_mode = UDP_RW_RECV;

 //--- IP Address

   DWORD hostByIP = ntohl(inet_addr(CString(g_peer_ip_addr_def)));
   CIPAddressCtrl * pIPAddr = (CIPAddressCtrl *)GetDlgItem(IDC_IP_ADDR_CLI);
   pIPAddr->SetAddress(hostByIP);

 //--- Data transferr classes

   g_udp_rw_fc = new CTDTP_fc_srv;
   g_udp_rw_tc = new CTDTP_tc_srv;
   g_udp_daq_fc = new CTDTP_fc_srv_daq;

   EnableButton(IDC_BUTTON_STOP_RW,  FALSE);
   EnableButton(IDC_BUTTON_STOP_DAQ, FALSE);

   SetTimer(1, 100, NULL);

   g_hMsgWnd = this->m_hWnd;

   return TRUE;  // return TRUE  unless you set the focus to a control
}

void CUDP_serverDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
        if((nID & 0xFFF0) == IDM_ABOUTBOX)
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

void CUDP_serverDlg::OnPaint()
{
        if(IsIconic())
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
HCURSOR CUDP_serverDlg::OnQueryDragIcon()
{
        return static_cast<HCURSOR>(m_hIcon);
}

//----------------------------------------------------------------------------
void CUDP_serverDlg::ip_addr_to_str(char * ip_addr_buf)
{
   DWORD hostByIP;

   CIPAddressCtrl * pIPAddr = (CIPAddressCtrl *)GetDlgItem(IDC_IP_ADDR_CLI);
   pIPAddr->GetAddress(hostByIP);
   sprintf(ip_addr_buf, "%d.%d.%d.%d",
                           (hostByIP>>24) & 0xFF,
                           (hostByIP>>16) & 0xFF,
                           (hostByIP>>8)  & 0xFF,
                            hostByIP & 0xFF);
}

//----------------------------------------------------------------------------
void CUDP_serverDlg::EnableButton(int id, BOOL state)
{
   CButton * pBtn = (CButton *)GetDlgItem(id);
   if(pBtn)
      pBtn->EnableWindow(state);

}

//----------------------------------------------------------------------------
void CUDP_serverDlg::EnableEdit(int id, BOOL state)
{
   CEdit * pEdit = (CEdit *)GetDlgItem(id);
   if(pEdit)
      pEdit->EnableWindow(state);

}

//----------------------------------------------------------------------------
void CUDP_serverDlg::EnableCtrls(BOOL state)
{
   EnableButton(IDC_RADIO1, state);
   EnableButton(IDC_RADIO2, state);
   EnableButton(IDC_BUTTON_FILE, state);
   EnableEdit(IDC_EDIT_FILENAME, state);
}

//-------------------- Cancel button -----------------------------------------
void CUDP_serverDlg::OnBnClickedCancel()
{
   // TODO: Add your control notification handler code here

   OnCancel();
}

//-------------------- Timer -------------------------------------------------
void CUDP_serverDlg::OnTimer(UINT_PTR nIDEvent)
{
   CStatic * pStatic;
   CString s;
   static unsigned int prev_nbytes_rx_fc    = 0xFFFFFFFF;
   static unsigned int prev_nbytes_rx_tc    = 0xFFFFFFFF;
   static unsigned int prev_packets_rx_daq  = 0xFFFFFFFF;
   static unsigned int prev_packets_daq_repeats = 0xFFFFFFFF;
   static unsigned int prev_packets_daq_failed  = 0xFFFFFFFF;

   if(g_op_mode == UDP_RW_RECV)
   {
      if(prev_nbytes_rx_fc != g_nbytes_rx_fc)//g_accepted_pkt)
      {
         s.Format("%u", g_nbytes_rx_fc); //g_accepted_pkt);
         pStatic = (CStatic *)GetDlgItem(IDC_STATIC_STATUS_RW); //IDC_STATIC_STATUS_DAQ);
         if(pStatic)
            pStatic->SetWindowText((LPCTSTR)s);
      }
      prev_nbytes_rx_fc = g_nbytes_rx_fc; //g_accepted_pkt;
   }
   else if(g_op_mode == UDP_RW_SEND)
   {
      if(prev_nbytes_rx_tc != g_nbytes_rx_tc)//g_accepted_pkt)
      {
         s.Format("%u", g_nbytes_rx_tc); //g_accepted_pkt);
         pStatic = (CStatic *)GetDlgItem(IDC_STATIC_STATUS_RW);
         if(pStatic)
            pStatic->SetWindowText((LPCTSTR)s);
      }
      prev_nbytes_rx_tc = g_nbytes_rx_tc; //g_accepted_pkt;
   }

   if(prev_packets_rx_daq != g_packets_rx_daq)
   {
      s.Format("%u", g_packets_rx_daq);
      pStatic = (CStatic *)GetDlgItem(IDC_STATIC_STATUS_DAQ);
      if(pStatic)
         pStatic->SetWindowText((LPCTSTR)s);
   }
   prev_packets_rx_daq = g_packets_rx_daq;

   if(prev_packets_daq_repeats != g_packets_daq_repeats)
   {
      s.Format("%u", g_packets_daq_repeats);
      pStatic = (CStatic *)GetDlgItem(IDC_STATIC_STATUS_REPEATS);
      if(pStatic)
         pStatic->SetWindowText((LPCTSTR)s);
   }
   prev_packets_daq_repeats = g_packets_daq_repeats;

   if(prev_packets_daq_failed != g_packets_daq_failed)
   {
      s.Format("%u", g_packets_daq_failed);
      pStatic = (CStatic *)GetDlgItem(IDC_STATIC_STATUS_FAILEDREQ);
      if(pStatic)
         pStatic->SetWindowText((LPCTSTR)s);
   }
   prev_packets_daq_failed = g_packets_daq_failed;


   CDialog::OnTimer(nIDEvent);
}

//------------- On Destroy ---------------------------------------------------
void CUDP_serverDlg::OnDestroy()
{
   if(g_op_mode == UDP_RW_RECV)
      g_udp_rw_fc->Stop();
   else if(g_op_mode == UDP_RW_SEND)
      g_udp_rw_tc->Stop();

   g_udp_daq_fc->Stop();

   if(g_udp_rw_fc)
      delete g_udp_rw_fc;
   if(g_udp_rw_tc)
      delete g_udp_rw_tc;
   if(g_udp_daq_fc)
      delete g_udp_daq_fc;

   CDialog::OnDestroy();
}

//------------- File browser dialog ------------------------------------------
void CUDP_serverDlg::OnBnClickedButtonFile()
{
   CString pathName;
   CEdit * pEdit;

   pEdit = (CEdit*)GetDlgItem(IDC_EDIT_FILENAME);

   if(g_op_mode == UDP_RW_RECV)
   {
      const char szFilters[] = "All Files (*.*)|*.*||";
      CFileDialog fileDlg (FALSE, "", "*.*", OFN_HIDEREADONLY | OFN_ENABLESIZING /*| OFN_OVERWRITEPROMPT*/, szFilters, this);

      if(fileDlg.DoModal() == IDOK)
      {
         pathName = fileDlg.GetPathName();
         pEdit->SetWindowText(pathName);
      }
   }
   else if(g_op_mode == UDP_RW_SEND)
   {
      const char szFilters[] = "All Files (*.*)|*.*||";
      CFileDialog fileDlg (TRUE, NULL, "*.*", OFN_HIDEREADONLY | OFN_ENABLESIZING, szFilters, this);

      if(fileDlg.DoModal() == IDOK)
      {
         pathName = fileDlg.GetPathName();
         pEdit->SetWindowText(pathName);
      }
   }
}

//---------------- Start R/W Button ------------------------------------------
void CUDP_serverDlg::OnBnClickedButtonStartRw()
{
   int id;
   CEdit * pEdit;
   CString s;
   CString s_remoute_ip_addr;
   FILE * hFile = NULL;

   BOOL rc;

   EnableButton(IDC_BUTTON_START_RW, FALSE);
   EnableCtrls(FALSE);

   id = GetCheckedRadioButton(IDC_RADIO1,IDC_RADIO2);

   switch(id)
   {
      case IDC_RADIO1:
         g_op_mode = UDP_RW_RECV;
         break;
      case IDC_RADIO2:
         g_op_mode = UDP_RW_SEND;
         break;
   }

   pEdit = (CEdit*)GetDlgItem(IDC_EDIT_FILENAME);
   pEdit->GetWindowText(s);
   if(s == _T(""))
   {
      EnableButton(IDC_BUTTON_START_RW, TRUE);
      EnableCtrls(TRUE);

      AfxMessageBox("Invalid file name.");

      pEdit->SetFocus();
      return;
   }

   if(g_op_mode == UDP_RW_RECV)
   {

      hFile = fopen((char*)((LPCSTR)s), "rb");
      if(hFile == NULL)
         hFile = fopen((char*)((LPCSTR)s), "wb+");
      else
      {
         CString s1;

         fclose(hFile);
         hFile = NULL;

         s1 = s + CString(_T("\n already exists. Do you want to replace it?"));
         if(AfxMessageBox(s1, MB_YESNO) == IDYES)
         {
            hFile = fopen((char*)((LPCSTR)s), "wb+");
         }
         else
         {
            EnableButton(IDC_BUTTON_START_RW, TRUE);
            EnableCtrls(TRUE);
            pEdit->SetFocus();
            return;
         }
      }
   }
   else if(g_op_mode == UDP_RW_SEND)
   {
      hFile = fopen((char*)((LPCSTR)s), "rb");
   }
   else
   {
      EnableButton(IDC_BUTTON_START_RW, TRUE);
      EnableCtrls(TRUE);
      AfxMessageBox("Invalid file name.");
      pEdit->SetFocus();
      return;
   }

   if(hFile == NULL)
   {
      CString s1;
      EnableButton(IDC_BUTTON_START_RW, TRUE);
      EnableCtrls(TRUE);
      s1 = CString(_T("Could not open file: \n")) + s;
      AfxMessageBox(s1, MB_ICONERROR);
      pEdit->SetFocus();
      return;
   }

   char peer_ip_addr[32];
   ip_addr_to_str(peer_ip_addr);

   if(g_op_mode == UDP_RW_RECV)
   {
      g_hFile_rw_fc = hFile;

      s = _T("");
      for(;;) //-- Single iteration loop
      {
         g_udp_rw_fc->m_s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
         if(g_udp_rw_fc->m_s == INVALID_SOCKET)
         {
            s.Format("Could not open socket - Error %d.", WSAGetLastError());
            break;
         }

  //-- Here we just set remoute address for the socket

         rc = g_udp_rw_fc->set_udp_remote_host(CString(peer_ip_addr), UDP_RW_PORT_REMOTE);
         if(rc == FALSE)
         {
            s = _T("Could not set remote host.");
            break;
         }

         rc = g_udp_rw_fc->do_bind(UDP_RW_PORT_LOCAL);
         if(rc == FALSE)
            s = _T("Could not set local host.");

         break;
      }
      if(s != _T(""))
      {
         fclose(hFile);
         EnableButton(IDC_BUTTON_START_RW, TRUE);
         EnableCtrls(TRUE);
         AfxMessageBox(s, MB_ICONERROR);
         return;
      }
const char cmd_1[] = _T("some_flash_data_N1");
      g_udp_rw_fc->Start(srv_fc_prc_func_N1,
                         (unsigned char*)cmd_1,
                         strlen(cmd_1));
   }
   else if(g_op_mode == UDP_RW_SEND)
   {
      g_hFile_rw_tc = hFile;

      s = _T("");
      for(;;) //-- Single iteration loop
      {
         g_udp_rw_tc->m_s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
         if(g_udp_rw_tc->m_s == INVALID_SOCKET)
         {
            s.Format("Could not open socket - Error %d.", WSAGetLastError());
            break;
         }

  //-- Here we just set remoute address for the socket

         rc = g_udp_rw_tc->set_udp_remote_host(CString(peer_ip_addr), UDP_RW_PORT_REMOTE);
         if(rc == FALSE)
         {
            s = _T("Could not set remote host.");
            break;
         }

         rc = g_udp_rw_tc->do_bind(UDP_RW_PORT_LOCAL);
         if(rc == FALSE)
            s = _T("Could not set local host.");

         break;
      }
      if(s != _T(""))
      {
         fclose(hFile);
         EnableButton(IDC_BUTTON_START_RW, TRUE);
         EnableCtrls(TRUE);
         AfxMessageBox(s, MB_ICONERROR);
         return;
      }
const char cmd_2[] = _T("some_flash_data_N2");
      g_udp_rw_tc->Start(srv_tc_prc_func_N1,
                         (unsigned char*)cmd_2,
                         strlen(cmd_2));
   }

   EnableButton(IDC_BUTTON_STOP_RW, TRUE);
}

//---------------- Stop R/W Button -------------------------------------------
void CUDP_serverDlg::OnBnClickedButtonStopRw()
{
   EnableButton(IDC_BUTTON_STOP_RW, FALSE);

   if(g_op_mode == UDP_RW_RECV)
      g_udp_rw_fc->Stop();
   else if(g_op_mode == UDP_RW_SEND)
      g_udp_rw_tc->Stop();

   EnableButton(IDC_BUTTON_START_RW, TRUE);
   EnableCtrls(TRUE);
}

//---------------- Start DAQ Button ------------------------------------------
void CUDP_serverDlg::OnBnClickedButtonStartDaq()
{
   CString s;
   BOOL rc;

   EnableButton(IDC_BUTTON_START_DAQ, FALSE);

   s = _T("");
   for(;;) //-- Single iteration loop
   {
      g_udp_daq_fc->m_s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
      if(g_udp_daq_fc->m_s == INVALID_SOCKET)
      {
         s.Format("Could not open socket - Error %d.", WSAGetLastError());
         break;
      }

//-- Here we just set remoute address for the socket

      char peer_ip_addr[32];
      ip_addr_to_str(peer_ip_addr);

      rc = g_udp_daq_fc->set_udp_remote_host(CString(peer_ip_addr), UDP_DAQ_PORT_REMOTE);
      if(rc == FALSE)
      {
         s = _T("Could not set remote host.");
         break;
      }

      rc = g_udp_daq_fc->do_bind(UDP_DAQ_PORT_LOCAL);
      if(rc == FALSE)
         s = _T("Could not set local host.");

      break;
   }
   if(s != _T(""))
   {
      EnableButton(IDC_BUTTON_START_DAQ, TRUE);
      EnableCtrls(TRUE);
      AfxMessageBox(s, MB_ICONERROR);
      return;
   }

const char cmd_3[] = _T("daq_mode_N1");
   g_udp_daq_fc->Start(srv_fc_daq_func_N1,
                      (unsigned char*)cmd_3,
                      strlen(cmd_3));

   EnableButton(IDC_BUTTON_STOP_DAQ, TRUE);
}

//---------------- Stop DAQ Button -------------------------------------------
void CUDP_serverDlg::OnBnClickedButtonStopDaq()
{
   EnableButton(IDC_BUTTON_STOP_DAQ, FALSE);

   g_udp_daq_fc->Stop();

   EnableButton(IDC_BUTTON_START_DAQ, TRUE);
}

//---------------- Info Button -----------------------------------------------
void CUDP_serverDlg::OnBnClickedButtonHelp()
{
   CString cmdLine;
   STARTUPINFO si;
   PROCESS_INFORMATION pi;

   cmdLine  = "lister.exe /A /T1  tdtp_udp_server_info.txt";

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

//----------------------------------------------------------------------------
void CUDP_serverDlg::OnBnClickedRadio1()
{
   g_op_mode = UDP_RW_RECV;
}

//----------------------------------------------------------------------------
void CUDP_serverDlg::OnBnClickedRadio2()
{
   g_op_mode = UDP_RW_SEND;
}
//----------------------------------------------------------------------------
LRESULT CUDP_serverDlg::OnThreadExit(WPARAM wpar, LPARAM lpar)
{
   if(lpar == 1)
      OnBnClickedButtonStopRw();
   else if(lpar == 2)
      OnBnClickedButtonStopDaq();

   return 0;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
/*
//----------------------------------------------------------------------------
int srv_wr_init(TDTPSRVWR * psrv)
{
   memset(psrv, 0, sizeof(TDTPSRVWR));

   psrv->s_tx = (void*)&g_srv_tx_ch;
   psrv->s_rx = (void*)&g_srv_rx_ch;

   psrv->tx_buf = g_srv_tx_buf;
   psrv->rx_buf = g_srv_rx_buf;

   psrv->rdata_func  = srv_wr_prc_func;
   psrv->wr_blk_data = g_srv_file_rd_buf;

   psrv->wr_sid = 0; //-- Wrong; must be rand()

   return ERR_OK;
}

//----------------------------------------------------------------------------
int srv_rd_init(TDTPSRVRD * psrv)
{
   memset(psrv, 0, sizeof(TDTPSRVRD));

   psrv->s_tx = (void*)&g_srv_tx_ch;
   psrv->s_rx = (void*)&g_srv_rx_ch;

   psrv->tx_buf = g_srv_tx_buf;
   psrv->rx_buf = g_srv_rx_buf;

   psrv->data_prc_func = srv_rd_prc_func;;

   psrv->rd_sid = 0; //-- Wrong; must be rand()

   return ERR_OK;
}
*/
