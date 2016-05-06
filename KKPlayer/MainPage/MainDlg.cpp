// MainDlg.cpp : implementation of the CMainDlg class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "MainDlg.h"
#include <dwmapi.h>
#include <atldlgs.h>
#include "SuiVideo.h"
#include "../Tool/cchinesecode.h"
#include <control/SCmnCtrl.h>
#pragma comment(lib,"dwmapi.lib")
namespace SOUI
{
	CMainDlg::CMainDlg() : SHostWnd(_T("LAYOUT:XML_MAINWND"))
	{
		m_bLayoutInited=FALSE;
		m_PlayerState=0;
	} 

	CMainDlg::~CMainDlg()
	{
	}

	int CMainDlg::OnCreate( LPCREATESTRUCT lpCreateStruct )
	{
		// 		MARGINS mar = {5,5,30,5};
		// 		DwmExtendFrameIntoClientArea ( m_hWnd, &mar );
		SetMsgHandled(FALSE);
		return 0;
	}

	void CMainDlg::OnShowWindow( BOOL bShow, UINT nStatus )
	{
		if(bShow)
		{
 			//AnimateHostWindow(200,AW_CENTER);
		}
	}


	BOOL CMainDlg::OnInitDialog( HWND hWnd, LPARAM lParam )
	{
		m_bLayoutInited=TRUE;

		return 0;
	}
	void CMainDlg::OnAVPlay()
	{
		SImageButton* pPlay=(SImageButton*)FindChildByName(L"AVPlayBtn");
		CSuiVideo* av=(CSuiVideo*)FindChildByName(L"AVMainWnd");
		if(m_PlayerState==1){
            pPlay->SetAttribute(L"skin",L"_skin.PLAY",TRUE);
			av->SetPlayStat(1);
			m_PlayerState=2;
		}else if(m_PlayerState==2){
                pPlay->SetAttribute(L"skin",L"_skin.Pause",TRUE);
				av->SetPlayStat(1);
				m_PlayerState=1;
		}

	}
    void CMainDlg::OnFolder()
	{
		wchar_t* filter = L"文件(*.mp4; *.avi; *.flv)\0*.mp4;*.avi; *.rmvb;*.flv\0全部 (*.*)\0*.*\0\0";  
		CFileDialog dlg(true, 0, 0, OFN_FILEMUSTEXIST|OFN_HIDEREADONLY|OFN_PATHMUSTEXIST, filter, m_hWnd);  
		if(dlg.DoModal() == IDOK)
		{  
			CSuiVideo* av=(CSuiVideo*)FindChildByName(L"AVMainWnd");
			char path[1024]="";
			CChineseCode::wcharTochar(dlg.m_szFileName,path,1024);
			int ret=av->OpenMeida(path);
			if(ret==-1)
			{
			     MessageBox(m_hWnd, L"文件打开错误", 0, MB_OK); 
			     m_PlayerState=0;
			     SImageButton* pPlay=(SImageButton*)FindChildByName(L"AVPlayBtn");
			     pPlay->SetAttribute(L"skin",L"_skin.PLAY",TRUE);
			}else
			{
				 m_PlayerState=1;
                 SImageButton* pPlay=(SImageButton*)FindChildByName(L"AVPlayBtn");
				 pPlay->SetAttribute(L"skin",L"_skin.Pause",TRUE);
			}
			
		}  
	}
	void CMainDlg::OnFileList()
	{
		MessageBox(m_hWnd, L"此功能暂时未实现", 0, MB_OK);  
	}
}