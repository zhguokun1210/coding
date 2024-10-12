
#include "mainprocess.h"                         // for CMainProc
#include "../../inc/sglc/task/idataextract.h"           // for GetExtractObject()
#include "../../inc/sglc/ibasictool.h"           // for IBasicTool 外部工具
#include "../../inc/pnas/dao/icsstpldao.h"       

ICsstplDao* g_pCsstplDaos = NULL;


//DLL初始化操作
bool Initialize()
{
	return true;
};

//DLL反初始化操作
bool Uninitialize()
{
	return true;
};


IExtract* GetExtractObject(void* pParams[])
{
	IExtract* pObject = 0;
	if(pParams && *pParams)
	{
		g_pCsstplDaos = (ICsstplDao*)pParams[1];
		pObject = new CMainProc((IBasicTool*)pParams[0]);
	}
	return pObject;
}
