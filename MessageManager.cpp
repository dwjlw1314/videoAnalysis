#include <uuid/uuid.h>
#include "MessageManager.hpp"

using namespace VideoEngine2;

MessageManager::MessageManager()
    :m_ZmqIndex(MAX_SOCKET_SET+1)
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;
    /** @todo 初始化工作 */
    m_pLogger = GsafetyLog::GetInstance();
    m_GroupId = getUuid(true);
    LOG4CPLUS_INFO("root", "Create Success!");
}

MessageManager::~MessageManager()
{
    /* release resource */
    google::protobuf::ShutdownProtobufLibrary();
}

void MessageManager::SetZmqIndex(uint32_t index)
{
    m_ZmqIndex = index;
}

void MessageManager::SetRdkafkaHost(const std::string& host)
{
    m_Broker = host;
}

void MessageManager::SetSubTopicName(const std::string& name)
{
    m_SubTopicNameV.push_back(name);
}

void MessageManager::SetPubTopicName(const std::string& status,
    const std::string& alarm, const std::string& analysis)
{
    m_StatusPubTopic = status;
    m_AlarmPubTopic = alarm;
    m_AnalysisPubTopic = analysis;
}

void MessageManager::SetGroupId(const std::string& gid)
{
    m_GroupId = gid;
}

bool MessageManager::InitResource(std::shared_ptr<ZmqManager>& zmqObject)
{
    m_KafkaMessage.kafkatype = PRODUCER|CONSUME|SUBCONSUME;
    m_KafkaMessage.groupid = m_GroupId;
    m_KafkaMessage.broker = m_Broker;
    m_KafkaMessage.topic_name_list.clear();
    // "gjsy" --> test topic name
    //msg.topic_name_list.push_back("gjsy");
    m_KafkaMessage.topic_name = m_StatusPubTopic;
    m_KafkaMessage.topic_name_list = m_SubTopicNameV;
    GsafetyRdkafka* pobj = new GsafetyRdkafka(m_KafkaMessage);
    m_Rdkafka.reset(pobj);

    m_ZmqManager = zmqObject;
    LOG4CPLUS_INFO("root", "kafka InitResource Finish!");
    return m_Rdkafka->GetInitStatus();
}

void MessageManager::MessageProcessLoop(void)
{
    uint64_t rfd = 0;
    std::string zmqMsg;
    /** @todo 是否还要使用暂定 */
    InstructionInfo instruct;

    KafkaMessage kafka_msg = m_KafkaMessage;

    rfd |= (uint64_t)0x0000000000000001 << m_ZmqIndex;

    while(1)
    {
        //使用发送端接收任务管理递送的数据
        uint64_t bit = m_ZmqManager->MessagePool(rfd,false,500);
        if (bit & 0xffffffffffffffff)
        {
            std::size_t msg_len;
            m_ZmqManager->ReceiveMessage(m_ZmqIndex, zmqMsg, msg_len, false);
            //接收任务管理类数据，发送到平台
            m_KafkaMessage.topic_name = m_StatusPubTopic;
            m_KafkaMessage.payload = zmqMsg;
            m_KafkaMessage.len = msg_len;
            if (m_Rdkafka->SendMessage(m_KafkaMessage))
                LOG4CPLUS_INFO("root", "Message delivery KAFKA Success!");
            else
                LOG4CPLUS_INFO("root", "Message delivery KAFKA Fail!");
        }

        kafka_msg.len = 0;
        kafka_msg.payload = "";
        //msg.offset = RdKafka::Topic::OFFSET_BEGINNING;
        m_Rdkafka->ReceiveMessage(kafka_msg);
        if (kafka_msg.len)
        {
            //发送平台下发的指令到任务管理类
            if (m_ZmqManager->SendMessage(m_ZmqIndex, kafka_msg.payload, kafka_msg.len))
                LOG4CPLUS_INFO("root", "Message delivery ZMQ Success!");
            else
                LOG4CPLUS_INFO("root", "Message delivery ZMQ Fail!");
        }
    }
}

bool MessageManager::SendStatusToPlatform(const std::string& msg, std::size_t len)
{
    m_KafkaMessage.topic_name = m_StatusPubTopic;
    m_KafkaMessage.payload = msg;
    m_KafkaMessage.len = len;
    return m_Rdkafka->SendMessage(m_KafkaMessage);
}

bool MessageManager::SendAlarmInfoToPlatform(const std::string& msg, std::size_t len)
{
    KafkaMessage kafka_message;
    kafka_message.topic_name = m_AlarmPubTopic;
    kafka_message.payload = msg;
    kafka_message.len = len;
    return m_Rdkafka->SendMessage(kafka_message);
}

