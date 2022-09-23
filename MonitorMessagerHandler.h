#ifndef MONITOR_HANDLER_H_
#define MONITOR_HANDLER_H_

#include "ZmqManager.hpp"
#include "GsafetyRdkafkaLibrary.hpp"
#include "MonitorParameters.h"
#include <json/json.h>
#include "ExternalDataSet.pb.h"
#include "GsafetyLogLibrary.hpp"

using namespace std;


class MonitorMessagerHandler
{
private:
    /* data */
    shared_ptr<ZmqManager> m_ZmqManager;
    uint32_t mainZmqIndex;
    uint32_t aiZmqIndex;
    uint32_t decZmqIndex;
    unique_ptr<GsafetyRdkafka> m_Rdkafka;
    KafkaMessage m_KafkaMessage;
    uint32_t sleep_time;
    MonitorParameter gsMonitor;
    uint32_t cameraNum;
    pair <string, DecodeParam> tempDecParam;
    pair <string, AIParam> tempAIParam;
    string ExceptionProbuf;
    string data_metrics;
    string delayMsg;
    GsafetyLog* m_pLogger;



    

    /**
     * @brief 将管道内的ai信息解析为结构数据
     * 
     * @param AIMsg 管道内一次传输的数据
     */
    bool _json2AiPair(string AIMsg);

    /**
     * @brief 将管道内的decode信息解析为结构数据
     * 
     * @param decMsg 管道内一次传输的数据
     */
    bool _json2DecPair(string decMsg);

    

public:
    MonitorMessagerHandler(/* args */);
    ~MonitorMessagerHandler();


    /**
     * @brief Set主线程通讯的index
     * @param index 消息管理类ADD方法返回值
     */
    void SetMainZmqIndex(uint32_t index);



    

    /**
     * @brief 初始化kafka及zmq的相关参数
     * 
     * @param contionerId 容器id
     * @param cameraNum video的数量,方便创建监控参数
     * @param zmqObject zmq线程引用
     * @param aiIndex zmq参数：aiindex
     * @param decIndex zmq参数：decindex
     * @param brokor kafka参数：brokor=host
     * @param topicName kafka参数，topic名称，默认参数"m_MonitorDelay"
     * @return true 创建成功
     * @return false 创建失败
     */
    bool InitResource(string contionerId, std::shared_ptr<ZmqManager>& zmqObject, uint32_t aiIndex, uint32_t decIndex, string brokor, string topicName="m_MonitorDelay");

    /**
     * @brief 循环处理，从zmq中循环拉取消息，判断性能是否延迟，推送性能监测到pushgateway，判断异常推送kafka。
     * 
     */
    void MessageProcessLoop(void);


    string createJson();


    
    /**
     * @brief 将delayMsg数据推送到kafka平台
     * 
     */
    void _pushException2Kafka();
    

};


#endif


