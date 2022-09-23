/*
 * @Author: liuchunhui
 * @Date: 2022-03-25 17:47:48
 * @LastEditTime: 2022-03-30 11:56:05
 * @LastEditors: Please set LastEditors
 * @Description: 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 * @FilePath: /opt/GsafetyVideoEngineLibrary/AlarmManager.cpp
 */
#include "AlarmManager.h"

AlarmManager::AlarmManager(AIIndexDataMap& ai_index_dataM, TrustDataTableMap& trust_dataM)
    : m_AIIndexDataM(ai_index_dataM), m_TrustDataTableM(trust_dataM)
{
    
    m_pLogger = GsafetyLog::GetInstance();
    // onetime.Init();
    AlgorithmAlarmTypeOne[0] = "Alarm-yolov5"; //烟火
    AlgorithmAlarmTypeOne[1] = "Alarm-yolov5"; //安全帽
    AlgorithmAlarmTypeOne[2] = "Alarm-yolov5"; //值班人员脱岗
    AlgorithmAlarmTypeOne[9] = "Alarm-yolov5"; //动火作业
    AlgorithmAlarmTypeOne[10] = "Alarm-yolov5"; //反光衣识别
    AlgorithmAlarmTypeOne[7] = "Alarm"; //人流量统计
    AlgorithmAlarmTypeOne[11] = "Alarm-yolov5"; //皮带跑偏,纯cpu计算
    AlgorithmAlarmTypeOne[12] = "Alarm"; //打架
    AlgorithmAlarmTypeOne[13] = "Alarm"; //摔倒
    AlgorithmAlarmTypeOne[14] = "Alarm"; //抽烟
    AlgorithmAlarmTypeOne[3] = "NoAlarm"; //人脸识别
    AlgorithmAlarmTypeOne[4] = "NoAlarm"; //车辆识别
    AlgorithmAlarmTypeOne[5] = "NoAlarm"; //车辆属性
    AlgorithmAlarmTypeOne[6] = "NoAlarm"; //人群密度
    AlgorithmAlarmTypeOne[8] = "NoAlarm"; //人车跟踪，结构化
}

AlarmManager::~AlarmManager()
{
}

bool AlarmManager::init(int AlgorithmType, int zmqIndex_task, int zmqIndex_analysis, 
        bool use_private_library, shared_ptr<ZmqManager>& zmqObject)
{

    this->AlgorithmType = AlgorithmType;
    LOG4CPLUS_INFO("root", "AlarmManager Success! AlgorithmType: " << AlgorithmType);
    LOG4CPLUS_INFO("root", "AlgorithmAlarmType is: " << AlgorithmAlarmTypeOne.at(AlgorithmType));

    this->m_zmqIndex_analysis = zmqIndex_analysis;
    this->m_zmqIndex_task = zmqIndex_task;
    this->m_usePrivateLib = use_private_library;
    m_ZmqManager = zmqObject;
    
    LOG4CPLUS_INFO("root", "m_ZmqManager init Finish!");

    if (AlgorithmType == PERSION)
    {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        for (int i = 0; i < 64; i++)
            start_time[i] = tv.tv_sec;
    }

    return true;
};


void AlarmManager::setVectorThreshold(const vector <float> & threshold)
{
    this->thresholdVector = threshold;
};


bool AlarmManager::isAlarmHandler(vector<int>& inferIndexV, vector<InterArea> &interAreaVrector, 
        DetectioResV &inputputRes, const vector<int> &widthHight)
{
    this->widthHight = widthHight;

    //判断阈值vector是否有输入
    // if (thresholdVector.size()==0)
    //     haveThreshold = false;
    // else
    //     haveThreshold = true;

    //判断阈值vector类型、推理是否一致
    // if (haveThreshold & thresholdVector.size()!=inputputRes.size())
    // {
    //     throw "size of threshold is not consistent with size of videos!";
    // }

    //判断阈值交互、推理、宽高输入是否一致
    if ((interAreaVrector.size() == inputputRes.size()) && (interAreaVrector.size() == widthHight.size()/2))
    {
        //一致，进行告警分析
        if (AlgorithmAlarmTypeOne.at(AlgorithmType) == "NoAlarm")
        {
            //cout << "no alarm analysis!" << endl;
            _noalarmHandler(inferIndexV, interAreaVrector, inputputRes);
            return false;
        }
        else if (AlgorithmAlarmTypeOne.at(AlgorithmType) == "Alarm-yolov5")
        {
            //cout << "alarm analysis!" << endl;
            _alarmHandler(inferIndexV, interAreaVrector, inputputRes);
            return true;
        }
        else
        {
            //没有告警类型函数
            throw "No Alarm handler function!";
        }
    }
    else
    {
        //不一致进行报错
        throw "size of interAreaVrector is not equal to size of ppRes!";
    }

};

