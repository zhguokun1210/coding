#pragma once
#include <stdio.h>
#include <string.h>
#include <QStringList>
#include <qdatetime.h>
#include <math.h>

#include "../../inc/hisdb/hdbaccessintf.h"

#include "../../inc/sglc/icsintflib.h"           // for CICSIntfLib
#include <qmutex.h>

#include "../../inc/pnas/dao/icsstpldao.h"       


class IBasicTool;

class CStpstra 
{
public:
	CStpstra(IBasicTool* pTool);
	~CStpstra();

	int fileNo;
	IHdbAccess *m_IHdbAccess;
	void ConnectHdb();

	//获取新能源场站短期功率预测值pfstationshorthub
	void Getpfstationshorthub(int id, QList<QVariantList> m_resultList);

	void StartSgStp(QTime curTime);

	void toDistableData(QString tableName, int planId, int daynum, QList<QVariantList> dvalList);
private:
	IBasicTool* m_pFriend;
};

