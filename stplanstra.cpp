#include "stplanstra.h"

#include "../../inc/sglc/dao/iscadadao.h"    // for IScadaDao
#include "../../inc/sglc/base/idailylog.h"           // for IDailyLog
#include "../../inc/sglc/msgc/iactionalarm.h"           // for IActionAlarm
#include "../../inc/sglc/base/itimemgr.h"           // for IDailyLog
#include <stdio.h>
#include <string.h>
#include <QStringList>
#include <qdatetime.h>
#include <math.h>
#include <qthread.h>
#include "../../../E8000/inc/rtdb/e8000_rtdbaccess_scada.h"
#include "../../inc/sglc/base/idatetime.h"

#include "../../inc/sglc/ibasictool.h"           // for IBasicTool 外部工具
#include <QSqlQuery>
#include "../../inc/pnas/dao/icsstpldao.h"       
#include "../../inc/include/gui/lib/uiutil/uiutil.h"


extern ICsstplDao* g_pCsstplDaos;



CStpstra::CStpstra(IBasicTool* pTool) : m_pFriend(pTool)
{
	m_IHdbAccess = NULL;


	ConnectHdb();

}


CStpstra::~CStpstra()
{
	if (m_IHdbAccess != NULL)
	{
		delete m_IHdbAccess;
	}
}

void CStpstra::ConnectHdb()
{
	m_IHdbAccess = GetHdbAccess();

	if (m_IHdbAccess)
	{
		m_pFriend->GetDailyLog()->WriteDailyLog(0,fileNo, LL_INFO, "hdbconnect suc");
	}
	else
	{
		m_pFriend->GetDailyLog()->WriteDailyLog(0,fileNo, LL_INFO, "hdbconnect fail");

	}
}




void CStpstra::StartSgStp(QTime curTime)//优化计划
{
	int curh = curTime.hour();
	int curmin = curTime.minute();
	m_pFriend->GetDailyLog()->WriteDailyLog(0, fileNo, LL_INFO, "进入计划制定模块,curh =%d,curmin=%d", curh, curmin);

	//配置信息获取
	//例：获取风电站计划参数plstation
	int StationPlanNum = g_pCsstplDaos->GetPldstparDao()->GetRecordCount();
	QList<int> WindStationPlanIDList;
	QList<int> WindStationstationidList;
	QList<double> WindStationcapList;
	for (int i = 0; i < StationPlanNum; i++)
	{
		int gentype = g_pCsstplDaos->GetPlstationDao()->GetgentypeByPos(i);
		if (gentype == 3) //发电类型为风电站
		{
			//配置信息：计划id
			int stationplanID = g_pCsstplDaos->GetPlstationDao()->GetIdenByPos(i);
			WindStationPlanIDList.append(stationplanID);

			//配置信息：场站id，用于后续预测数据读取
			std::string sctrlid(g_pCsstplDaos->GetPlstationDao()->GetirdfidByPos(i));
			QString s_stationid = QString::fromStdString(sctrlid);
			int stationid = s_stationid.section("_", 1, 1).toInt();
			WindStationstationidList.append(stationid);

			//例 配置信息：装机容量
			//需要其他相关配置信息的话，按照g_pCsstplDaos->GetPlstationDao()->GetcapByPos(i)此类方式即可获取
			double cap = g_pCsstplDaos->GetPlstationDao()->GetcapByPos(i);
			WindStationcapList.append(cap);
		}
	}


	//预测数据获取
	//例: 获取新能源场站短期功率预测值pfstationshorthub
	QList<QVariantList> m_resultList;
	int selectedstationid = WindStationstationidList.at(0);
	//m_resultList中存储了查询的场站所对应的未来一日各时间节点的预测数据
	Getpfstationshorthub(selectedstationid,  m_resultList);

	//例：获取场站未来一日的第一个点的预测数据
	double ForecastData_1 = m_resultList.at(0).at(2).toDouble();




	//自定义优化算法
	//基于上述获取到的场站预测数据以及需要的配置信息，进行自定义优化计划制定
	double PlanData_1 = 100;//优化算法修改后的场站未来一日的第一个点的计划值



	//存储优化计划  
	//示例待升级
	toDistableData();

}

void CStpstra::Getpfstationshorthub(int id, QList<QVariantList> m_resultList)
{
	//新能源场站短期功率预测值历史表
	QString n_tablename = "pfstationshorthub_2024";
	QString n_fields = "rdfid,p1,p2,p3,p4,p5,p6,p7,p8,p9,p10,p11,";
	n_fields += "p12,p13,p14,p15,p16,p17,p18,p19,p20,p21,p22,p23,p24,p25,p26,p27,";
	n_fields += "p28,p29,p30,p31,p32,p33,p34,p35,p36,p37,p38,p39,p40,p41,p42,p43,p44,p45,";
	n_fields += "p46,p47,p48,p49,p50,p51,p52,p53,p54,p55,p56,p57,p58,p59,p60,";
	n_fields += "p61,p62,p63,p64,p65,p66,p67,p68,p69,p70,p71,p72,p73,p74,p75,p76,p77,";
	n_fields += "p78,p79,p80,p81,p82,p83,p84,p85,p86,p87,p88,p89,p90,";
	n_fields += "p91,p92,p93,p94,p95,p96";

	//查询的时间段。例：未来一天的0点到23.59
	QDate dt = QDate::currentDate();
	dt = dt.addDays(1);
	QDateTime m_startTime = QDateTime(dt, QTime(0, 0, 0));
	QDateTime m_endTime = QDateTime(dt, QTime(23, 59, 59));
	double dStartTime = UIUtil::QDateTimeToCDateTime(m_startTime);
	double dEndTime = UIUtil::QDateTimeToCDateTime(m_endTime);

	//m_resultList用来存储查询结果的list
	int n_result;

	n_result = m_IHdbAccess->GetTimeSeriesById(n_tablename.toStdString(), n_fields.toStdString(), 1, dStartTime, dEndTime, m_resultList);
	m_pFriend->GetDailyLog()->WriteDailyLog(0, fileNo, LL_INFO, "Getpfstationshorthub------strTable:%s,strField:%s,Id:%d,startTime:%s,endTime:%s.", n_tablename.toStdString().c_str()
		, n_fields.toStdString().c_str(), id, m_startTime.toString("yyyy-MM-dd hh:mm:ss").toStdString().c_str()
		, m_endTime.toString("yyyy-MM-dd hh:mm:ss").toStdString().c_str());

	//m_resultList的用法
	//m_resultList.at(0).at(1).toInt() 代表获取的rdfid，即场站索引
	//m_resultList.at(0).at(2).toDouble() 代表获取的p1，即未来一天内的第一个点的预测功率值
	//m_resultList.at(0).at(3).toDouble() 代表获取的p2，即未来一天内的第二个点的预测功率值



}

void CStpstra::toDistableData(QString tableName, int planId, int daynum, QList<QVariantList> dvalList)
{
	
}