void AlarmManager::_noalarmHandler(vector<int>& inferIndexV, vector<InterArea> &interAreaVrector, DetectioResV &ppRes)
{
    if (!ppRes.size() || !ppRes.at(0).size()) return;

    InternalDataSet ids;
    std::string msg = "";

    auto siter = ppRes.at(0).begin();
    auto eiter = ppRes.at(0).end();
    for (;siter != eiter;)
    {
        /** @todo 人群密度告警判断和数据转发http告警分析模块 */
        if (CROWD_DENSITY == AlgorithmType)
        {
            /** @todo 发送结果到http告警分析模块 */
            ids.idot = I_CROWDDENSITY_VIDEO;
            ids.PersonnelNum = siter->count;
            //ids.mat = siter->image;
            
            //人群密度数据本地处理同时对外发送,如果需要可添加开关选项单独处理
            if (siter->count > m_AIIndexDataM[inferIndexV[0]].alarmPersonnels)
                siter->isAlarm = ALARM;
            siter++;
        }
        /** @todo 人脸识别告警判断和数据转发http告警分析模块 */
        else if (FACE == AlgorithmType)
        {
            ids.idot = I_FACERESULT_VIDEO;
            //该接口只使用私有人脸库进行比对,公共人脸库使用http服务承载
            if (m_usePrivateLib)
            {
                /** @todo 人脸特征匹配 */
                _FeatureCompare(m_AIIndexDataM[inferIndexV[0]], siter++);
                if (siter == eiter) return; else continue;
            }
            else
            {
                AlgorithmResult ar;
                ids.mat = siter->image;
                ar.feature = siter->feature;
                ar.Confidence = m_AIIndexDataM[inferIndexV[0]].threshold;
                ar.rect.x = siter->bbox[0]; ar.rect.y = siter->bbox[1];
                ar.rect.width = siter->bbox[2]; ar.rect.height = siter->bbox[3];
                ids.DetectBoxV.emplace_back(ar);
                siter++->isAlarm = NOALARM; //非告警
            }
        }
        /** @todo 车牌识别告警判断和数据转发http告警分析模块 */
        else if (VEHICLE == AlgorithmType || VEHICLE_TYPE == AlgorithmType)
        {
            ids.idot = I_VEHICLERESULT_VIDEO;
            //该接口只使用私有车辆库进行比对,公共车辆库使用http服务承载
            if (m_usePrivateLib)
            {
                /** @todo 车牌匹配 */
                _VehicleCompare(m_AIIndexDataM[inferIndexV[0]], siter++);
                if (siter == eiter) return; else continue;
            }
            else
            {
                AlgorithmResult ar;
                ids.mat = siter->image;
                ar.Confidence = siter->conf;
                ar.vehicleNum = siter->info;
                ar.trackerId = siter->trackerId;
                ar.rect.x = siter->bbox[0]; ar.rect.y = siter->bbox[1];
                ar.rect.width = siter->bbox[2]; ar.rect.height = siter->bbox[3];
                ids.DetectBoxV.emplace_back(ar);
                siter++->isAlarm = NOALARM; //非告警
            }
        }
    }
    ids.AlgorithmType = AlgorithmType;
    ids.AlarmInterval = m_AIIndexDataM[inferIndexV[0]].alarmInterval;
    ids.TrustFlag = m_AIIndexDataM[inferIndexV[0]].trustFlag;
    ids.CurrentId = m_AIIndexDataM[inferIndexV[0]].taskId;
    ids.CameraId = m_AIIndexDataM[inferIndexV[0]].decodeId;
    m_DataAnalysis->InternalDataSetSerialize(msg, ids);
    m_ZmqManager->SendMessage(m_zmqIndex_analysis, msg, msg.size());
}

void AlarmManager::_VehicleCompare(const IndexDataAI& ida, std::vector<DetectioRes>::iterator pDetectioRes)
{
    for (auto &node : m_TrustDataTableM)
    {
        if (ida.trustFlag != node.second.trust)
            continue;
        if (!pDetectioRes->info.compare(node.second.vehicle))
        {
            pDetectioRes->isAlarm = ALARM;
            pDetectioRes->info = node.second.name;
            if (!node.second.department.empty())
                pDetectioRes->info += (":" + node.second.department);
            break;
        }
    }
}

//可优化，与httpmanager合并
void AlarmManager::_FeatureCompare(const IndexDataAI& ida, std::vector<DetectioRes>::iterator pDetectioRes)
{
    float max_sim = ida.threshold;
    pDetectioRes->conf = ida.threshold; //盼盼需要的阈值数据;
    float sim = 0.0;
    int pos;
    for(auto &node : m_TrustDataTableM)
    {
        std::vector<FEATURE> face2;
        face2.push_back(node.second.feature);
        if (ida.trustFlag == node.second.trust)
            sim = m_spFaceSDK->compares(pDetectioRes->feature, face2, pos);
        else continue;

        if (sim > max_sim)
        {
            max_sim = sim;
            //pDetectioRes->conf = max_sim;
            pDetectioRes->isAlarm = ALARM;
            pDetectioRes->info = node.second.name;
            if (!node.second.department.empty())
                pDetectioRes->info += (":" + node.second.department);
        }
    }
}

