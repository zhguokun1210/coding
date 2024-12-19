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
#include <QDebug>
#include <QSettings>
#include <QTextCodec>
#include <QDateTime>
#include <QSqlQuery>
#include "../../../E8000/inc/rtdb/e8000_rtdbaccess_scada.h"
#include "../../inc/sglc/base/idatetime.h"

#include "../../inc/sglc/icustomtool.h"           // for ICustomTool 外部工具
#include <QSqlQuery>
#include "../../inc/pnas/dao/icsstpldao.h"       
#include "../../inc/include/gui/lib/uiutil/uiutil.h"


extern ICsstplDao* g_pCsstplDaos;



CStpstra::CStpstra(ICustomTool* pTool) : m_pFriend(pTool)
{
	m_IHdbAccess = NULL;

	fileNo = 0;
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

	m_bHiscon = hisdbConnect();
	if (!m_bHiscon)
	{
		m_pFriend->GetDailyLog()->WriteDailyLog(0, fileNo, LL_ERROR, "connect his table failed.");
	}
	else
	{
		m_pFriend->GetDailyLog()->WriteDailyLog(0, fileNo, LL_ERROR, "connect his table suc.");

	}

	m_pHdbTrans = GetHdbtrans();
	if (m_pHdbTrans != nullptr)
	{
		m_pFriend->GetDailyLog()->WriteDailyLog(0, fileNo, LL_INFO, "hdbtrans suc");
	}
	else
	{
		m_pFriend->GetDailyLog()->WriteDailyLog(0, fileNo, LL_INFO, "hdbtrans fail");

	}
}




