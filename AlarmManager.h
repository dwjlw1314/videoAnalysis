/*
 * @Author: liuchunhui
 * @Date: 2022-03-24 15:25:40
 * @LastEditTime: 2022-03-30 11:54:39
 * @LastEditors: Please set LastEditors
 * @Description: 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 * @FilePath: /opt/GsafetyVideoEngineLibrary/AlarmManager.h
 */


#ifndef ALARM_MANAGER_H_
#define ALARM_MANAGER_H_
#include "yololayer.h"
#include "DataAnalysis.hpp"
#include "InternalDataSet.h"
#include "GsafetyLogLibrary.hpp"
#include "SDKTime.h"
#include "ZmqManager.hpp"
#include "opencv2/opencv.hpp"
#include "gsFaceSDK.h"

#include <iostream>
#include <fstream>
#include <string.h>
#include <stdio.h>
#include <map>
#include <vector>

#define ALARM 1     //私有库告警标记
#define NOALARM 0   //私有库非告警标记

using namespace std;

typedef Yolo::Detection ADetectioRes;
using ADetectioResV = std::vector<std::vector<ADetectioRes>>;
typedef Global_Detect DetectioRes;
using DetectioResV = std::vector<std::vector<Global_Detect>>;

typedef std::map<int, std::string> AlgorithmAlarmType;

// AlgorithmAlarmType AlgorithmAlarmTypeOne;

class AlarmManager
{
private:
    std::shared_ptr<DataAnalysis> m_DataAnalysis;
    std::shared_ptr<gsFaceSDK> m_spFaceSDK;
    std::map<int, AlgorithmData> algorithemDataMap;
    int AlgorithmType;
    GsafetyLog* m_pLogger;
    AlarmResult oneAlarmResult;
    int m_zmqIndex_task;
    int m_zmqIndex_analysis;
    AIIndexDataMap& m_AIIndexDataM;
    TrustDataTableMap& m_TrustDataTableM;
    shared_ptr<ZmqManager> m_ZmqManager;
    vector<int> widthHight;
    AlgorithmAlarmType AlgorithmAlarmTypeOne;
    vector <float> thresholdVector;
    bool m_usePrivateLib;
    bool haveThreshold;
    uint64_t start_time[64] = {0};
    bool person_first[64] = {true};

    /**
     * @brief alarm-detection告警,生成告警的AlarmResultVector
     * 
     * @param algor_dataM 多视频通道的算法数据
     * @param inputputRes 算法infer输出的结果，需要对这个结果直接操作,指针类型
     */
    void _alarmHandler(vector<int>& inferIndexV, vector<InterArea> & interAreaVrector, DetectioResV& inputputRes);

    /** @brief dwj add */
    void _noalarmHandler(vector<int>& inferIndexV, vector<InterArea> &interAreaVrector, DetectioResV&inputputRes);

    /**
     * @brief 感兴趣区域形式
     * 
     * @param region 前端交互的结构体InterArea
     * @return shutDownArea 关闭 
     * @return interesingArea 感兴趣区域
     * @return reversedInteresingArea反选区域，
     * @return line伴线
     */
    string _regionFlag(InterArea & region);

    /**
     * @brief 通过阈值和告警类型判断是否告警
     * 
     * @param classid 检测类型
     * @param confidence 检测置信度
     * @return true 告警
     * @return false 不告警
     */
    bool _isAlarm(int classid, float confidence, float threshold);

    /**
     * @brief 判断检测框在感兴趣区域内告警
     * 
     * @param areaFlag 交互区域类型
     * @param allPoints 前端交互点
     * @param tmpbbox 检测框位置
     * @param width 图片宽
     * @param hight 图片高
     * 
     * @return true 检测框在感兴趣区域内
     * @return false 检测框不在感兴趣区域内
     */
    bool _isInInterestingArea(string areaFlag, std::vector<InterPoint> & allPoints, vector<float>& tmpbbox, int width, int hight);

    
    /**
     * @brief 判断点是否在区域内
     * 
     * @param p 目标点
     * @param ptPolygon 多边形区域
     * @param nCount 多边形边数
     * @return true 在区域内
     * @return false 在区域外
     * 
     */
    bool _PtInPolygon(cv::Point p, vector<cv::Point>& ptPolygon, int nCount);

    /**
     * @brief 人脸特征比对处理
     * 
     * @param ida 单路Ai算法参数
     * @param pDetectioRes 分析结果指定数据指针
     */
    void _FeatureCompare(const IndexDataAI& ida, std::vector<DetectioRes>::iterator pDetectioRes);
    
    /**
     * @brief 车牌号比对处理
     * 
     * @param ida 单路Ai算法参数
     * @param pDetectioRes 分析结果指定数据指针
     */
    void _VehicleCompare(const IndexDataAI& ida, std::vector<DetectioRes>::iterator pDetectioRes);

public:
    
    /**
     * @param ai_index_dataM 摄像头相关索引信息
     * @param trust_dataM 信任数据集合
     */
    AlarmManager(AIIndexDataMap& ai_index_dataM, TrustDataTableMap& trust_dataM);
    ~AlarmManager();

    /**
     * @brief 初始化
     * 
     * @param AlgorithmType 算法类型
     * @param m_zmqIndex_task zmq的index
     * @param zmqIndex_analysis http告警分析索引
     * @param use_private_library 是否使用本地私有特征库
     * @param zmqObject zmq线程
     * @return true 初始化成功
     * @return false 初始化失败
     */
    bool init(int AlgorithmType, int m_zmqIndex_task, int zmqIndex_analysis,
        bool use_private_library, shared_ptr<ZmqManager>& zmqObject);


    /**
     * @brief Set the Vector Threshold object
     * 
     * @param threshold 单视频流个性化设置阈值，注意必须是vector形式
     */
    void setVectorThreshold(const vector <float> & threshold);
  
    
    /**
     * @brief 判断是否进行告警分析，(告警通过阈值，交互区域的逻辑进行)，告警直接对分析结果进行操作
     * 
     * @param inferIndexV 多通道交互索引
     * @param interAreaVrector 多通道交互的vector
     * @param inputputRes 算法infer输出的结果，需要对这个结果直接操作,指针类型
     * @param widthHight 解析图片的宽高值
     * @return true 进行告警分析
     * @return false 不进行告警分析
     */
    bool isAlarmHandler(vector<int>& inferIndexV, vector<InterArea> & interAreaVrector,
            DetectioResV & inputputRes, const vector<int>& widthHight);
    //不同告警算法采用重载的方式，告警类型

    /**
     * @brief 发送一路算法告警信息到zmq
     * 
     * @param decode_index 解码对应索引
     * @param oneAlgorithmData 算法数据
     */
    void sendOneAlarm(int decode_index, AlarmResult & oneAlarmResult);

    /** @brief 添加人脸算法智能指针对象 */
    void addFaceSDKPointer(const std::shared_ptr<gsFaceSDK>& FaceObject)
    {
        m_spFaceSDK = FaceObject;
    }
};



#endif