void AlarmManager::_alarmHandler(vector<int>& inferIndexV, vector<InterArea> &interAreaVrector, DetectioResV &ppRes)
{
    InterArea region;
    string areaFlag;
    int width;
    int higth;
    DetectioRes result;
    
    for (int i = 0; i < ppRes.size(); i++)
    //单张图片
    {
        bool alarmFlag = false; //默认没有报警
        region = interAreaVrector.at(i);
        areaFlag = _regionFlag(region);
        width = widthHight[2*i];
        higth = widthHight[2*i+1];
    
        float threshold = region.thresholdArea;

        if (AlgorithmType == PERSION && 0 == ppRes.at(i).size())
        {
            struct timeval tv;
            gettimeofday(&tv, NULL);
            int interval = tv.tv_sec - start_time[inferIndexV[i]];
            uint64_t ltl = m_AIIndexDataM[inferIndexV[i]].leaveTimeLimit;
            if(!person_first[inferIndexV[i]] && interval > ltl)
            {
                DetectioRes tmp;
                tmp.isAlarm = true;
                ppRes.at(i).push_back(tmp);
                person_first[inferIndexV[i]] = true;
                continue;
            }
        }

        for (int j=0; j<ppRes.at(i).size();j++)
        //单个检测框
        {
            bool alarmConfidence = false;    //检测框是否告警
            bool alarmInInteresting = false; //检测框是否在感兴趣区域

            result = ppRes.at(i).at(j);
            alarmConfidence = _isAlarm(result.class_id, result.conf, threshold);

            vector<float> tmpbbox = {result.bbox[0], result.bbox[1], result.bbox[2], result.bbox[3]};

            alarmInInteresting = _isInInterestingArea(areaFlag, region.allPoints, tmpbbox, width, higth);

            if (alarmConfidence && alarmInInteresting) //在感兴趣区域内告警
            {
                if (AlgorithmType == PERSION)
                {
                    struct timeval tv;
                    gettimeofday(&tv, NULL);
                    ppRes.at(i).at(j).isAlarm = false;
                    start_time[inferIndexV[i]] = tv.tv_sec;
                    person_first[inferIndexV[i]] = false;
                }
                //如果告警，保留检测框，同时，将告警标志改为true
                alarmFlag = true;
            }
            else
            {
                //如果不告警，删除ppRes中的检测框
                ppRes.at(i).erase(ppRes.at(i).begin()+j);
                j--;
            }
        }
        //如果报警标识是false，说明图片没有告警
        // 删除ppres的内部vector
        if (alarmFlag == false)
        {
            ppRes.at(i).clear();
        }
    }
};

bool AlarmManager::_isInInterestingArea(string areaFlag, std::vector<InterPoint> &allPoints, vector<float> &tmpbbox, int width, int hight)
{
    if (areaFlag != "reversedInteresingArea" && areaFlag != "interesingArea")
    {
        return true;
    };

    //将bbox数据变为真实的点数据
    // vector<cv::Point>::iterator oneBoxPoint;
    vector<cv::Point> bbox; //代表矩形的四个点
    cv::Point leftup(tmpbbox[0]-tmpbbox[2]/2, tmpbbox[1]-tmpbbox[3]/2);
    bbox.push_back(leftup);
    
    cv::Point rightup(tmpbbox[0]+tmpbbox[2]/2, tmpbbox[1]-tmpbbox[3]/2);
    bbox.push_back(rightup);

    cv::Point leftdown(tmpbbox[0]-tmpbbox[2]/2, tmpbbox[1]+tmpbbox[3]/2);
    bbox.push_back(leftdown);

    cv::Point rightdown(tmpbbox[0]+tmpbbox[2]/2, tmpbbox[1]+tmpbbox[3]/2);
    bbox.push_back(rightdown);

    //将多边形数据变为真实点数据 
    // vector<InterPoint>::iterator priorPolygonPoint;
    vector<cv::Point> Polygon; //代表多边形的多个点
    cv::Point  PolygonPoint;


    for (auto priorPolygonPoint : allPoints)
    {
        PolygonPoint.x = priorPolygonPoint.horizontal * width;
        PolygonPoint.y = priorPolygonPoint.ordinate * hight;
        Polygon.push_back(PolygonPoint);
    }

    // 判断bbox的四个点有几个在多边形内
    bool isPointInPolygon;
    int pointsNumInArea = 0;
    for (auto oneBoxPoint : bbox)
    {
        isPointInPolygon = _PtInPolygon(oneBoxPoint, Polygon, Polygon.size());
        if (isPointInPolygon)
        {
            pointsNumInArea++;
        }
    }
   
    //根据bbox是否在感兴趣区域内判断是否告警
    if (areaFlag=="reversedInteresingArea" & pointsNumInArea==0)
    {
        return true;
    }
    else if (areaFlag == "interesingArea" & pointsNumInArea==4)
    {
        return true;
    }
    else if (pointsNumInArea>0 & pointsNumInArea<4)
    {
        return true;
    }
    
    return false;
};




