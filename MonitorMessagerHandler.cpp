#include "MonitorMessagerHandler.h"
MonitorMessagerHandler::MonitorMessagerHandler(/* args */)
{
    m_pLogger = GsafetyLog::GetInstance();
    sleep_time = 10;

}

MonitorMessagerHandler::~MonitorMessagerHandler()
{
}


//私有函数
// =================================================================================

bool MonitorMessagerHandler::_json2DecPair(string decMsg)
{
    if (decMsg.empty())
    {
        std::cout << "decMsg.empty!" << std::endl;
        return false;
    }

    bool res;
    JSONCPP_STRING errs;
    Json::Value root;
    Json::CharReaderBuilder readerBuilder;

    std::unique_ptr<Json::CharReader> const jsonReader(readerBuilder.newCharReader());

    res = jsonReader->parse(decMsg.c_str(), decMsg.c_str() + decMsg.length(), &root, &errs);
    if (res == 0)
        std::cout << "RES = 0" << std::endl;
    if (!res || !errs.empty())
    {
        std::cout << "parseJson err. " << errs << std::endl;
    }

    // std::cout << "cameraId: " << root["cameraId"].asString() << std::endl;
    // std::cout << "decodeId: " << root["decodeId"].asString() << std::endl;

    // std::cout << "videoFPS: " << root["videoFPS"].asInt() << std::endl;
    // std::cout << "decodeFPS: " << root["decodeFPS"].asDouble() << std::endl;
    // std::cout << "SkipFrameFrequency: " << root["SkipFrameFrequency"].asInt() << std::endl;

    if (root["cameraId"].isNull())
    {
        cout << "the Value of cameraId is empty!" << endl;
        return false;
    }

    tempDecParam.first = root["cameraId"].asString();
    tempDecParam.second.decodeId = root["decodeId"].asString();
    tempDecParam.second.decodeFPS = root["decodeFPS"].asDouble();
    tempDecParam.second.videoFPS = root["videoFPS"].asInt();
    tempDecParam.second.SkipFrameFrequency = root["SkipFrameFrequency"].asUInt();
    return true;
};

bool MonitorMessagerHandler::_json2AiPair(string AIMsg)
{
    if (AIMsg.empty())
    {
        std::cout << "AIMsg.empty!" << std::endl;
        return false;
    }

    bool res;
    JSONCPP_STRING errs;
    Json::Value root;
    Json::CharReaderBuilder readerBuilder;

    std::unique_ptr<Json::CharReader> const jsonReader(readerBuilder.newCharReader());

    res = jsonReader->parse(AIMsg.c_str(), AIMsg.c_str() + AIMsg.length(), &root, &errs);
    if (res == 0)
        std::cout << "RES = 0" << std::endl;
    if (!res || !errs.empty())
    {
        std::cout << "parseJson err. " << errs << std::endl;
    }

    // std::cout << "algorithmId: " << root["algorithmId"].asString() << std::endl;
    // std::cout << "algorithmName: " << root["algorithmName"].asString() << std::endl;

    // std::cout << "algorithmFPS: " << root["algorithmFPS"].asInt() << std::endl;
    // std::cout << "defaultFPS: " << root["defaultFPS"].asInt() << std::endl;

    if (root["algorithmId"].isNull())
    {
        cout << "the Value of algorithmId is empty!" << endl;
        return false;
    }

    tempAIParam.first = root["algorithmName"].asString();
    tempAIParam.second.algorithmName = root["algorithmName"].asString();
    tempAIParam.second.algorithmFPS = root["algorithmFPS"].asInt();
    tempAIParam.second.defaultFPS = root["defaultFPS"].asInt();
    return true;
};




void MonitorMessagerHandler::_pushException2Kafka()
{
    m_KafkaMessage.payload = delayMsg;
    m_KafkaMessage.len = delayMsg.length();
    m_KafkaMessage.partition = -1;

    //判断异常，推送到kafka
    if (m_Rdkafka->SendMessage(m_KafkaMessage)){
        cout << "发送kafka正常" << endl;
        LOG4CPLUS_INFO("root", "Message delivery KAFKA Success!");
    }           
    else{
        cout << "发送kafka失败" << endl;
        LOG4CPLUS_INFO("root", "Message delivery KAFKA Fail!");
    }
        
};







//共有函数
// =================================================================================


void MonitorMessagerHandler::SetMainZmqIndex(uint32_t index)
{
    mainZmqIndex = index;
}

