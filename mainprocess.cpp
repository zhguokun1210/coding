// mainprocess.cpp : implementation file
//
#include "mainprocess.h"                         // for CMainProc
#include "../../inc/sglc/base/idailylog.h"           // for IDailyLog
#include "../../inc/sglc/base/itimemgr.h"           // for IDailyLog


#include <stdio.h>
#include <string.h>
#include <QStringList>
#include <qdatetime.h>
#include <math.h>
#include <qtextcodec.h>

IBasicTool* CMainProc::m_pTool = NULL;

#pragma execution_character_set("utf-8")
CMainProc::CMainProc(IBasicTool* pTool)
{
	m_pTool = pTool;
	//QTextCodec::setCodecForCStrings( QTextCodec::codecForName("GBK") );	//字符编码转换
	QTextCodec::setCodecForLocale( QTextCodec::codecForName("utf-8") );
	//QTextCodec::setCodecForTr( QTextCodec::codecForName("GBK") );



	m_CStpstra = new CStpstra(pTool);
	m_startTime = GetTimeMgr()->GetSecondTimes();



}

CMainProc::~CMainProc()
{
	delete m_CStpstra;
}

//执行统计分析处理，返回错误码。
int CMainProc::Execute()
{
	m_pTool->GetDailyLog()->WriteDailyLog(0, 0, LL_INFO, "CMainProc::Execute，m_startTime=%f,m_curTime=%f", m_startTime, m_curTime);

	if(GetTimeMgr()->GetTimeSpan(m_startTime, &m_curTime) >= 60.0)
	{
	
		
		QTime curTime = QTime::currentTime();
		
		m_pTool->GetDailyLog()->WriteDailyLog(0, 0, LL_INFO, "准备调用计划模块");
		m_CStpstra->StartSgStp(curTime);
		
		m_pTool->GetDailyLog()->WriteDailyLog(0, 0, LL_INFO, "完成计划制定");
		GetTimeMgr()->SleepInMilliSecond(60000);

		m_startTime = m_curTime;
	}

	return m_errCode;
}


//获得执行中的错误信息。返回错误码，0--无错误。
int CMainProc::GetErrorInfo(char* strInfo)
{
	if(strInfo)
	{
		strncmp(strInfo, m_errInfo, 999);
		strInfo[999] = '\0';
	}
	return m_errCode;
}


