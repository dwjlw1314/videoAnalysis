/**
Copyright (C) 2021 - 2022, Beijing Chen An Co.Ltd.All rights reserved.
File Name: MessageManager.hpp
Description: message dispatch management class
Change History:
Date         Version        Changed By      Changes
2022/01/05   1.0.0.0           dwj          Create
*/

#ifndef MESSAGEMANAGER_H
#define MESSAGEMANAGER_H

#include "GsafetyRdkafkaLibrary.hpp"
#include "ExternalDataSet.pb.h"
#include "InternalDataSet.h"
#include "ZmqManager.hpp"

//thread process function
using MessageManagerHandler = std::function<void(void)>;

class MessageManager final
{
public:
    /* 线程安全类 */
    MessageManager();
    ~MessageManager();

    /**
     * @brief Set the Zmq Index object
     * @param index 消息管理类ADD方法返回值
     */
    void SetZmqIndex(uint32_t index);

    /**
     * @brief Set and Get the Group Id object
     * @param gid 订阅分组号
     */
    void SetGroupId(const std::string& gid);
    const std::string& GetGroupId() { return m_GroupId; }

    /**
     * @brief Set the Rdkafka Host object
     * @param host 消息中间件服务主机 host:port
     */
    void SetRdkafkaHost(const std::string& host);

    /**
     * @brief Set the Sub Topic Name object
     * @param name 订阅模式消息队列名称
     */
    void SetSubTopicName(const std::string& name);

    /**
     * @brief Set the Pub Topic Name object 
     * @param status 指令状态消息队列名称
     * @param alarm 算法告警队列名称
     * @param analysis 性能分析队列名称
     */
    void SetPubTopicName(const std::string& status,
        const std::string& alarm, const std::string& analysis);

    /**
     * @brief 初始化消息传输和消息中转资源
     * @param zmqObject 消息传输上下文
     * @return 成功返回 true
     */
    bool InitResource(std::shared_ptr<ZmqManager>& zmqObject);

    /** @brief 循环收发平台和模块数据 */
    void MessageProcessLoop(void);

    /**
     * @brief 发送消息至 kafka gsafety_status topic
     * @param msg 消息传输上下文
     * @param len 发送的消息大小
     * @return 成功返回 true
     */
    bool SendStatusToPlatform(const std::string& msg, std::size_t len);

    /**
     * @brief 发送消息至 kafka gsafety_alarm topic
     * @param msg 消息传输上下文
     * @param len 发送的消息大小
     * @return 成功返回 true
     */
    bool SendAlarmInfoToPlatform(const std::string& msg, std::size_t len);

    /**
     * @brief 发送信任名单数据到平级POD
     * @param msg 消息传输上下文
     * @param len 发送的消息大小
     * @return 成功返回 true
     */
    bool SendTrustListToPlatform(const std::string& msg, std::size_t len);

    //模拟收发平台数据函数，非正式函数
    std::string SendMsgToPlatform();
    void RecvMsgFromPlatform(std::string msg, uint32_t index);

private:
    uint32_t m_ZmqIndex;
    std::string m_Broker;
    std::string m_GroupId;
    std::vector<std::string> m_SubTopicNameV;
    std::string m_StatusPubTopic;
    std::string m_AnalysisPubTopic;
    std::string m_AlarmPubTopic;
    KafkaMessage m_KafkaMessage;
    GsafetyLog* m_pLogger;
    std::unique_ptr<GsafetyRdkafka> m_Rdkafka;
    std::shared_ptr<ZmqManager> m_ZmqManager;
};

#endif  /* MESSAGEMANAGER_H */