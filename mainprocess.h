// mainprocess.h : header file
//
#ifndef _MAINPROCESS_20211130_INCLUDED_
#define _MAINPROCESS_20211130_INCLUDED_

class IBasicTool;

#include "../../inc/sglc/task/idataextract.h" // for IStatistics
#include "ibasictool.h"           // for IBasicTool 外部工具

#include <qthread.h>
#include "stplanstra.h"

/////////////////////////////////////////////////////////////////////////////


class CMainProc : public IExtract, QThread
{
public:
	CMainProc(IBasicTool* pTool);
	virtual ~CMainProc();

	//执行统计分析处理，返回错误码。
	int Execute();

	//获得执行中的错误信息。返回错误码，0--无错误。
	int GetErrorInfo(char* strInfo=0);


	static IBasicTool* GetTool(){ return m_pTool; }
	
private:
	int m_errCode;
	char m_errInfo[1000];
	CStpstra*  m_CStpstra;
	float m_startTime;
	float m_curTime;




	static IBasicTool* m_pTool;
	
};

#endif 