bool MessageManager::SendTrustListToPlatform(const std::string& msg, std::size_t len)
{
    KafkaMessage kafka_message;
    kafka_message.topic_name = TRUST_TOPIC;
    kafka_message.payload = msg;
    kafka_message.len = len;
    return m_Rdkafka->SendMessage(kafka_message);
}

std::string MessageManager::SendMsgToPlatform()
{
    std::string msg;
    // InstructionInfo instruct;

    // instruct.set_iot(DELETE); //AMODIFY  DELETE

    // instruct.set_currentid(getUuid(true));

    // instruct.set_algorithmid("015f28b9df1bdd36427dd976fb73b29d");

    // instruct.set_containerid("4ba2fa9c7697");
    
    // instruct.set_algorithmtype(FIRE);

    // struct timeval tv;
	// gettimeofday(&tv, NULL);
	// time_t t = tv.tv_sec;
    // instruct.set_timestamp(t);

    // instruct.set_status(0);

    // AlgorithmParm* alparm;
    // alparm = instruct.add_aparms();
    // alparm->set_nms(0.6);
    // alparm->set_threshold(0.3);
    // alparm->set_batchsize(1);
    // alparm->set_cameraid("321d53fb18fa4692b40fa5c03a0d2eda");
    
    // InteractionArea *ia = new InteractionArea;
    // ia->set_interesttype(1);
    // ia->set_shutdownarea(true);
    // InteractionPoint *ip = ia->add_allpoints();
    // ip->set_horizontal(0.6);
    // ip->set_ordinate(0.4);

    // alparm->set_allocated_region(ia);

    // CameraInfo* caminfo;
    // caminfo = instruct.add_camerainfos();
    // caminfo->set_cameraid("321d53fb18fa4692b40fa5c03a0d2eda");
    // caminfo->set_cameraurl("rtsp://admin:12345@192.168.26.5:554/h264/ch1/main/av_stream");
    // caminfo->set_cameraname("instruct");
    // caminfo->set_desturl("rtmp://10.3.9.107:1937/live/");
    // caminfo->set_pushflow(true);
    // caminfo->set_endtimestamp(t);

    //instruct.SerializeToString(&msg);
    cv::Mat src;
	src = cv::imread("/opt/SFCN_trt/testDense/img0001_overlaymap.jpeg");
    InternalDataSetInfo internal_data;
    internal_data.set_iiot(CROWDDENSITY_VIDEO);
    internal_data.set_currentid("015f28b9df1bdd36427dd976fb73b29d");
    internal_data.set_trustflag(1);
    internal_data.set_cameraid("015f28b9df1bdd36427dd976fb73b29d");
    internal_data.set_cameraname("dwj");
    internal_data.set_algorithmtype(6);


    std::vector<unsigned char> buff;
    cv::imencode(".jpg", src, buff);
    std::string str;
    str.resize(buff.size());
    memcpy(&str[0], buff.data(), buff.size());
    internal_data.set_mat(str);
    internal_data.SerializeToString(&msg);
    return msg;
}

void MessageManager::RecvMsgFromPlatform(std::string msg, uint32_t index)
{
    uint64_t rfd = 0;
    rfd |= (uint64_t)0x0000000000000001 << index;

    while(1)
    {
        std::string zmqMsg;
        std::size_t msg_len;
        //使用发送端接收任务管理递送的数据
        uint64_t bit = m_ZmqManager->MessagePool(rfd,true,500);
        if (bit & 0xffffffffffffffff)
        {
            m_ZmqManager->ReceiveMessage(m_ZmqIndex, zmqMsg, msg_len);
        }
        else continue;

        InternalDataSetInfo internal_data;
        if(internal_data.ParseFromString(zmqMsg))
        {
            std::cout << internal_data.cameraid() << std::endl;
            cv::Mat mat2(1, internal_data.mat().size(), CV_8U, (char*)internal_data.mat().data());
            cv::Mat dst = cv::imdecode(mat2, CV_LOAD_IMAGE_COLOR);
            cv::imwrite("/opt/SFCN_trt/testDense/xxx.jpeg", dst);
        }
        // AlarmInfo alarm;
        // if (alarm.ParseFromString(zmqMsg))
        // {
        //     std::cout << alarm.alarmid() << std::endl;
        //     std::cout << alarm.timestamp() << std::endl;
        //     std::cout << alarm .cameraname()<< std::endl;
        //     std::cout << alarm.currentid()<< std::endl;
        //     std::cout << alarm.videopath()<< std::endl;
        //     std::cout << alarm.imagepath()<< std::endl;
        // }
    }
}