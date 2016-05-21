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
#include <control/SSliderBar.h>
#include <control/SListView.h>
#include <queue>
#include <map>
extern SOUI::CAutoRefPtr<SOUI::IRenderFactory> pRenderFactory;
#include <helper/SAdapterBase.h>
#pragma comment(lib,"dwmapi.lib")
namespace SOUI
{


	class CTestAdapterFix : public SAdapterBase
	{
		int * m_pCbxSel;
		CSuiVideo *m_pSuiVideo;
		std::vector<AV_Hos_Info *> m_slQue;
		std::map<int,IBitmap*> m_BitMap;
	public:

		CTestAdapterFix(CSuiVideo *pSuiVideo)
		{
			m_pSuiVideo=pSuiVideo;
			m_pSuiVideo->GetAVHistoryInfo(m_slQue);
			int count=m_slQue.size();
			if(count>0)
			{
				m_pCbxSel = new int[count];
				memset(m_pCbxSel,0,sizeof(int)*count);
			}
			

		}

		~CTestAdapterFix()
		{
			delete []m_pCbxSel;   

			for(int i=0;i<m_slQue.size();i++)
			{
				AV_Hos_Info * p=m_slQue.at(i);
				free(p->pBuffer);
				free(p);
			}
			m_slQue.clear();
		}

		virtual int getCount()
		{
			return m_slQue.size();
		}   

		virtual void getView(int position, SWindow * pItem,pugi::xml_node xmlTemplate)
		{
			if(pItem->GetChildrenCount()==0)
			{
				pItem->InitFromXml(xmlTemplate);
			}
			SImageWnd  *pAV_img= pItem->FindChildByName2<SImageWnd>(L"AV_img");
			
		
			std::map<int,IBitmap*>::iterator _It=m_BitMap.find(position);
			if(_It==m_BitMap.end())
			{
				AV_Hos_Info * pAVPic=m_slQue.at(position);
				BITMAPINFOHEADER header;
				header.biSize = sizeof(BITMAPINFOHEADER);
				int bpp=32;
				header.biWidth = pAVPic->width;
				header.biHeight = pAVPic->height*(-1);
				header.biBitCount = bpp;
				header.biCompression = 0;
				header.biSizeImage = 0;
				header.biClrImportant = 0;
				header.biClrUsed = 0;
				header.biXPelsPerMeter = 0;
				header.biYPelsPerMeter = 0;
				header.biPlanes = 1;


				//3 构造文件头
				BITMAPFILEHEADER bmpFileHeader;
				HANDLE hFile = NULL;
				DWORD dwTotalWriten = 0;
				DWORD dwWriten;

				bmpFileHeader.bfType = 0x4d42; //'BM';
				bmpFileHeader.bfOffBits=sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER);
				bmpFileHeader.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER)+ pAVPic->width*pAVPic->height*bpp/8;


				int Totl=sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER)+pAVPic->bufLen;
				unsigned char *bufx=(unsigned char *)::malloc(Totl);
				unsigned char *bufx2=bufx;
				memcpy(bufx2,&bmpFileHeader, sizeof(BITMAPFILEHEADER));

				bufx2+=sizeof(BITMAPFILEHEADER);

				memcpy(bufx2,&header, sizeof(BITMAPINFOHEADER));
				bufx2+=sizeof(BITMAPINFOHEADER);
				memcpy(bufx2,pAVPic->pBuffer, pAVPic->bufLen);
				IBitmap *pImg=NULL;
				pRenderFactory->CreateBitmap(&pImg);
				HRESULT  ll=0;//pImg->LoadFromFile(L"D:/pic/0.bmp");

				ll=pImg->LoadFromMemory(bufx,Totl);
				/*IBitmap *pImg = new IBitmap();*/

				pAV_img->SetImage(pImg);

