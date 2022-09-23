/**
Copyright (C) 2021 - 2022, Beijing Chen An Co.Ltd.All rights reserved.
File Name: TaskManager.hpp
Description: Main task management class
root@gsafety:~# export GSAFETY_MONGODB=mongodb://root:root@10.3.9.107:27017
root@gsafety:~# export GSAFETY_ALGORITH=fire
Change History:
Date         Version        Changed By      Changes
2022/01/05   1.0.0.0           dwj          Create
*/

#ifndef TASKMANAGER_H
#define TASKMANAGER_H

#include "MongoDB.h"
#include "DataAnalysis.hpp"
#include "MessageManager.hpp"
#include "HttpServerManager.hpp"
#include "ZmqManager.hpp"
#include "ThreadPool.h"
#include "CodecManager.h"
#include "GsAIModelProcesser.h"
#include "MonitorMessagerHandler.h"

#include <json/json.h>
#include <openssl/md5.h>

#ifdef ENABLERZ
//dwj test head
#include "VideoAlgorithm.hpp"
#endif

#include <map>
#include <string>
#include <memory>
using namespace std;

using AlgorithmObjMap = std::map<std::string, std::shared_ptr<GsAIModelProcesser>>;
using CodecObjMap = map<string, shared_ptr<CodecManager>>;

class TaskManager final
{
public:
    TaskManager();
    ~TaskManager();

    /** @brief 启动视频分析任务 */
    void StartTaskProcessing(void);

    /** @brief 获取对象启动状态 */
    bool GetInitStatus() { return m_Status; }
     
private:

    /** @brief zeromq通信端子初始化 */
    void InitZeromqPairs(void);

    /** @brief 初始化消息管理 */
    bool InitMessageManager(void);

    /** @brief 初始化性能管理 */
    bool InitPerformanceManager(void);

    /** @brief 初始化分析类管理，eg：face */
    bool InitHttpServerManager(void);

    /** @brief 创建自定义日志模式*/
    void CreateLoggerMode() = delete;

    /** @brief 创建编解码管理器 */
    bool CreateCodecManager(void);

    /** @brief 创建算法管理器 */
    bool CreateAlgorithManager(void);

    /** 
    * @brief 添加编解码资源
    * @param camera_data 单个摄像头基本数据对象
    * @return 线程资源不足，返回失败
    */
    bool AddCodecResource(CameraData& camera_data);

    /** 
    * @brief 修改编解码资源
    * @param camera_data 单个摄像头基本数据对象
    */
    bool ModifyCodecResource(CameraData& camera_data);

    /** 
    * @brief 删除编解码资源
    * @param camera_data 单个摄像头基本数据对象
    */
    bool DeleteCodecResource(CameraData& camera_data);

    /** 
    * @brief 添加算法资源
    * @param camera_data 算法基本数据对象
    */
    bool AddAlgorithResource(AlgorithmData& algorith_data);

    /** 
    * @brief 修改算法资源
    * @param camera_data 算法基本数据对象
    */
    bool ModifyAlgorithResource(AlgorithmData& algorith_data);

    /** 
    * @brief 删除算法资源
    * @param camera_data 算法基本数据对象
    */
    bool DeleteAlgorithResource(AlgorithmData& algorith_data);

    /** 
    * @brief 内部数据综合处理方法
    * @param msg 序列化消息实体
    */
    void InDataIntegratedProcess(const std::string& msg);

    /** 
    * @brief 外部数据综合处理方法
    * @param msg 序列化消息实体
    */
    void OutDataIntegratedProcess(const std::string& msg);

    /** 
    * @brief 发送数据综合处理结果
    * @param inst_data 反序列化消息实体
    * @param status 指令处理状态枚举信息
    */
    void SendIntegratedProcessResult(InstructionData& inst_data, DATASTATE status);

    /** 
    * @brief 算法调度,使用摄像头ID进行算法操作
    * @param amap 平台算法数据集合
    * @param mode 算法1:1 or 1:n 模式选择
    */
    void AlgorithmDispatch(AlgorithmDataMap& amap, bool mode);

    /** 
    * @brief 摄像头调度
    * @param cmap 平台摄像头数据集合
    */
    void CameraDispatch(CameraDataMap& cmap);

    /** 
    * @brief 获取zeromq通信端子索引
    * @param id zeromq通信端子创建之初的绑定名称
    * @return zeromq通信端子索引，未找到返回-1
    */
    int FindZeromqIndex(const std::string& id);

    /** 
    * @brief 新增zeromq索引至m_ZeroMQIndexM
    * @param id 双向通信管道唯一标识ID
    * @param useGpu 是否创建GPUMat
    */
    void AddZeromqIndexToMap(const std::string& id, bool useGpu = true);

