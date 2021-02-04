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


// UDP_serverDlg.h : header file
//

#pragma once
#include "afxwin.h"
#include "resource.h"


// CUDP_serverDlg dialog
class CUDP_serverDlg : public CDialog
{
// Construction
public:
	CUDP_serverDlg(CWnd* pParent = NULL);	// standard constructor

   CButton m_FileCheckBtn;

   void EnableButton(int id, BOOL state);
   void EnableEdit(int id, BOOL state);
   void EnableCtrls(BOOL state);
   void ip_addr_to_str(char * ip_addr_buf);

// Dialog Data
	enum { IDD = IDD_UDP_SERVER_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support


// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
   afx_msg void OnDestroy();
   afx_msg void OnBnClickedCancel();
   afx_msg void OnTimer(UINT_PTR nIDEvent);
   afx_msg void OnBnClickedButtonFile();
   afx_msg void OnBnClickedButtonStartRw();
   afx_msg void OnBnClickedButtonStopRw();
   afx_msg void OnBnClickedButtonStartDaq();
   afx_msg void OnBnClickedButtonStopDaq();
   afx_msg void OnBnClickedButtonHelp();
   afx_msg void OnBnClickedRadio2();
   afx_msg void OnBnClickedRadio1();
   afx_msg LRESULT OnThreadExit(WPARAM wpar, LPARAM lpar);

};