bool MonitorMessagerHandler::InitResource(string contionerId, std::shared_ptr<ZmqManager> &zmqObject, uint32_t aiIndex, uint32_t decIndex, string brokor, string topicName)
{
    //获取zmq
    
    m_ZmqManager = zmqObject;
    aiZmqIndex = aiIndex;
    decZmqIndex = decIndex;
    LOG4CPLUS_INFO("root", "m_ZmqManager init Finish!");

    gsMonitor.init(contionerId, sleep_time);
    LOG4CPLUS_INFO("root", "gsMonitor init Finish!");

    m_KafkaMessage.kafkatype = PRODUCER;
    m_KafkaMessage.broker = brokor;
    m_KafkaMessage.topic_name = topicName;
    // kafka消息内容

    GsafetyRdkafka *pobj = new GsafetyRdkafka(m_KafkaMessage); ////后续修改？？？？？？？？？？？？？？？？？
    m_Rdkafka.reset(pobj);

    LOG4CPLUS_INFO("root", "kafka init Finish!");
    return m_Rdkafka->GetInitStatus();
}

void MonitorMessagerHandler::MessageProcessLoop(void)
{
    uint64_t index = 0;
    string aiZmqMsg, decZmqMsg, decname,ainame;
    size_t aiMsgLen, decMsgLen;
    bool flag, add_data;


    index |= (uint64_t)0x0000000000000001 << aiZmqIndex; // 0
    index |= (uint64_t)0x0000000000000001 << decZmqIndex; //1

    while (1)
    {
        //cout << "===============out one while" << endl;

        while (1)
        {
            //可能同时时间接受多组数据，嵌套循环
            //cout << "===============in one while" << endl;

            int indexBit = m_ZmqManager->MessagePool(index); //指的是有信息的位

            if (indexBit == 0){
                break;
            }

            add_data = true; //表示本次循环中是否有数据 
            for (int i = 0; i < m_ZmqManager->GetIndexNums(); i++)
            {
                
                if ((indexBit & 0x01) && (i == aiZmqIndex)) // aizmqindex
                {
                    m_ZmqManager->ReceiveMessage(aiZmqIndex, aiZmqMsg, aiMsgLen);//如果没有消息会阻塞？？？？、
                    ainame = m_ZmqManager->GetName(aiZmqIndex);
                    //cout << ainame << endl;

                    //解析参数，生成想要的格式
                    _json2AiPair(aiZmqMsg);
                    gsMonitor.insertAlgorithmParam(tempAIParam);

                    //插入到监控参数类中
                    //cout << aiZmqMsg << endl;
                    //cout << "received aiZmqMsg" << endl;
                }

                else if ((indexBit & 0x01) && (i == decZmqIndex)) // deczmqindex
                {
                    m_ZmqManager->ReceiveMessage(decZmqIndex, decZmqMsg, decMsgLen);
                    string decname = m_ZmqManager->GetName(decZmqIndex);
                    //cout << decname << endl;

                    //解析参数，生成想要的格式
                    _json2DecPair(decZmqMsg);
                    gsMonitor.insertDecodeParam(tempDecParam);

                    //插入到监控参数类中

                    //cout << decZmqMsg << endl;
                    //cout << "received decZmqMsg" << endl;
                }
                indexBit >>= 0x01; //下面这一位是否有消息 并不知道？？？
            }
        }

        //cout << "是否有新增数据:" << add_data << endl;

        if (add_data){
            //处理message，同时判断异常
            delayMsg = gsMonitor.checkDelay();

            //推送到httpclient中
            gsMonitor.pushPrameter2Gateway();

            flag = gsMonitor.isDelay;
            //cout << "监控是否延迟:" << flag <<endl;

            if (flag)
            {
                _pushException2Kafka();
            }
        }

        add_data = false; //表示本次循环中是否有数据
        sleep(sleep_time);
    }
}

string MonitorMessagerHandler::createJson()
{
    std::string jsonStr;
    Json::Value root;
    Json::StreamWriterBuilder writerBuilder;
    std::ostringstream os;

    root["decodeId"] = "Limingdjijfieje";
    root["cameraId"] = "Limingjxhuifhu23k";

    root["videoFPS"] = 100;
    root["decodeFPS"] = 5;
    root["SkipFrameFrequency"] = 6;

    std::unique_ptr<Json::StreamWriter> jsonWriter(writerBuilder.newStreamWriter());
    jsonWriter->write(root, &os);
    jsonStr = os.str();

    std::cout << jsonStr << std::endl;
    return jsonStr;
};