    /** @brief 清空数据集合 */
    void ClearDataMap(void);

    /** @brief 更新1:N模式算法ID */
    void UpdateAlgorithmID(const std::string& aid);

    /** @brief 更新本地信任名单库(m_TrustDataTableM) 
     * @param internal_data http管理类传输数据结构
     * @param flag 是否写入数据库,write=true;
    */
    void updataTrustListData(const InternalDataSet& internal_data, bool flag);

    /** 
    * @brief 更新容器表相关数据
    * @param metadata 元对象指针,已弃用
    * @param id 摄像头或者算法id
    * @param dtype 指定处理模式
    * @param mtype 指令操作归属分类
    */
    void UpdateContainerTableData(const std::string& id, DOT dtype, METADATATYPE mtype);
    
    /** 
    * @brief 创建过滤json字符串
    * @param filter 过滤字符串,参数方式为inout
    * @param id 指定摄像头唯一标识
    */
    void CreateFilterStr(std::string& filter, const std::string& id);

    /** 
    * @brief 创建摄像头更新json字符串
    * @param filter 更新字符串,参数方式为inout
    * @param id 指定摄像头唯一标识
    * @param type 指令操作类型
    */
    void CreateCameraUpdateStr(std::string& update, const std::string& id, DOT type);
    void CreateAlgorithUpdateStr(std::string& update, const std::string& id, DOT type);

    /** 
    * @brief 创建过滤json字符串
    * @param filter 过滤字符串,参数方式为inout
    * @param id 指定告警唯一标识
    */
    void CreateAlarmFilterStr(std::string& filter, const std::string& id);
    
    /** 
    * @brief 创建告警更新json字符串
    * @param filter 更新字符串,参数方式为inout
    * @param alarm_result 告警视频数据集
    */
    void CreateAlarmUpdateStr(std::string& update, const AlarmResult& alarm_result);

    /** 
    * @brief 更新容器表相关数据
    * @param inst_data 解析后的平台数据
    */
    void AddContainerTableData(const InstructionData& inst_data);

    /** @brief 发送算法程序启动状态到前端平台 */
    bool SendRunStateToPlatform(volatile bool flag = SUCCESS);

    /** @brief 发送算法告警信息到前端平台 */
    bool SendAlgorithmAlarmResult(const InstructionData& inst_data);

    /** @brief 获取指定字符串md5值 */
    std::string FromStringGetMd5(const std::string& value);

    /** @brief 读取程序启动基础数据,下一版本移动到数据分析类 */
    bool FromMongoDBGetBaseData(const std::string& md5);

    /** @brief 读取摄像头基础数据,保留cid,下一版本移动到数据分析类 */
    void FromMongoDBGetContainerData(const std::string& cid);

    /** @brief 读取分析类黑名单数据, 适配于addHttpServerManager分支 */
    bool FromMongoDBGetTrustData(int32_t AlgorithmType);

    /** @brief 获取可用摄像头线程使用数量 */
    int getAvailableCamera();

    /**
     * @brief 计算主程序启动任务所需线程数
     * @param amap 平台算法数据集合
     * @param cmap 平台摄像头数据集合
     */
    uint32_t GetThreadNums(const AlgorithmDataMap& amap, const CameraDataMap& cmap);

private:
    bool m_Status;
    bool m_Mode; //1:1=true
    bool m_InitMode;
    //数据库地址
    std::string m_uri;
    std::string m_HostName;
    //std::string m_AlgorithId;
    std::string m_AlgorithMd5;

    BaseData m_BaseData;
    CameraDataMap m_CameraDataM;
    AlgorithmDataMap m_AlgorithmDataM;
    AlgorithmObjMap m_AlgorithmObjectM;
    CodecObjMap codecObjMap;
    ZeromqIndexDataMap m_ZeroMQIndexM;
    TrustDataTableMap m_TrustDataTableM;

    GsafetyLog* m_pLogger;

    std::shared_ptr<MongoDB> m_MongoDb;
    std::shared_ptr<ZmqManager> m_ZmqManager;
    std::shared_ptr<DataAnalysis> m_DataAnalysis;
    std::shared_ptr<MessageManager> m_MessageManager;
    std::shared_ptr<HttpServerManager> m_HttpServerManager;
    std::shared_ptr<MonitorMessagerHandler> m_MonitorManager;

    //std::shared_ptr<CodecManager> m_CodecManager;
#ifdef ENABLERZ
    /** @test CreateCodecManager() 使用 ReadFrameFFmpeg*/
    std::shared_ptr<VideoAlgorithm> m_VideoAlgorithm;
#endif

    std::shared_ptr<ThreadPool> m_ThreadPool;
};

#endif  /* TASKMANAGER_H */