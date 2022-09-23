/**
Copyright (C) 2021 - 2022, Beijing Chen An Co.Ltd.All rights reserved.
File Name: HttpServerManager.hpp
Description: http服务,接收图片数据进行分析，数据推送，告警
Change History:
Date         Version        Changed By      Changes
2022/06/17   1.0.0.0           dwj          Create
*/

#ifndef HTTPSERVERMANAGER_H
#define HTTPSERVERMANAGER_H

#include <Eigen/Dense>
#include <opencv2/freetype.hpp>

#include "HttpServer.hpp"
#include "ZmqManager.hpp"
#include "InternalDataSet.h"
#include "ExternalDataSet.pb.h"
#include "DataAnalysis.hpp"
#include "HttpPlatFormData.h"
#include "FaceDetectAndFeature.h"

#include<memory>
using namespace std;

#include "opencv2/opencv.hpp"
using namespace cv;

using Eigen::MatrixXd;

typedef Eigen::Matrix<float, 1, _FEATURESIZE, Eigen::RowMajor> R_FEATURE;

class HttpServerManager final
{
public: 
    HttpServerManager(const std::string HttpHost);
    ~HttpServerManager();

    /**
     * @brief httpServer类对象运行主方法(线程)
     * @param bd 基础数据集
     * @param zmq_mgmt zmq对象
     * @param zermq_index_data 索引数据集
    */
    void initResource(const BaseData& bd, std::shared_ptr<ZmqManager> zmq_mgmt, 
        ZeromqIndexDataMap& zermq_index_data);

    /**
     * @brief 分析类算法结果处理主方法(线程)
     * @param bd 基础数据集
     * @param zmq_mgmt zmq对象
     * @param zermq_index_data 索引数据集
     * @param trust_dataM 私有库名单集合
    */
    void alarmAnalysis(const BaseData& bd, std::shared_ptr<ZmqManager> zmq_mgmt, 
        ZeromqIndexDataMap& zermq_index_data, const TrustDataTableMap& trust_dataM);

private:

    /** register handle */
    void registerHandlerProc();

    /** 人脸数据集合处理 */
    bool handlerFaceAll(mg_connection *mgConnection, http_message *message);
	bool handlerFaceFeatureCompare(mg_connection *mgConnection, http_message *message);
	bool handlerSetFaceFeature(mg_connection *mgConnection, http_message *message);
	bool handlerClearData(mg_connection *mgConnection, http_message *message);
    
    /** 相似度计算接口*/
    float getSimilarity(const R_FEATURE& face1, const R_FEATURE& face2) = delete;
    float compares(const FEATUREM& f1, const FEATUREM& f2);

private:
    /**
     * @brief 解析客户端发送过来的人脸图片数据信息
     * 
     * @param message 
     */
    bool parseFaceAllData(mg_connection *mgConnection, http_message *message, HttpPlatFormData &httpPlatFormData);
    /**
     * @brief 图片分析
     * 
     * @param mgConnection http 连接 
     * @param image 图片
     * @return true 成功
     * @return false 失败
     */
    bool processFaceImage(mg_connection *mgConnection, InternalDataSet &internalDataSet);

    /**
     * @brief 发送信息到客户端
     * 
     * @param mgConnection http连接
     * @param message 发送的消息
     */
    void sendMessageToClient(mg_connection *mgConnection, HttpPlatFormData &httpPlatFormData);

    /**
     * @brief 组装内部数据集
     * 
     * @param internalDataSet 内部数据结构体
     * @param message 组装的之后的数据
     */
    void assembleInternalDataset(InternalDataSet &internalDataSet, HttpPlatFormData &httpPlatformData);

    /**
     * @brief 发送数据给数据分析线程
     * 
     * @param internalDataSet 
     */
    void sendToAIDataAnalysis(int zmqIndex, InternalDataSet &internalDataSet);

    /**
     * @brief 获取头域数据
     * 
     * @param message 
     * @param name 头域名称
     */
    string getHttpHeaderNameValue(http_message *message, string &name);
	
    void processResult(const BaseData &bd, InternalDataSet& internal_dataS, const TrustDataTableMap& trust_dataM);
    void alarmPush(std::shared_ptr<ZmqManager> zmq_mgmt, InternalDataSet& internal_dataS, int camera_index);
    bool savePicture(std::string& path, const cv::Mat& frame);
    /** @todo 车辆数据集合处理 */
    
private:
    bool init;//是否初始化成功
    int m_zmqIndex_task;
    int m_zmqIndex_analysis;
    BaseData baseData;
    shared_ptr<CFaceDetectAndFeature> faceDetectAndFeature;
    std::shared_ptr<HttpServer> m_HttpServer;
    std::shared_ptr<ZmqManager> m_ZmqManager;
};

#endif /* HTTPSERVERMANAGER_H */