//作用：判断点是否在多边形内
// p指目标点， ptPolygon指多边形的点集合， nCount指多边形的边数
bool AlarmManager::_PtInPolygon(cv::Point p, vector<cv::Point> &ptPolygon, int nCount)
{
    // 交点个数
    int nCross = 0;
    for (int i = 0; i < nCount; i++)
    {
        cv::Point p1 = ptPolygon[i];
        cv::Point p2 = ptPolygon[(i + 1) % nCount]; // 点P1与P2形成连线

        if (p1.y == p2.y)
            continue;
        if (p.y < min(p1.y, p2.y))
            continue;
        if (p.y >= max(p1.y, p2.y))
            continue;
        // 求交点的x坐标（由直线两点式方程转化而来）

        double x = (double)(p.y - p1.y) * (double)(p2.x - p1.x) / (double)(p2.y - p1.y) + p1.x;

        // 只统计p1p2与p向右射线的交点
        if (x > p.x)
        {
            nCross++;
        }
    }
    // 交点为偶数，点在多边形之外
    // 交点为奇数，点在多边形之内
    if ((nCross % 2) == 1)
    {
    	//cout << p.x << "," << p.y << "在区域内";
    	return true;
    }
    else
    {
    	//cout << p.x << "," << p.y << "在区域外";
    	return false;
    }
    
};

bool AlarmManager::_isAlarm(int class_id, float class_confidence, float threshold)
{
    //先判断阈值
    if (class_confidence>threshold)
    {
        //再判断分类类型
        switch (AlgorithmType)
        {
        case FIRE:
            // TODO: to implement the stragedy to trigger alarm
            if (class_id < 6)
                return true;
            return false;
        case HELMET:
            if (class_id == 2)
                return true;
            return false;
        case PERSION:
            if(class_id == 0)  //根据需求适当放开匹配类型数量
            //if (class_id == 0 || class_id == 1 || class_id == 2)
                return true;
            return false;
        case WELDING:
            if(class_id == 0 || class_id == 1)
            //if (class_id == 1)
                return true;
            return false;
        case REFLECTIVE_CLOSTHES:
            if(class_id == 4)
            //if (class_id == 1)
                return true;
            return false;
        case BLET_DEVIATION:
            if(class_id == 2)
            //if (class_id == 1)
                return true;
            return false;
        }
    }
    return false;
    
};

string AlarmManager::_regionFlag(InterArea &region)
{
    if (region.shutDownArea == true)
    {
        return "shutDownArea";
    }

    if (region.interestType == 1)
    {
        return "interesingArea";
    }
    else if (region.interestType == 2)
    {
        return "reversedInteresingArea";
    }
    else if (region.interestType == 3)
    {
        return "line";
    }
    else
    {
        return "else";
    }
}

void AlarmManager::sendOneAlarm(int decode_index, AlarmResult &oneAlgorithmData)
{
    Json::Value root;
    Json::StreamWriterBuilder writerBuilder;
    std::ostringstream os;

    /** @todo 算法结构化数据处理 */
    root["Type"] = ALARMA;
    root["TaskId"] = oneAlgorithmData.CurrentId;
    root["AlarmId"] = oneAlgorithmData.AlarmId;
    root["CameraId"] = oneAlgorithmData.CameraId;
    root["CameraName"] = oneAlgorithmData.CameraName;
    //root["TimeStamp"] = (uint64_t)SDK_Time();
    root["TimeStamp"] = oneAlgorithmData.TimeStamp;
    root["ImagePath"] = oneAlgorithmData.ImagePath;
    // root["Aresult"] = ""

    std::unique_ptr<Json::StreamWriter> jsonWriter(writerBuilder.newStreamWriter()); // json格式
    jsonWriter->write(root, &os);
    std::string msg = os.str();
    LOG4CPLUS_INFO("root", "send oneAlarmMassage,CameraId: " << oneAlgorithmData.CameraId);
    //std::cout << msg << std::endl;
    m_ZmqManager->SendMessage(decode_index, msg, msg.size(), false);
    m_ZmqManager->SendMessage(m_zmqIndex_task, msg, msg.size());
}