void CStpstra::StartSgStp(QTime curTime)//优化计划
{
	int curh = curTime.hour();
	int curmin = curTime.minute();
	m_pFriend->GetDailyLog()->WriteDailyLog(0, fileNo, LL_INFO, "进入计划制定模块,curh =%d,curmin=%d", curh, curmin);

	//配置信息获取
	//例：获取风电站计划参数plstation
	int StationPlanNum = g_pCsstplDaos->GetPlstationDao()->GetRecordCount();
	QList<int> WindStationstationidList;
	QList<double> WindStationcapList;
	m_pFriend->GetDailyLog()->WriteDailyLog(0, fileNo, LL_INFO, "StationPlanNum=%d", StationPlanNum);
	for (int i = 0; i < StationPlanNum; i++)
	{
		int gentype = g_pCsstplDaos->GetPlstationDao()->GetgentypeByPos(i);
		m_pFriend->GetDailyLog()->WriteDailyLog(0, fileNo, LL_INFO, "gentype=%d", gentype);
		if (gentype == 3) //发电类型为风电站
		{
			//配置信息：场站id，用于后续预测数据读取
			int stationid = g_pCsstplDaos->GetPlstationDao()->GetIdentifierByPos(i);
			WindStationstationidList.append(stationid);
			m_pFriend->GetDailyLog()->WriteDailyLog(0, fileNo, LL_INFO, "stationid=%d", stationid);

			//例 配置信息：装机容量
			//需要其他相关配置信息的话，按照g_pCsstplDaos->GetPlstationDao()->GetcapByPos(i)此类方式即可获取
			double cap = g_pCsstplDaos->GetPlstationDao()->GetcapByPos(i);
			WindStationcapList.append(cap);
		}
	}
	double cap = g_pCsstplDaos->GetPlstationDao()->GetcapByPos(0);
	m_pFriend->GetDailyLog()->WriteDailyLog(0, fileNo, LL_INFO, "装机容量=%f", cap);


	//预测数据获取
	//例: 获取新能源场站短期功率预测值pfstationshorthub
	QList<QVariantList> m_resultList;
	int selectedstationid = WindStationstationidList.at(0);
	m_pFriend->GetDailyLog()->WriteDailyLog(0, fileNo, LL_INFO, "WindStationstationidList.at(0)=%d", WindStationstationidList.at(0));
	//m_resultList中存储了查询的场站所对应的未来一日各时间节点的预测数据
	Getpfstationshorthub(selectedstationid,  m_resultList);

	//例：获取场站未来一日的第一个点的预测数据
	double ForecastData_1 = -1;
	if(m_resultList.size() >0)
		 ForecastData_1 = m_resultList.at(0).at(2).toDouble();

	m_pFriend->GetDailyLog()->WriteDailyLog(0, fileNo, LL_INFO, "场站id=%d,未来一日的第一个点的预测数据ForecastData_1=%f,m_resultList.size()=%d", selectedstationid,ForecastData_1, m_resultList.size());



	//自定义优化算法
	//基于上述获取到的场站预测数据以及需要的配置信息，进行自定义优化计划制定
	double PlanData_1 = 100;//优化算法修改后的场站未来一日的第一个点的计划值



	//存储优化计划  
	//插入新的初始优化计划记录
	//例：写入发电初始计划plstationinitial_2024，以计划参数表plstation第一条计划为例
	QString dtTabName = "plstationinitial_2024";
	//savetime对应最终生成计划的当日时间，即未来的日前计划时间。例：日前计划96点都存在一条记录里，以未来一日的第一个点作为savetime，即00:00
	QDateTime theDatetime = QDateTime(QDate::currentDate().addDays(1), QTime(0, 0, 0));
	//计划的id，名称name和索引irdfid，通过计划参数表plstation获取，以计划参数表plstation第一条计划为例。计划的名称name和索引irdfid也可通过其他偏好方法，自己记录在内存中，需要用时再获取
	QString sql = QString("insert into %1(id,savetime,createtime,updatetime,name,irdfid,p1) values(%2,'%3','%4','%4','%5','%6','%7')")
		.arg(dtTabName).arg(WindStationstationidList.at(0)).arg(theDatetime.toString("yyyy-MM-dd hh:mm:ss"))
		.arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")).arg(g_pCsstplDaos->GetPlstationDao()->GetnameByPos(0)).arg(g_pCsstplDaos->GetPlstationDao()->GetirdfidByPos(0)).arg(PlanData_1);
	QSqlQuery query(g_pHisDB);
	if (!query.exec(sql))
	{
		m_pFriend->GetDailyLog()->WriteDailyLog(0, fileNo, LL_ERROR, "write %s failed :\n %s.", dtTabName.toStdString().c_str(), sql.toStdString().c_str());
	}
	else
	{
		m_pFriend->GetDailyLog()->WriteDailyLog(0, fileNo, LL_INFO, "插入成功，场站id=%d，p1=%f", WindStationstationidList.at(0), PlanData_1);

	}


	//更新已有的初始优化计划记录
	sql = QString("update %1 set updatetime = '%2' ").arg(dtTabName)
		.arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
	sql += QString(",name='%1',irdfid='%2'").arg(g_pCsstplDaos->GetPlstationDao()->GetnameByPos(0)).arg(g_pCsstplDaos->GetPlstationDao()->GetirdfidByPos(0));
	sql += QString(",p1=%1").arg(PlanData_1+10);
	sql += QString(" where id=%1 and savetime='%2'").arg(WindStationstationidList.at(0)).arg(theDatetime.toString("yyyy-MM-dd hh:mm:ss"));
	QSqlQuery query2(g_pHisDB);
	if (!query2.exec(sql))
	{
		m_pFriend->GetDailyLog()->WriteDailyLog(0, fileNo, LL_ERROR, "update %s failed :\n %s.", dtTabName.toStdString().c_str(), sql.toStdString().c_str());
	}
	else
	{
		m_pFriend->GetDailyLog()->WriteDailyLog(0, fileNo, LL_INFO, "update成功，场站id=%d，p1=%f", WindStationstationidList.at(0), PlanData_1 + 10);

	}
}

