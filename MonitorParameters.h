/*
Copyright (C) 2022, Beijing Chen An Co.Ltd.All rights reserved.
File Name: ProcessParam.h
Description: ��������Э��
Change History:
Date         Version        Changed By      Changes
2022/01/19   2.0.0.1        liuchunhui       Create
*/

#ifndef MONITOR_PARAMETER_H_
#define MONITOR_PARAMETER_H_
#include <iostream>
#include <fstream>
#include <string.h>
#include <stdio.h>
#include <map>
#include <vector>
#include "../GsafetyHttpClient/HttpClient.h"
#include "ExternalDataSet.pb.h"

using namespace std;
using namespace VideoEngine2; //probuf的命名空间，他如何定义的命名空间？？？？

typedef struct DecodeParam
{
    string decodeId;
    int videoFPS;
    double decodeFPS;
    uint32_t SkipFrameFrequency;
} DecodeParam;

typedef struct AIParam
{
    string algorithmName;
    int algorithmFPS;
    int defaultFPS;
} AIParam;

typedef map<string, DecodeParam> DecodeParamMap; //<cameraId, DecodeParam>
typedef map<string, AIParam> AIParamMap;         //<algotithmId, AIParam>

class MonitorParameter
{
private:
    /* data */
    int _decode_frame_all; //同一时间的解码总和
    string contionerId; //容器ID
    int timeInterval;   //推送间隔
    int algorithmNum;

    /**
     * @brief 检查算法参数延迟情况
     *
     * @return 如果返回1代表延迟，返回0代表不延迟
     */
    bool _checkAlgDelay(int algfps);

    /**
     * @brief 检查解码延迟情况
     *
     * @return 如果返回1代表延迟，如果返回0代表不延迟
     */
    bool _checkDecDelay(int videoFPS, int decodeFPS);

public:
   
    // string algorithm;   //算法类型
    HttpClient GsMonitorClient;
    DecodeParamMap GsDecodeParam;
    AIParamMap GsAIParam;
    string data_metrics; // pushgateway推送形式
    bool isDelay;

    MonitorParameter();
    ~MonitorParameter();


    void setTimeInterval(int elseTimeInterval);

    /**
     * @brief 初始化必要参数
     *
     * @param timeInterval 推送时间间隔
     * @param decodeNum decode线程数
     * @param algorithmNum ai线程数
     */
    void init(string contionerId, int timeInterval, int algorithmNum = 1);

    /**
     * @brief 监测插入参数是否数量是否一致
     *
     */
    int checkParamConsistent(); //???

    /**
     * @brief 检查线程延迟情况，并生成data_metric
     *
     * @return 如果有延迟返回1，没有延迟返回0
     */
    string checkDelay();

    void pushPrameter2Gateway();

    // void pushException();

    /**
     * @brief 插入decode线程的map值
     *
     * @param oneDecodeParam
     */
    void insertDecodeParam(pair<string, DecodeParam> & oneDecodeParamPair);

    /**
     * @brief 插入ai算法线程的map值
     *
     * @param oneAlgorithmParam
     */
    void insertAlgorithmParam(pair<string, AIParam> & oneAIParamPair);
};

#endif