				m_BitMap.insert(std::pair<int,IBitmap*>(position,pImg));
				::free(bufx);
			}else
			{
                 pAV_img->SetImage(_It->second);
			}
			
			
			SStatic *pTxt = pItem->FindChildByName2<SStatic>(L"AV_name");
			pTxt->SetWindowText(L"测试代码");
		}

		bool OnCbxSelChange(EventArgs *pEvt)
		{
			return true;
		}

		bool OnButtonClick(EventArgs *pEvt)
		{
			
			return true;
		}
	};


	CMainDlg::CMainDlg() : SHostWnd(_T("LAYOUT:XML_MAINWND"))
	{
		m_bLayoutInited=FALSE;
		m_PlayerState=0;
		m_lastSeekTime=0;
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

        SetMsgHandled(FALSE);
		CSuiVideo* pAV=(CSuiVideo* )FindChildByName(L"AVMainWnd");
		

		//行高固定的列表
		SListView *pLstViewFix = FindChildByName2<SListView>("AV_list");
		if(pLstViewFix)
		{
			ILvAdapter *pAdapter = new CTestAdapterFix(pAV);
			pLstViewFix->SetAdapter(pAdapter);
			pAdapter->Release();
			
		}

		return 0;
	}
	void CMainDlg::OnAVPlay()
	{
		SImageButton* pPlay=(SImageButton*)FindChildByName(L"AVPlayBtn");
		CSuiVideo* av=(CSuiVideo*)FindChildByName(L"AVMainWnd");
		if(m_PlayerState==1){
            pPlay->SetAttribute(L"skin",L"_skin.PLAY",TRUE);
			av->SetPlayStat(1);
			pPlay->UpdateWindow();
			m_PlayerState=2;
		}else if(m_PlayerState==2){
                pPlay->SetAttribute(L"skin",L"_skin.Pause",TRUE);
				av->SetPlayStat(1);
				m_PlayerState=1;
				pPlay->UpdateWindow();
		}

	}
	void CMainDlg::OnTimer(char cTimerID)
	{
        if(m_PlayerState!=0)
		{
             CSuiVideo* av=(CSuiVideo*)FindChildByName(L"AVMainWnd");
			 MEDIA_INFO info=av->GetMeadiaInfo();
			 SSliderBar *SeekBar=(SSliderBar *)FindChildByName(L"slider_video_Seek");
			 
			 std::wstring CurTimeStr;
			
			 int h=(info.CurTime/(60*60));
			 int m=(info.CurTime%(60*60))/(60);
			 int s=((info.CurTime%(60*60))%(60));
			 wchar_t strtmp[100]=L"";
			 if(h<10){
				 swprintf_s(strtmp,L"0%d:",h);
				 CurTimeStr=strtmp;
			 }
			 else{
				 swprintf_s(strtmp,L"%d:",h);
				 CurTimeStr=strtmp;
			 }
			 if(m<10){
				 swprintf_s(strtmp,L"0%d:",m);
				 CurTimeStr+=strtmp;
			 }
			 else{				  
				 swprintf_s(strtmp,L"%d:",m);
				 CurTimeStr+=strtmp;
			 }

			 if(s<10){
				 swprintf_s(strtmp,L"0%d",s);
				 CurTimeStr+=strtmp;
			 }
			 else{
				 swprintf_s(strtmp,L"%d",s);
				 CurTimeStr+=strtmp;
			 }
			 CurTimeStr+=L"/";

			

			 
			  int Min=0;
			  int Max=0;
			  SeekBar->GetRange(&Min,&Max);
			  static std::wstring hh=L"00:00:00";
			  if(info.TotalTime>0)
			  {
                  SeekBar->SetRange(0,info.TotalTime);
				  h=info.TotalTime/(60*60);
				  m=(info.TotalTime%(60*60))/60;
				  s=((info.TotalTime%(60*60))%60);


				  if(h<10)
				  {
					  swprintf_s(strtmp,L"0%d:",h);
					  hh=strtmp;
				  }
				  else{
					  swprintf_s(strtmp,L"%d:",m);
					  hh=strtmp;
				  }
				  if(m<10){
					  swprintf_s(strtmp,L"0%d:",m);
					  hh+=strtmp;
				  }
				  else{
					  swprintf_s(strtmp,L"%d:",m);
					 hh+=strtmp;
				  }
				  if(s<10){
					  swprintf_s(strtmp,L"0%d:",s);
					 hh+=strtmp;
				  }
				  else{
					  swprintf_s(strtmp,L"%d",s);
					  hh+=strtmp;
				  }
			  }
			  if(info.CurTime>m_CumrTime&&info.serial==av->PktSerial())
			  {
			     if(m_CumrTime-m_lastSeekTime<=0&&info.CurTime-m_CumrTime<10)
				 {
                       SeekBar->SetValue(info.CurTime);
					    m_CumrTime=info.CurTime;
				 }else if(m_CumrTime-m_lastSeekTime>0){
                     SeekBar->SetValue(info.CurTime);
					  m_CumrTime=info.CurTime;
				 }
				
			  }else if(m_serial==info.serial)
			  {
                  
			  }
			  m_serial=info.serial;
			  m_SeekTimer++;
			  CurTimeStr+=hh;
			  SStatic* AV_CurTimeTxt=( SStatic  *)FindChildByName(L"AV_CurTimeTxt");
			  AV_CurTimeTxt->SetWindowText(CurTimeStr.c_str());

			  
		}
	}
	bool  CMainDlg::OnSliderVideo(EventArgs *pEvt)
	{
		EventSliderPos *pEvt2 = sobj_cast<EventSliderPos>(pEvt);
		SASSERT(pEvt2);
		SSliderBar * pSlider = sobj_cast<SSliderBar>(pEvt->sender);
		SASSERT(pSlider);


		SSliderBar *VolBar=(SSliderBar *)FindChildByName(L"slider_video");
		if(m_PlayerState!=0)
		{
			m_SeekTimer=0;
			m_lastSeekTime=m_CumrTime;
			int vol=pEvt2->nPos-m_CumrTime;
			m_CumrTime+=vol;
			CSuiVideo* av=(CSuiVideo*)FindChildByName(L"AVMainWnd");
			av->AvSeek(pEvt2->nPos);
		}/**/
		return true;
	}
	bool CMainDlg::OnSliderAudio(EventArgs *pEvt)
	{
		EventSliderPos *pEvt2 = sobj_cast<EventSliderPos>(pEvt);
		SASSERT(pEvt2);
		SSliderBar * pSlider = sobj_cast<SSliderBar>(pEvt->sender);
		SASSERT(pSlider);


		SSliderBar *VolBar=(SSliderBar *)FindChildByName(L"slider_video");
		if(m_PlayerState!=0)
		{
			long vol=pEvt2->nPos*10 -10000;
			CSuiVideo* av=(CSuiVideo*)FindChildByName(L"AVMainWnd");
			av->SetVolume(vol);
		}/**/
		return true;
	}
    void CMainDlg::OnFolder()
	{
		m_CumrTime=0;
		SStatic* AV_CurTimeTxt=( SStatic  *)FindChildByName(L"AV_CurTimeTxt");
		AV_CurTimeTxt->SetWindowText(L"00:00:00/00:00:00");

		SSliderBar *SeekBar=(SSliderBar *)FindChildByName(L"slider_video_Seek");
		SeekBar->SetRange(0,10000);
		SeekBar->SetValue(0);
		wchar_t* filter = L"文件(*.mp4; *.avi; *.flv)\0*.mp4;*.avi; *.rmvb;*.flv\0全部 (*.*)\0*.*\0\0";  
		CFileDialog dlg(true, 0, 0, OFN_FILEMUSTEXIST|OFN_HIDEREADONLY|OFN_PATHMUSTEXIST, filter, m_hWnd);  
		if(dlg.DoModal() == IDOK)
		{  
			CSuiVideo* av=(CSuiVideo*)FindChildByName(L"AVMainWnd");
			char path[1024]="";
			CChineseCode::wcharTochar(dlg.m_szFileName,path,1024);
			this->KillTimer(1);
			int ret=av->OpenMeida(path);
			if(ret==-1)
			{
			     MessageBox(m_hWnd, L"文件打开错误", 0, MB_OK); 
			     m_PlayerState=0;
			     SImageButton* pPlay=(SImageButton*)FindChildByName(L"AVPlayBtn");
			     pPlay->SetAttribute(L"skin",L"_skin.PLAY",TRUE);
				 pPlay->UpdateWindow();
			}else
			{

				// SeekBar->SetRange(0,1);
				 m_PlayerState=1;
				
                 SImageButton* pPlay=(SImageButton*)FindChildByName(L"AVPlayBtn");
				 pPlay->SetAttribute(L"skin",L"_skin.Pause",TRUE);
				 pPlay->UpdateWindow();
				 this->SetTimer(1,1000);
				 SeekBar->SetValue(0);
				 m_CumrTime=0;
			}
			
		}  
	}
	void CMainDlg::OnFileList()
	{
		MessageBox(m_hWnd, L"此功能暂时未实现", 0, MB_OK);  
	}

	void CMainDlg::OnDecelerate()
	{
         CSuiVideo* av=(CSuiVideo*)FindChildByName(L"AVMainWnd");
		 av->OnDecelerate();
	}
	void CMainDlg::OnAccelerate()
	{
		CSuiVideo* av=(CSuiVideo*)FindChildByName(L"AVMainWnd");
		av->OnAccelerate();
	}
}