void CStpstra::Getpfstationshorthub(int id, QList<QVariantList>& m_resultList)
{
	//新能源场站短期功率预测值历史表
	QString n_tablename = "pfstationshortpub";
	QString n_fields = "id,rdfid,p1,p2,p3,p4,p5,p6,p7,p8,p9,p10,p11,";
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

	n_result = m_IHdbAccess->GetTimeSeriesById(n_tablename.toStdString(), n_fields.toStdString(), id, dStartTime, dEndTime, m_resultList);
	m_pFriend->GetDailyLog()->WriteDailyLog(0, fileNo, LL_INFO, "Getpfstationshorthub------strTable:%s,strField:%s,Id:%d,startTime:%s,endTime:%s.", n_tablename.toStdString().c_str()
		, n_fields.toStdString().c_str(), id, m_startTime.toString("yyyy-MM-dd hh:mm:ss").toStdString().c_str()
		, m_endTime.toString("yyyy-MM-dd hh:mm:ss").toStdString().c_str());

	//m_resultList的用法
	//m_resultList.at(0).at(1).toInt() 代表获取的rdfid，即场站索引
	//m_resultList.at(0).at(2).toDouble() 代表获取的p1，即未来一天内的第一个点的预测功率值
	//m_resultList.at(0).at(3).toDouble() 代表获取的p2，即未来一天内的第二个点的预测功率值

	m_pFriend->GetDailyLog()->WriteDailyLog(0, fileNo, LL_INFO, "获取新能源场站短期功率预测值,n_result=%d", n_result);


}


bool CStpstra::getdatabasePara()
{
	QString fileName = "../config/real/e8000.ini";
	QSettings sets(fileName, QSettings::IniFormat);
	sets.setIniCodec(QTextCodec::codecForName("UTF-8"));

	strhisdbtype = sets.value("mdb.e8000his/dbtype").toString();
	hisport = sets.value("mdb.e8000his/port").toInt();
	strhisDrive = sets.value("mdb.e8000his/dbdriver").toString();
	if (strhisDrive == "QMYSQL")
		strhisDrive = "QMYSQL3";
	strhisHostname = sets.value("mdb.e8000his/hostname").toString();
	strhisdbName = sets.value("mdb.e8000his/database").toString();
	strhisPassword = sets.value("mdb.e8000his/password").toString();
	strhisUsername = sets.value("mdb.e8000his/username").toString();
	GetDailyLog()->WriteDailyLog(0, fileNo, LL_TRACE, "his database: %s-%s-%s-%s.", strhisDrive.toStdString().c_str(),
		strhisHostname.toStdString().c_str(), strhisdbName.toStdString().c_str(), strhisUsername.toStdString().c_str());


	return true;
}

bool CStpstra::hisdbConnect()
{
	if (!getdatabasePara()) return false;

	bool bRet = false;
	if (g_pHisDB.isOpen())
		g_pHisDB.close();

	GetDailyLog()->WriteDailyLog(0, fileNo, LL_TRACE, "hisdbConnect");
	g_pHisDB = QSqlDatabase::addDatabase(strhisDrive, "restphis");
	if (g_pHisDB.isValid())
	{
		g_pHisDB.setDatabaseName(strhisdbName);
		g_pHisDB.setUserName(strhisUsername);
		g_pHisDB.setPassword(strhisPassword);
		//g_pHisDB.setPort(hisport);
		if (strhisDrive.compare("QMYSQL3") == 0)
		{
			g_pHisDB.setHostName(strhisHostname);
		}
		bRet = g_pHisDB.open();
		printf("stpmodel open database------------------------\n");
		if (bRet)
		{
			if (strhisDrive.compare("QMYSQL3") == 0)
				g_pHisDB.exec("SET NAMES 'UTF8' ");
		}
		else
		{
			GetDailyLog()->WriteDailyLog(0, fileNo, LL_TRACE, "his database strhisDrive %s,strhisdbName %s,strhisUsername %s,strhisHostname %s." \
				, strhisDrive.toStdString().c_str(), strhisdbName.toStdString().c_str(), strhisUsername.toStdString().c_str(), strhisHostname.toStdString().c_str());
		}
	}
	else
	{
		GetDailyLog()->WriteDailyLog(0, fileNo, LL_TRACE, "his database is not valid: %s.", strhisDrive.toStdString().c_str());
	}
	return bRet;
}


