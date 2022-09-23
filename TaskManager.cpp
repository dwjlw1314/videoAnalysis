#include "TaskManager.hpp"

using namespace VideoEngine2;

TaskManager::TaskManager() :
    m_Status(false), m_Mode(false), m_InitMode(false),
    m_MongoDb(std::make_shared<MongoDB>()),
    m_ZmqManager(std::make_shared<ZmqManager>()),
    m_DataAnalysis(std::make_shared<DataAnalysis>()),
    m_MessageManager(std::make_shared<MessageManager>()),
#ifdef ENABLERZ
    m_VideoAlgorithm(std::make_shared<VideoAlgorithm>()),
#endif
    m_MonitorManager(std::make_shared<MonitorMessagerHandler>())
{
    std::string algorith_name;
    m_pLogger = GsafetyLog::GetInstance();

    //删除未处理告警视频文件
    std::string file_path = "rm -rf ";
    file_path += getProgramRunPath() + "/*.mp4";
    system(file_path.c_str());

    /** @todo 创建对应本模块日志输出格式文件 */
    m_pLogger->EnableConfigureWatch();

    try
    {
        //公共库函数API
        m_uri = getEnvVariable("GSAFETY_MONGODB");
        m_HostName = getEnvVariable("GSAFETY_HOSTNAME");
        algorith_name = getEnvVariable("GSAFETY_ALGORITH");
        m_AlgorithMd5 = FromStringGetMd5(algorith_name);
    }
    catch(std::logic_error e)
    {
        LOG4CPLUS_ERROR("root", "get EnvVariable error!");
        return;
    }

    LOG4CPLUS_INFO("root", "MongodbURL: " << m_uri);
    LOG4CPLUS_INFO("root", "ContaineID: " << m_HostName);
    LOG4CPLUS_INFO("root", "AlgorithID: " << m_AlgorithMd5);
    
    if (!m_MongoDb->Init(m_uri))
    {
        LOG4CPLUS_ERROR("root", "init mongodb fail!");
        return;
    }

    //公共库函数 hostname == ContainerID, 由于k8s机制,使用getEnvVariable
    //m_HostName = getHostName();

    /** @todo 从数据库获取程序启动基础信息 */
    if (!FromMongoDBGetBaseData(m_AlgorithMd5))
    {
        LOG4CPLUS_ERROR("root", "Get BaseData fail!");
        return;
    }

    /** @todo 从数据库获取摄像头和算法基础信息 */
    FromMongoDBGetContainerData(m_HostName);

    /** @todo 数据库读取资源可用性判断，暂时不实现 */

    /** @todo 创建文件存储单例对象 */
    LOG4CPLUS_INFO("root", "StorageServer: " << m_BaseData.StorageServer);
    GsFastDfsManager::GetInstance()->initSource("group1", m_BaseData.StorageServer);

    //m_Mode == true(1:1)
    if (!m_CameraDataM.empty() && !m_AlgorithmDataM.empty())
    {
        m_InitMode = true;
        m_Mode = (m_CameraDataM.size() == m_AlgorithmDataM.size());
        LOG4CPLUS_INFO("root", "AlgorithmMode: " << (m_Mode ? "1:1" : "1:N"));
    }

    /** @todo 线程池初始化 摄像头个数+消息管理1个 + 备用1个 */
    uint32_t thread_count = GetThreadNums(m_AlgorithmDataM, m_CameraDataM);
    m_ThreadPool = std::make_shared<ThreadPool>(thread_count);
    LOG4CPLUS_INFO("root", "ThreadPool Numbers: " << thread_count);

    /** @todo 初始化zeromq通信端子和GPU资源 */
    InitZeromqPairs();

    /** @todo 初始化消息管理 */
    LOG4CPLUS_INFO("root", "KafkaServer: " << m_BaseData.kafkaHost);
    if (!InitMessageManager())
    {
        LOG4CPLUS_ERROR("root", "InitMessageManager Fail!");
        return;
    }

    /** @todo 创建http监听服务，实现图片输入分析*/
    if (!InitHttpServerManager())
    {
        LOG4CPLUS_ERROR("root", "InitHttpServerManager Fail!");
        return;
    }

    /** @todo 初始化编解码资源 */
    if (!CreateCodecManager())
    {
        LOG4CPLUS_INFO("root", "CreateCodecManager Abnormal!");
    }

    /** @todo 初始化算法资源 */
    if (!CreateAlgorithManager())
    {
        LOG4CPLUS_INFO("root", "CreateAlgorithManager Abnormal!");
    }

    /** @todo 初始化告警资源, 已移动至算法模块 */

    /** @todo 初始化性能分析资源，(未实现)获取可用gpu号 */
    if (!InitPerformanceManager())
    {
        LOG4CPLUS_INFO("root", "InitPerformanceManager Abnormal!");
    }

    /** 
     * @brief Moved to the outside
     * @todo 发送状态到平台
     * SendRunStateToPlatform();
     */

    /** @todo 设置接收平台端递送的数据过滤值 */
    m_DataAnalysis->SetBaseInfo(m_BaseData);

    LOG4CPLUS_INFO("root", "TaskManager Initializer Success");

    /** @todo 当前类资源初始化状态标记 */
    m_Status = true;
}

TaskManager::~TaskManager()
{
    //todo-- 发送消息给平台
    //todo-- 处理资源释放
}

uint32_t TaskManager::GetThreadNums(const AlgorithmDataMap& amap, const CameraDataMap& cmap)
{
    uint32_t count = 0;
    for (auto& node : cmap)
    {
        if (node.second.AvailableStatus)
            count += 2;
        if (node.second.PushFlow)
            count++;
    }
    if (amap.size() == cmap.size())
    {
        for (auto& node : amap)
        {
            if (node.second.AvailableStatus)
                count++;
        }
    }
    else { count++; }

    //消息管理,性能分析,业务分析,http服务,备选 (预分配)
    count += 5;
   
    return count;
}

std::string TaskManager::FromStringGetMd5(const std::string& value)
{    
    char szBuf[1024] = {0};
	unsigned char pbyBuff[17] = {0};
	MD5_CTX mdContext;
 
	MD5_Init (&mdContext);
 
	MD5_Update (&mdContext, value.c_str(), value.length());
	MD5_Final (pbyBuff, &mdContext);

	for(int i = 0; i < MD5_DIGEST_LENGTH; i++) 
	{
		sprintf(&szBuf[i*2], "%02x", pbyBuff[i]);
	}
	
    std::string md5(szBuf);

	return md5;
}

bool TaskManager::FromMongoDBGetBaseData(const std::string& md5)
{
    std::vector<string> ret_value;
    std::map<string,string> key_value;
    key_value["AlgorithmID"] = md5;

    /** @todo 当前加载指定算法的参数,后面实现加载算法+主机的细粒度配置(已实现) */
    m_MongoDb->mongoQueryMatching<std::map<std::string,std::string>>("gsafety_base", key_value, ret_value);

    m_BaseData.AlgorithmId = md5;
    m_BaseData.ContainerID = m_HostName;
        
    for (auto node : ret_value)
    {
        LOG4CPLUS_INFO("root", node);
        if (m_DataAnalysis->OutBaseDataDeserialize(node, m_BaseData))
        {
            LOG4CPLUS_INFO("root", "Get BaseData Success!");
            return true;
        }
    }
    return false;
}

void TaskManager::FromMongoDBGetContainerData(const std::string& cid)
{
    std::vector<std::string> ret_value;
    std::map<string,string> key_value;
    key_value["ContainerID"] = cid;
    bool first_entry = true;

    m_MongoDb->mongoQueryMatching<std::map<std::string,std::string>>("gsafety_container", key_value, ret_value);

    for(auto node : ret_value)
    {
        CameraData camera_data;
        AlgorithmData algorithm_data;
        LOG4CPLUS_INFO("root", node);
       
        if (!m_DataAnalysis->OutContainerDataDeserialize(node, camera_data, algorithm_data))
            continue;

        if (m_CameraDataM.find(camera_data.CameraId) == m_CameraDataM.end())
            m_CameraDataM[camera_data.CameraId] = camera_data;

        if (first_entry && m_AlgorithmDataM.find(algorithm_data.AlgorithmId) == m_AlgorithmDataM.end())
            m_AlgorithmDataM[algorithm_data.AlgorithmId] = algorithm_data;

        if (camera_data.AvailableStatus && algorithm_data.AlgorithmId != camera_data.CameraId)
        {
            //只更新一次可用算法数据
            if (!first_entry) continue;
            first_entry = false;
            m_AlgorithmDataM.clear();
            algorithm_data.AlgorithmId = camera_data.CameraId;
            m_AlgorithmDataM[algorithm_data.AlgorithmId] = algorithm_data;
        }
    }
}

bool TaskManager::FromMongoDBGetTrustData(int32_t AlgorithmType)
{
    /** @todo read MongoDB gsafety_face/gsafety_vehicle */
    std::string table = "";
    std::vector<string> ret_value;
    if (FACE == AlgorithmType)
        table = "gsafety_face";
    else if (VEHICLE == AlgorithmType)
        table = "gsafety_vehicle";
    else return false;

    /** @todo 当前加载指定算法的参数,后面实现加载算法+主机的细粒度配置(已实现) */
    m_MongoDb->mongoSelectAll(table, ret_value);
        
    for (auto node : ret_value)
        m_DataAnalysis->OutTrustDataDeserialize(node, m_TrustDataTableM);
    
    LOG4CPLUS_INFO("root", "Get TrustData Success!");
    return true;
}

void TaskManager::InitZeromqPairs()
{
    //初始化摄像头通信端子
    for(auto& node : m_CameraDataM)
    {
        if (node.second.AvailableStatus)
        {
            AddZeromqIndexToMap(node.second.CameraId);
        }
        else
        {
            LOG4CPLUS_INFO("root", "Current Camera status invalid: " << node.second.CameraId);
            continue;
        }
        if (node.second.PushFlow)
        {
            std::string CameraId_PushFlow = node.second.CameraId + "E";
            AddZeromqIndexToMap(CameraId_PushFlow);
        }
    }

    //初始化任务管理通信端子
    std::string tm_md5 = FromStringGetMd5("TaskManager");
    AddZeromqIndexToMap(tm_md5, false);

    //初始化消息管理通信端子
    const std::string& mm_id = m_BaseData.ContainerID;
    AddZeromqIndexToMap(mm_id, false);

    //初始化性能分析通信端子
    std::string ai_pa_md5 = FromStringGetMd5("PerformanceAnalysisAI");
    AddZeromqIndexToMap(ai_pa_md5, false);

    //初始化性能分析通信端子
    std::string de_pa_md5 = FromStringGetMd5("PerformanceAnalysisDE");
    AddZeromqIndexToMap(de_pa_md5, false);

    //if (!m_BaseData.EnableHttpServer) return;

    //初始化分析类别告警分析通信端子
    std::string aa_md5 = FromStringGetMd5("AlarmAnalysis");
    AddZeromqIndexToMap(aa_md5, false);
}

void TaskManager::AddZeromqIndexToMap(const std::string& id, bool useGpu)
{
    //static uint32_t num = 1;
    IndexData index_data(useGpu);
    //index_data.BindName = id + std::to_string(num++);
    //index_data.index = m_ZmqManager->Add(index_data.BindName);
    index_data.BindName = id;
    index_data.index = m_ZmqManager->Add(id);
    m_ZeroMQIndexM[id] = std::move(index_data);
    LOG4CPLUS_INFO("root", id << "->BindZeromqIndex: " << index_data.index);
}

int TaskManager::FindZeromqIndex(const std::string& id)
{
    volatile int index = -1;
    ZeromqIndexDataMap::iterator iter = m_ZeroMQIndexM.begin();
    while (iter != m_ZeroMQIndexM.end())
    {
        if (!iter->first.compare(id))
        {
            if (!iter->second.used->load())
            {
                index = iter->second.index;
                break;
            }
        }
        iter++;
    }
    return index;
}

bool TaskManager::InitMessageManager(void)
{
    m_MessageManager->SetGroupId(m_BaseData.ContainerID);
    m_MessageManager->SetRdkafkaHost(m_BaseData.kafkaHost);
    m_MessageManager->SetPubTopicName(m_BaseData.KafkaStatusPubTopic,
        m_BaseData.KafkaAlarmPubTopic, m_BaseData.KafkaAnalysisPubTopic);

    for_each(m_BaseData.kafkaSubTopicName.cbegin(), m_BaseData.kafkaSubTopicName.cend(),
        [this](const std::string& val)->void { this->m_MessageManager->SetSubTopicName(val); });

    //添加kafka http信任名单数据订阅 topic
    m_MessageManager->SetSubTopicName(TRUST_TOPIC);

    const std::string& id = m_MessageManager->GetGroupId();
    int index = FindZeromqIndex(id);
    m_MessageManager->SetZmqIndex(index);

    if (!m_MessageManager->InitResource(m_ZmqManager))
        return false;

    //索引使用标记
    m_ZeroMQIndexM[id].used->store(true);

    // 消息处理主函数类型
    //MessageManagerHandler handler;
    m_ThreadPool->addJob(std::bind(&MessageManager::MessageProcessLoop, m_MessageManager));

    return true;
}

bool TaskManager::InitPerformanceManager(void)
{
    std::string ai_pa_md5 = FromStringGetMd5("PerformanceAnalysisAI");
    std::string de_pa_md5 = FromStringGetMd5("PerformanceAnalysisDE");
    int ai_index = FindZeromqIndex(ai_pa_md5);
    int de_index = FindZeromqIndex(de_pa_md5);
    if (!m_MonitorManager->InitResource(m_BaseData.ContainerID, m_ZmqManager, ai_index, de_index, 
        m_BaseData.kafkaHost, m_BaseData.KafkaAnalysisPubTopic))
        return false;

    m_ThreadPool->addJob(std::bind(&MonitorMessagerHandler::MessageProcessLoop, m_MonitorManager));
    
    return true;
}

bool TaskManager::CreateCodecManager(void)
{
    /** @todo 依次创建状态为可用且未激活的摄像头 */
    CameraDataMap::iterator iter;
    for(iter = m_CameraDataM.begin(); iter != m_CameraDataM.end(); iter++)
    {
        if (!iter->second.AvailableStatus || iter->second.ActiveStatus)
            continue;

        if (!AddCodecResource(iter->second))
        {
            //资源不足导致添加失败,摄像头状态不变
            LOG4CPLUS_ERROR("root", "AddCodecResource: " << iter->second.CameraId << " Failed");
            return false;
        }
        //添加成功
        LOG4CPLUS_INFO("root", "AddCodecResource: " << iter->second.CameraId << " Ok");
    }
    return true;
}

bool TaskManager::CreateAlgorithManager(void)
{
    /** @todo 依次创建状态为可用且未激活的算法 */
    AlgorithmDataMap::iterator iter;
    for(iter = m_AlgorithmDataM.begin(); iter != m_AlgorithmDataM.end(); iter++)
    {
        if (!iter->second.AvailableStatus || iter->second.ActiveStatus)
            continue;

        if (!AddAlgorithResource(iter->second))
        {
            //资源不足导致添加失败,算法状态不变
            LOG4CPLUS_ERROR("root", "AddAlgorithResource: " << iter->second.AlgorithmId << " Failed");
            return false;
        }
        //添加成功
        LOG4CPLUS_INFO("root", "AddAlgorithResource: " << iter->second.AlgorithmId << " Ok");
    }
    return true;
}

#ifdef ENABLERZ
bool TaskManager::AddCodecResource(CameraData& camera_data)
{
    //判断线程池可用数量
    if (m_ThreadPool->idlCount() < 2)
        return false;

    //新建解码-算法双向通信端
    if (m_ZeroMQIndexM.find(camera_data.CameraId) == m_ZeroMQIndexM.end())
    {
        AddZeromqIndexToMap(camera_data.CameraId);
    }

    //摄像头资源添加
    if (m_CameraDataM.find(camera_data.CameraId) == m_CameraDataM.end())
    {
        m_CameraDataM[camera_data.CameraId] = camera_data;
    }

    if (camera_data.PushFlow)
    {
        //构造编码的唯一ID
        std::string encode_id = camera_data.CameraId;
        encode_id += "E";

        //新建算法-编码双向通信端
        if (m_ZeroMQIndexM.find(encode_id) == m_ZeroMQIndexM.end())
            AddZeromqIndexToMap(encode_id);

        m_ZeroMQIndexM[encode_id].used->store(true);

        //绑定编码主处理函数
        auto e_fun = std::bind(&VideoAlgorithm::WriteFrameFFmpeg, m_VideoAlgorithm,
            m_CameraDataM[camera_data.CameraId], m_ZmqManager, m_ZeroMQIndexM);

        m_ThreadPool->addJob(e_fun);
    }

    m_ZeroMQIndexM[camera_data.CameraId].used->store(true);

    //绑定解码主处理函数
    auto d_fun = std::bind(&VideoAlgorithm::ReadFrameFFmpeg, m_VideoAlgorithm,
        m_CameraDataM[camera_data.CameraId], m_ZmqManager, m_ZeroMQIndexM);

    /**
     * 主处理函数更新(改由主任务管理):
     * CameraData: AvailableStatus、ActiveStatus字段(编码处理)
     * IndexData: used
     */
    m_ThreadPool->addJob(d_fun);

    return true;
}
#else
bool TaskManager::AddCodecResource(CameraData& camera_data)
{
    //判断线程池可用数量
    if (m_ThreadPool->idlCount() < 2)
        return false;

    //构造编码的唯一ID
    std::string encode_id = camera_data.CameraId;
    encode_id += "E";

    //新建解码-算法双向通信端
    if (m_ZeroMQIndexM.find(camera_data.CameraId) == m_ZeroMQIndexM.end())
    {
        AddZeromqIndexToMap(camera_data.CameraId);
    }

    if (camera_data.PushFlow)//推流
    {
        //新建算法-编码双向通信端
        if (m_ZeroMQIndexM.find(encode_id) == m_ZeroMQIndexM.end())
            AddZeromqIndexToMap(encode_id);
    }

    //摄像头资源添加,重复添加复用原有数据
    if (m_CameraDataM.find(camera_data.CameraId) == m_CameraDataM.end())
        m_CameraDataM[camera_data.CameraId] = camera_data;

    string md5 = FromStringGetMd5("PerformanceAnalysisDE");
    shared_ptr<CodecManager> codecManager = make_shared<CodecManager>();
    LOG4CPLUS_INFO("root", camera_data.CameraId << " ->Codec object creation");
    bool connectCamera = codecManager->initAudioAndVideoCodec(camera_data, m_ZmqManager, m_ZeroMQIndexM);
    if(!connectCamera)
    {
        return false;
    }

    m_ZeroMQIndexM[camera_data.CameraId].used->store(true);

    //绑定解码主处理函数
    auto d_fun = std::bind(&CodecManager::audioAndVideoDecoder, codecManager,
        m_ZeroMQIndexM[camera_data.CameraId], m_ZeroMQIndexM.find(md5)->second);

    /**
     * 主处理函数更新(改由主任务管理):
     * CameraData: AvailableStatus、ActiveStatus字段(编码处理)
     * IndexData: used
     */
    m_ThreadPool->addJob(d_fun);

    auto u_fun = std::bind(&CodecManager::uploadFileToFastDfs, codecManager);
    m_ThreadPool->addJob(u_fun);

    if (camera_data.PushFlow)//推流
    {
        m_ZeroMQIndexM[encode_id].used->store(true);

        //绑定编码主处理函数
        auto e_fun = std::bind(&CodecManager::audioAndVideoEncoder, codecManager,
            m_ZeroMQIndexM.find(encode_id)->second, m_ZeroMQIndexM.find(md5)->second);

        m_ThreadPool->addJob(e_fun);
    }

    return true;
}
#endif

bool TaskManager::ModifyCodecResource(CameraData& camera_data)
{
    if (m_ZeroMQIndexM.find(camera_data.CameraId) == m_ZeroMQIndexM.end())
        return false;

    //CameraData 转换 string (使用json)
    std::string msg;
    m_DataAnalysis->InDataSerialize(&camera_data, msg, CODEC);

    LOG4CPLUS_INFO("root", "TM to CODEC Instruction (Modify) Data:\n" << msg);

    int index = m_ZeroMQIndexM[camera_data.CameraId].index;
    if (!m_ZmqManager->SendMessage(index, msg, msg.length(),false))
        return false;

    m_ZeroMQIndexM[camera_data.CameraId].used->store(true);

    m_CameraDataM[camera_data.CameraId] = camera_data;
    m_CameraDataM[camera_data.CameraId].ActiveStatus = true;
    return true;
}

bool TaskManager::DeleteCodecResource(CameraData& camera_data)
{
    if (m_ZeroMQIndexM.find(camera_data.CameraId) == m_ZeroMQIndexM.end())
        return false;

    //CameraData 转换 string (使用json)
    std::string msg = "";
    m_DataAnalysis->InDataSerialize(&camera_data, msg, CODEC);

    int index = m_ZeroMQIndexM[camera_data.CameraId].index;
    if (!m_ZmqManager->SendMessage(index, msg, msg.length(),false))
        return false;

    LOG4CPLUS_INFO("root", "TM to CODEC Instruction (Delete) Data:\n" << msg);

    m_ZeroMQIndexM[camera_data.CameraId].used->store(false);

    return true;
}

bool TaskManager::AddAlgorithResource(AlgorithmData& algorith_data)
{
    //判断线程池可用数量
    if (m_ThreadPool->idlCount() < 1)
        return false;

    //算法资源添加
    m_AlgorithmDataM[algorith_data.AlgorithmId] = algorith_data;

    //1:1
    if (m_Mode)
    {
        //摄像头未激活状态变更为激活
        m_CameraDataM[algorith_data.AlgorithmId].ActiveStatus = true;

        //添加通信管理器
        LOG4CPLUS_INFO("root", algorith_data.AlgorithmId << " ->Algorithm object creation");
        m_AlgorithmObjectM[algorith_data.AlgorithmId] = std::make_shared<GsAIModelProcesser>(
            m_ZeroMQIndexM, m_TrustDataTableM);
        if (!m_AlgorithmObjectM[algorith_data.AlgorithmId]->initResource(m_AlgorithmDataM[algorith_data.AlgorithmId],
            m_ZmqManager, m_BaseData.AlgorithmType, m_uri, m_BaseData.UsePrivateLib))
            return false;
        
        //绑定算法主处理函数
        auto a_fun = std::bind(&GsAIModelProcesser::inferProcessLoop, m_AlgorithmObjectM[algorith_data.AlgorithmId]);
        m_ThreadPool->addJob(a_fun);
    }
    //1:N
    else
    {
        if (!algorith_data.ActiveStatus)
        {
            LOG4CPLUS_INFO("root", algorith_data.AlgorithmId << " ->Algorithm object creation");
            m_AlgorithmObjectM[algorith_data.AlgorithmId] = std::make_shared<GsAIModelProcesser>(
                m_ZeroMQIndexM, m_TrustDataTableM);
            if (!m_AlgorithmObjectM[algorith_data.AlgorithmId]->initResource(m_AlgorithmDataM[algorith_data.AlgorithmId],
                m_ZmqManager, m_BaseData.AlgorithmType, m_uri, m_BaseData.UsePrivateLib))
                return false;
                
            //绑定算法主处理函数
            auto a_fun = std::bind(&GsAIModelProcesser::inferProcessLoop, 
                m_AlgorithmObjectM[algorith_data.AlgorithmId]);

            m_ThreadPool->addJob(a_fun);
        }
        CameraDataMap::iterator iter;
        for(iter = m_CameraDataM.begin(); iter != m_CameraDataM.end(); iter++)
        {
            if (iter->second.AvailableStatus && !iter->second.ActiveStatus)
            {
                //添加监听通信端(使用套接字发送还是api调用)
                AlgorithmData tmp_ad = algorith_data;
                tmp_ad.AlgorithmId = iter->second.CameraId;
                std::string msg;
                m_DataAnalysis->InDataSerialize(&tmp_ad, msg, ALGORITHM);

                if (m_ZeroMQIndexM.find(algorith_data.AlgorithmId) == m_ZeroMQIndexM.end())
                    continue;

                LOG4CPLUS_INFO("root", "TM to AI Instruction (Add) Data:\n" << msg);

                int index = m_ZeroMQIndexM[algorith_data.AlgorithmId].index;
                if (!m_ZmqManager->SendMessage(index, msg, msg.length()))
                    LOG4CPLUS_ERROR("root", "AlgorithmID delivery Failed: " << algorith_data.AlgorithmId);
                iter->second.ActiveStatus = true;
            }
        }
    }

    m_AlgorithmDataM[algorith_data.AlgorithmId].ActiveStatus = true;

    return true;
}

bool TaskManager::ModifyAlgorithResource(AlgorithmData& algorith_data)
{
    bool status = true;
    std::string msg;

    if (m_ZeroMQIndexM.find(algorith_data.AlgorithmId) == m_ZeroMQIndexM.end())
        return false;

    int index = m_ZeroMQIndexM[algorith_data.AlgorithmId].index;

    //通知算法修改参数(使用套接字发送还是api调用)
    if (!m_DataAnalysis->InDataSerialize(&algorith_data, msg, ALGORITHM))
        status = false;

    LOG4CPLUS_INFO("root", "TM to AI Instruction (Modify) Data:\n" << msg);

    if (!m_ZmqManager->SendMessage(index, msg, msg.length()))
        status = false;

    if (m_Mode)
    {
        m_AlgorithmDataM[algorith_data.AlgorithmId] = algorith_data;
        m_AlgorithmDataM[algorith_data.AlgorithmId].ActiveStatus = true;
    }
    else
    {
        m_AlgorithmDataM[algorith_data.AlgorithmId] = algorith_data;
        if (m_AlgorithmDataM.find(algorith_data.AlgorithmId) == m_AlgorithmDataM.end())
        {
            m_AlgorithmDataM[algorith_data.AlgorithmId].ActiveStatus = false;
            m_AlgorithmDataM[algorith_data.AlgorithmId].AvailableStatus = false;
        }
    }

    return status;
}

bool TaskManager::DeleteAlgorithResource(AlgorithmData& algorith_data)
{
    bool status = true;
    std::string msg;

    if (m_ZeroMQIndexM.find(algorith_data.AlgorithmId) == m_ZeroMQIndexM.end())
        return false;

    int index = m_ZeroMQIndexM[algorith_data.AlgorithmId].index;

    if (!m_DataAnalysis->InDataSerialize(&algorith_data, msg, ALGORITHM))
        status = false;

    if (!m_ZmqManager->SendMessage(index, msg, msg.length()))
        status = false;

    LOG4CPLUS_INFO("root", "TM to AI Instruction (Delete) Data:\n" << msg);

    return status;
}

/* C++标准并不能保证volatile在多线程情况的正确性 */
bool TaskManager::SendRunStateToPlatform(volatile bool flag)
{
    std::string msg = "";
    sleep(1);  //保证订阅模式正常接收数据
    m_DataAnalysis->OutRunStateSerialize(msg, flag);
    LOG4CPLUS_INFO("root", "Send start status to platform");
    return m_MessageManager->SendStatusToPlatform(msg, msg.size());
}

bool TaskManager::SendAlgorithmAlarmResult(const InstructionData& inst_data)
{
    std::string msg = "";
    m_DataAnalysis->OutDataSerialize(inst_data, msg, SUCCESS);

    return m_MessageManager->SendAlarmInfoToPlatform(msg, msg.size());
}

void TaskManager::StartTaskProcessing(void)
{
    uint64_t binary_bit = 0;

    /** @todo 指定消息监听端 */
    std::string tm_md5 = FromStringGetMd5("TaskManager");
    binary_bit |= (uint64_t)0x0000000000000001 << m_ZeroMQIndexM[tm_md5].index;
    const std::string& mm_id = m_MessageManager->GetGroupId();
    binary_bit |= (uint64_t)0x0000000000000001 << m_ZeroMQIndexM[mm_id].index;

    //标记任务管理通信端已使用
    m_ZeroMQIndexM[tm_md5].used->store(true);

    //获取zeromq索引可用数量
    uint32_t nums = m_ZmqManager->GetIndexNums();

    //发送任务管理启动状态到平台
    SendRunStateToPlatform(m_Status);

    LOG4CPLUS_INFO("root", "Listen ZeromqIndexSet: " << binary_bit);

    /** @todo 启动消息监听(平台端和任务管理端) */
    while(m_Status)
    {
        uint64_t bit = m_ZmqManager->MessagePool(binary_bit);

        if (!(bit & 0xffffffffffffffff))
        {
            usleep(500000);
            continue;
        }

        for(int i = 0; i < nums; i++)
        {
            if (bit & 0x01)
            {
                std::size_t len;
                std::string msg;
                /** @todo 返回状态后期需要处理 */
                m_ZmqManager->ReceiveMessage(i, msg, len);

                if (i == m_ZeroMQIndexM[mm_id].index)
                    OutDataIntegratedProcess(msg);
                else
                    InDataIntegratedProcess(msg);
            }
            bit >>= 0x01;
        }
    }

    LOG4CPLUS_INFO("root", "Host: " << m_HostName << " start fail!");
}

void TaskManager::OutDataIntegratedProcess(const std::string& msg)
{
    //使用json 和 google proto 作为线程数据传输的方式
    InternalDataSet internal_data;
    if (m_DataAnalysis->InternalDataSetDeserialize(msg, internal_data))
    {
        /** 更新本地m_TrustDataTableM对象 */
        updataTrustListData(internal_data, false);
        return;
    }

    /** @todo 解析平台数据到本地数据集合 */
    InstructionData instruction_data;
    if (!m_DataAnalysis->OutDataDeserialize(msg, instruction_data))
    {
        /** @todo 解析失败处理逻辑 */
        if (instruction_data.CurrentId != m_HostName)
            return;

        SendIntegratedProcessResult(instruction_data, PARSINGERROR);
        LOG4CPLUS_INFO("root", "Instruction Parsing Fail: " << instruction_data.CurrentId);
        return;
    }

    //判断1:1 or 1:N 模式
    //bool mode = (instruction_data.AlgorithmDataM.size() == instruction_data.CameraDataM.size());
    if (!m_InitMode)
    {
        if (!(instruction_data.dot & D_ADD))
        {
            SendIntegratedProcessResult(instruction_data, INSTRUCTMISMATCH);
            std::string dot_name = m_DataAnalysis->DotConvertName(instruction_data.dot);
            LOG4CPLUS_INFO("root", "Unknown algorithm mode under current oper: " << dot_name);
            return;
        }
        m_InitMode = true;
        m_Mode = (instruction_data.AlgorithmDataM.size() == instruction_data.CameraDataM.size());
        LOG4CPLUS_INFO("root", "AlgorithmMode: " << (m_Mode ? "1:1" : "1:N"));
    }

    if (instruction_data.dot & D_ADD)
    {
        /** 50 表示限制显卡支持的做大解码路数,后期可更具算力表获取该值 */
        int total = instruction_data.CameraDataM.size() + getAvailableCamera();
        if (total > 50)
        {
            SendIntegratedProcessResult(instruction_data, NOCAPACITY);
            std::string dot_name = m_DataAnalysis->DotConvertName(instruction_data.dot);
            LOG4CPLUS_INFO("root", "Beyond the decoding capacity of the graphics card: " << total);
            return;
        }
        uint32_t thread_count = GetThreadNums(instruction_data.AlgorithmDataM, 
            instruction_data.CameraDataM);
        //该模式下只添加编解码和算法线程资源池数量
        m_ThreadPool->createTask(thread_count-3);
        LOG4CPLUS_INFO("root", "Increase ThreadPool Numbers: " << thread_count-3);
    }

    /** @todo 解析成功处理逻辑,添加摄像头和算法的调度 */
    if (instruction_data.CameraDataM.size())
        CameraDispatch(instruction_data.CameraDataM);

    if (instruction_data.AlgorithmDataM.size())
        AlgorithmDispatch(instruction_data.AlgorithmDataM, m_Mode);

    //添加数据到mongodb
    if (instruction_data.dot & D_ADD)
        AddContainerTableData(instruction_data);

    /** @todo 递送摄像头和算法处理结果到平台 */
    SendIntegratedProcessResult(instruction_data, SUCCESS);
}

void TaskManager::SendIntegratedProcessResult(InstructionData& inst_data, DATASTATE status)
{
    std::string msg = "";
    m_DataAnalysis->OutDataSerialize(inst_data, msg, status);

    const std::string& mm_id = m_MessageManager->GetGroupId();
    int mm_index = m_ZeroMQIndexM[mm_id].index;

    m_ZmqManager->SendMessage(mm_index, msg, msg.length(), false);
    LOG4CPLUS_INFO("root", "Send Instructions Integrated Process Status Ok");
}

void TaskManager::InDataIntegratedProcess(const std::string& msg)
{    
    /** @todo 解析本地数据到其它算法POD */
    InternalDataSet internal_data;
    if (m_DataAnalysis->InternalDataSetDeserialize(msg, internal_data))
    {
        /** 更新本地信任名单库和信任集合类成员,写库失败取消kakfka发送 */
        updataTrustListData(internal_data, true);
        /** @brief 发送数据到kafka */
        m_MessageManager->SendTrustListToPlatform(msg, msg.size());
        return;
    }

    /** @todo 解析本地数据到平台数据集合 */
    InstructionData instruct_data;
    if (!m_DataAnalysis->InDataDeserialize(msg, instruct_data))
        return;

    if (D_NOTICE == instruct_data.dot)
    {
        LOG4CPLUS_INFO("root", "TM from AI or CM Alarm Data:\n" << msg);
        /** @brief 算法告警信息更新 */
        if (instruct_data.AlarmData.Type == ALARMC)
        {
            std::string filter = "";
            std::string update = "";
            CreateAlarmFilterStr(filter, instruct_data.AlarmData.AlarmId);
            CreateAlarmUpdateStr(update, instruct_data.AlarmData);

            m_MongoDb->mongoUpdate("gsafety_alarm", filter, update);
        }
        /** @brief 算法告警信息入库 */
        else
            m_MongoDb->mongoInsertMany("gsafety_alarm", msg);

        /** @brief 发送算法告警信息到平台 */
        SendAlgorithmAlarmResult(instruct_data);
        return;
    }

    LOG4CPLUS_INFO("root", "In Instruction Data:\n" << msg);
    
    /** @todo 更新算法和编解码集合状态,后期for替换为if即可 */
    DATASTATE cur_status;
    CameraDataMap::const_iterator citer = instruct_data.CameraDataM.cbegin();
    for(; citer != instruct_data.CameraDataM.cend(); citer++)
    {
        cur_status = citer->second.Status;
        if (D_IDEL == citer->second.dot && SUCCESS == cur_status)
        {
            m_CameraDataM[citer->first].ActiveStatus = false;
            m_ZeroMQIndexM[citer->first].used->store(false);
            if (m_CameraDataM[citer->first].PushFlow)
                m_ZeroMQIndexM[citer->first + "E"].used->store(false);
            //新增摄像头自动关闭->发送关闭命令到算法线程
            AlgorithmDataMap::iterator aiter = m_AlgorithmDataM.find(citer->first);
            if (!m_Mode && aiter != m_AlgorithmDataM.end())
                UpdateAlgorithmID(citer->first);

            AlgorithmData ad = m_AlgorithmDataM.begin()->second;
            ad.AlgorithmId = citer->first;
            ad.dot = D_DEL;

            /** @todo 向算法中删除一个摄像头监听 */
            DeleteAlgorithResource(ad);

            /** @brief 打开会导致索引收不到数据 */
            // m_ZmqManager->Del(m_ZeroMQIndexM[citer->first].BindName);
            // m_ZeroMQIndexM.erase(citer->first);
            // if (m_CameraDataM[citer->first].PushFlow)
            // {
            //     std::string encode_id = citer->first + "E";
            //     m_ZmqManager->Del(m_ZeroMQIndexM[encode_id].BindName);
            //     m_ZeroMQIndexM.erase(encode_id);
            // }
        }
        if (D_ADD == citer->second.dot && FAILED == cur_status)
        {
            m_CameraDataM[citer->first].ActiveStatus = false;
            m_ZeroMQIndexM[citer->first].used->store(false);
            if (m_CameraDataM[citer->first].PushFlow)
                m_ZeroMQIndexM[citer->first + "E"].used->store(false);
        }
        else if (D_ADD != citer->second.dot && FAILED == cur_status)
        {
            m_CameraDataM[citer->first].ActiveStatus = true;
            m_ZeroMQIndexM[citer->first].used->store(true);
            if (m_CameraDataM[citer->first].PushFlow)
                m_ZeroMQIndexM[citer->first + "E"].used->store(true);
        }
        else /** @todo 更新数据库状态 */
            UpdateContainerTableData(citer->first, instruct_data.dot, CODEC);
    }
    AlgorithmDataMap::const_iterator aiter = instruct_data.AlgorithmDataM.cbegin();
    for(; aiter != instruct_data.AlgorithmDataM.cend(); aiter++)
    {
        cur_status = aiter->second.Status;
        if (D_DEL == aiter->second.dot && SUCCESS == cur_status)
        {
            m_AlgorithmObjectM.erase(aiter->first);
            if (m_AlgorithmDataM.find(aiter->first) != m_AlgorithmDataM.end())
                m_AlgorithmDataM[aiter->first].ActiveStatus = false;
        }
        /** @todo 更新数据库状态 */
        UpdateContainerTableData(aiter->first, instruct_data.dot, ALGORITHM);
    }

    /** @todo 更新m_InitMode状态,重新确认比例模式,该版本弃用 */
    
    /** @todo 发送状态信息到平台 */
    SendIntegratedProcessResult(instruct_data, cur_status);
}

void TaskManager::AddContainerTableData(const InstructionData& inst_data)
{
    CameraDataMap::const_iterator iter = inst_data.CameraDataM.cbegin();
    for(; iter != inst_data.CameraDataM.cend(); iter++)
    {
        std::string msg = "";
        std::vector<std::string> ret;
        std::map<std::string,std::string> key_value;
        key_value["ContainerID"] = m_BaseData.ContainerID;
        key_value["CameraId"] = iter->second.CameraId;
        m_MongoDb->mongoQueryMatching<std::map<std::string,std::string>>("gsafety_container", key_value, ret);

        auto fun = [](AlgorithmDataMap& adm)->const std::string
        {
            for(auto& node : adm)
            {
                if (node.second.AvailableStatus)
                    return node.second.AlgorithmId;
            }
            return "";
        };
        
        AlgorithmDataMap::const_iterator aiter;
        if (m_Mode)
            aiter = inst_data.AlgorithmDataM.find(iter->second.CameraId);
        else
            aiter = m_AlgorithmDataM.find(fun(m_AlgorithmDataM));

        m_DataAnalysis->OutContainerDataSerialize(msg, iter->second, aiter->second);

        if (ret.size())
        {
            LOG4CPLUS_INFO("root", "The camera added to the database already exists: " << iter->second.CameraId);
            m_MongoDb->mongoDeleteMany("gsafety_container", key_value);
        }

        if (m_MongoDb->mongoInsertMany("gsafety_container", msg))
            LOG4CPLUS_INFO("root", "The camera added to the database ok:\n" << msg);
        else
            LOG4CPLUS_ERROR("root", "The camera added to the database fail:\n" << msg);
    }
}

void TaskManager::UpdateContainerTableData(const std::string& id, DOT type, METADATATYPE mtype)
{
    /** @todo 编解码状态更新 */
    if (CODEC == mtype)
    {
        std::string filter = "";
        std::string update = "";
        CreateFilterStr(filter, id);
        CreateCameraUpdateStr(update, id, type);

        LOG4CPLUS_INFO("root", "ContainerTable Codec updata ID:\n" << filter << "\nupdate Data:\n" << update);

        m_MongoDb->mongoUpdate("gsafety_container", filter, update);
    }
    /** @todo 1:1算法修改成功，更新指定id算法数据; 1:N没有修改状态*/
    else if (D_AMODIFY == type && ALGORITHM == mtype)
    {
        std::string filter = "";
        std::string update = "";
        CreateFilterStr(filter, id);
        CreateAlgorithUpdateStr(update, id, type);

        LOG4CPLUS_INFO("root", "ContainerTable Algor updata ID:\n" << filter << "\nupdate Data:\n" << update);

        m_MongoDb->mongoUpdate("gsafety_container", filter, update);
    }

    /** @todo 1:N算法添加和删除操作不处理 */
}

void TaskManager::CreateFilterStr(std::string& filter, const std::string& id)
{
    Json::Value root;
    Json::StreamWriterBuilder writerBuilder;
    std::ostringstream os;

    root["ContainerID"] = m_BaseData.ContainerID;
    root["CameraId"] = id;

    std::unique_ptr<Json::StreamWriter> jsonWriter(writerBuilder.newStreamWriter());
    jsonWriter->write(root, &os);
    filter = os.str();
}

void TaskManager::CreateCameraUpdateStr(std::string& update, const std::string& id, DOT type)
{
    Json::Value root;
    Json::StreamWriterBuilder writerBuilder;
    std::ostringstream os;

    if (D_ADD == type)
        root["$set"]["CameraStatus"] = true;
    else if (D_DEL == type || D_IDEL == type)
        root["$set"]["CameraStatus"] = false;
    else if (D_VMODIFY == type)
    {
        root["$set"]["CameraName"] = m_CameraDataM[id].CameraName;
        root["$set"]["CameraUrl"] = m_CameraDataM[id].CameraUrl;
        root["$set"]["DestUrl"] = m_CameraDataM[id].DestUrl;
        root["$set"]["PushFlow"] = m_CameraDataM[id].PushFlow;
        root["$set"]["SkipFrameFrequency"] = m_CameraDataM[id].SkipFrameFrequency;
        root["$set"]["EndTimeStamp"] = m_CameraDataM[id].EndTimeStamp;
        root["$set"]["SaveVideoDuration"] = m_CameraDataM[id].SaveVideoDuration;
    }else return;

    std::unique_ptr<Json::StreamWriter> jsonWriter(writerBuilder.newStreamWriter());
    jsonWriter->write(root, &os);
    update = os.str();
}

void TaskManager::CreateAlgorithUpdateStr(std::string& update, const std::string& id, DOT type)
{
    std:string region = "";
    Json::Value root;
    Json::StreamWriterBuilder writerBuilder;
    std::ostringstream os;

    root["$set"]["AlgorithmNms"] = m_AlgorithmDataM[id].Nms;
    root["$set"]["AlgorithmThreshold"] = m_AlgorithmDataM[id].Threshold;
    root["$set"]["AlgorithmBatchSize"] = m_AlgorithmDataM[id].BatchSize;
    root["$set"]["AlgorithmResolution"] = m_AlgorithmDataM[id].Resolution;
    root["$set"]["AlgorithmTrustList"] = m_AlgorithmDataM[id].TrustList;
    root["$set"]["AlgorithmPersonnels"] = m_AlgorithmDataM[id].Personnels;
    root["$set"]["AlgorithLeaveTimeLimit"] = m_AlgorithmDataM[id].LeaveTimeLimit;
    //root["$set"]["AlgorithmRegion"] = m_DataAnalysis->FromRegionToString(m_AlgorithmDataM[id].Region);
    m_DataAnalysis->AlgorithmRegionSerialize(region, m_AlgorithmDataM[id].Region);
    root["$set"]["AlgorithmAlarmInterval"] = m_AlgorithmDataM[id].AlarmInterval;
    root["$set"]["AlgorithmRegion"] = region;

    std::unique_ptr<Json::StreamWriter> jsonWriter(writerBuilder.newStreamWriter());
    jsonWriter->write(root, &os);
    update = os.str();
}

void TaskManager::CreateAlarmFilterStr(std::string& filter, const std::string& id)
{
    Json::Value root;
    Json::StreamWriterBuilder writerBuilder;
    std::ostringstream os;

    root["AlarmId"] = id;

    std::unique_ptr<Json::StreamWriter> jsonWriter(writerBuilder.newStreamWriter());
    jsonWriter->write(root, &os);
    filter = os.str();
}

void TaskManager::CreateAlarmUpdateStr(std::string& update, const AlarmResult& alarm_result)
{
    Json::Value root;
    Json::StreamWriterBuilder writerBuilder;
    std::ostringstream os;

    root["$set"]["VideoPath"] = alarm_result.VideoPath;
    root["$set"]["CameraName"] = alarm_result.CameraName;
    root["$set"]["Type"] = alarm_result.Type;

    std::unique_ptr<Json::StreamWriter> jsonWriter(writerBuilder.newStreamWriter());
    jsonWriter->write(root, &os);
    update = os.str();
}

void TaskManager::CameraDispatch(CameraDataMap& cmap)
{
    CameraDataMap::iterator iter;
    for(iter = cmap.begin(); iter != cmap.end(); iter++)
    {
        //指令解析的摄像头不在已有运行过的摄像头集合中的处理逻辑
        if (m_CameraDataM.find(iter->second.CameraId) == m_CameraDataM.end())
        {
            //添加操作; 删除和修改不用操作
            if (iter->second.dot & D_ADD)
            {
                /** @todo 添加摄像头资源 */
                if (!AddCodecResource(iter->second))
                {
                    iter->second.Status = NORESOURCE;
                    continue;
                }
                
                iter->second.Status = SUCCESS;
            }
            else iter->second.Status = NOEXISTED;
        }
        else  //指令解析的摄像头存在已有运行过的摄像头中的处理逻辑
        {
            //添加操作,并且是可用未激活状态
            if (iter->second.dot & D_ADD && !m_CameraDataM[iter->first].ActiveStatus)
            {
                //添加模式,已有摄像头替换成新数据
                m_CameraDataM[iter->first] = iter->second;

                /** @todo 添加摄像头资源 */
                if (!AddCodecResource(iter->second))
                {
                    iter->second.Status = NORESOURCE;
                    continue;
                }
                
                iter->second.Status = SUCCESS;
            }
            else if (iter->second.dot & D_VMODIFY)
            {
                if (!m_CameraDataM[iter->first].ActiveStatus || 
                    !m_CameraDataM[iter->first].AvailableStatus)
                {
                    iter->second.Status = NOEXISTED;
                    continue;
                }

                /** @todo 修改摄像头资源 */
                if (!ModifyCodecResource(iter->second))
                {
                    iter->second.Status = NOEXISTED;
                    continue;
                }

                iter->second.Status = SUCCESS;
            }
            else if (iter->second.dot & D_DEL)
            {
                if (!m_CameraDataM[iter->first].ActiveStatus || 
                    !m_CameraDataM[iter->first].AvailableStatus)
                {
                    iter->second.Status = NOEXISTED;
                    continue;
                }
                
                /** @todo 删除摄像头资源 */
                if (!DeleteCodecResource(iter->second))
                {
                    iter->second.Status = NOEXISTED;
                    continue;
                }

                //m_CameraDataM[iter->first].ActiveStatus = false;

                if (m_CameraDataM[iter->first].PushFlow)
                {
                    CameraData cd = iter->second;
                    cd.CameraId = cd.CameraId + "E";
                    DeleteCodecResource(cd);
                }   
                
                iter->second.Status = SUCCESS;
            }
            else iter->second.Status = EXISTED;
        }
    }
}

void TaskManager::AlgorithmDispatch(AlgorithmDataMap& amap, bool mode)
{
    //模式选择不按照算法类型,按照指令摄像头和算法的唯一ID做参考
    AlgorithmDataMap::iterator iter;

    auto fun = [](AlgorithmDataMap& adm, AlgorithmData& ndm)->AlgorithmData&
    {
        for(auto& node : adm)
        {
            if (node.second.AvailableStatus)
                return node.second;
            else
                continue;
        }
        return ndm;
    };

    for(iter = amap.begin(); iter != amap.end(); iter++)
    {
        //指令解析的算法不在已有运行过的算法中的处理逻辑
        if (m_AlgorithmDataM.find(iter->second.AlgorithmId) == m_AlgorithmDataM.end())
        {
            //1:1模式下的添加操作
            if (mode && iter->second.dot & D_ADD)
            {
                /** @todo 添加算法资源 */
                if (!AddAlgorithResource(iter->second))
                {
                    //添加失败处理
                    iter->second.Status = NORESOURCE;
                    continue;
                }

                iter->second.Status = SUCCESS;
            }
            //1:N模式下的添加操作,忽略算法Id匹配
            else if (!mode && iter->second.dot & D_ADD)
            {
                //算法添加之前判断摄像头是否可用并未激活
                if (m_CameraDataM.find(iter->second.AlgorithmId) == m_CameraDataM.end()
                    || !m_CameraDataM[iter->second.AlgorithmId].ActiveStatus)
                {
                    /** @todo 向已有算法中添加一个摄像头监听 */
                    AlgorithmData& ad = fun(m_AlgorithmDataM, iter->second);
                    ad.dot = iter->second.dot;  //确保操作类型一致
                    ad.CurrentId = iter->second.CurrentId;
                        
                    if (!AddAlgorithResource(ad))
                    {
                        iter->second.Status = NORESOURCE;
                        continue;
                    }
                }

                iter->second.Status = IGNOREAID;
            }
            //1:N模式下的修改操作
            else if (!mode && iter->second.dot & D_AMODIFY)
            {
                if (m_CameraDataM.find(iter->second.AlgorithmId) == m_CameraDataM.end()
                    || !m_CameraDataM[iter->second.AlgorithmId].AvailableStatus
                    || !m_CameraDataM[iter->second.AlgorithmId].ActiveStatus)
                {
                    /** @todo 算法和摄像头都不存在或不可用处理方式 */
                    iter->second.Status = NOEXISTED;
                    continue;
                }
                    
                /** @todo 修改算法参数 */
                // AlgorithmData& ad = fun(m_AlgorithmDataM, iter->second);
                // iter->second.AlgorithmId = ad.AlgorithmId;
                if (!ModifyAlgorithResource(iter->second))
                {
                    //修改失败处理
                    iter->second.Status = NOEXISTED;
                    continue;
                }

                iter->second.Status = SUCCESS;
            }
            //1:N模式下的删除操作
            else if (!mode && iter->second.dot & D_DEL)
            {
                if (m_CameraDataM.find(iter->second.AlgorithmId) == m_CameraDataM.end()
                    || !m_CameraDataM[iter->second.AlgorithmId].AvailableStatus
                    || !m_CameraDataM[iter->second.AlgorithmId].ActiveStatus)
                {
                    /** @todo 算法和摄像头都不存在或不可用处理方式 */
                    iter->second.Status = NOEXISTED;
                    continue;
                }

                /** @todo 向算法中删除一个摄像头监听 */
                if (!DeleteAlgorithResource(iter->second))
                {
                    //删除失败处理
                    iter->second.Status = NOEXISTED;
                    continue;
                }
                m_CameraDataM[iter->first].ActiveStatus = false;
                iter->second.Status = SUCCESS;
            }
            else iter->second.Status = NOEXISTED;
        }
        else  //指令解析的算法存在已有运行过的算法中的处理逻辑
        {
            //1:1模式下的删除操作
            if (mode && iter->second.dot & D_DEL)
            {
                /** @todo 删除算法资源 */
                if (!m_AlgorithmDataM[iter->first].ActiveStatus
                    || !DeleteAlgorithResource(iter->second))
                {
                    iter->second.Status = NOEXISTED; //可能永远不执行
                    continue;
                }

                m_CameraDataM[iter->first].ActiveStatus = false;
                m_AlgorithmDataM[iter->first].ActiveStatus = false;
                //这里不删除m_AlgorithmObjectM和m_AlgorithmDataM资源，由算法线程通知删除
                iter->second.Status = SUCCESS;
            }
            //1:1模式下的修改操作
            else if (mode && iter->second.dot & D_AMODIFY)
            {
                /** @todo 修改算法资源 */

                if (!ModifyAlgorithResource(iter->second))
                {
                    iter->second.Status = NOEXISTED; //可能永远不执行
                    continue;
                }

                iter->second.Status = SUCCESS;
            }
            //1:1模式下的添加操作,并且是可用未激活状态
            else if (mode && iter->second.dot & D_ADD && 
                !m_AlgorithmDataM[iter->first].ActiveStatus)
            {
                /** @todo 创建算法资源 */
                if (!AddAlgorithResource(iter->second))
                {
                    iter->second.Status = EXISTED; //可能永远不执行
                    continue;
                }

                /** @todo 更新原有激活状态 */
  
                iter->second.Status = SUCCESS;
            }
            //1:N模式下的修改操作
            else if (!mode && iter->second.dot & D_AMODIFY)
            {
                /** @todo 修改已有算法参数 */
                // AlgorithmData& ad = fun(m_AlgorithmDataM, iter->second);
                // iter->second.AlgorithmId = ad.AlgorithmId;
                if (!ModifyAlgorithResource(iter->second))
                {
                    iter->second.Status = NOEXISTED; //可能永远不执行
                    continue;
                }
                
                iter->second.Status = SUCCESS;
            }
             //1:N模式下的删除操作
            else if (!mode && iter->second.dot & D_DEL)
            {
                if (!m_CameraDataM[iter->first].ActiveStatus)
                {
                    iter->second.Status = NOEXISTED;
                    continue;
                }

                /** @todo 向已有算法中删除一个摄像头监听 */
                if (!DeleteAlgorithResource(iter->second))
                {
                    iter->second.Status = NOEXISTED; //可能永远不执行
                    continue;
                }

                m_CameraDataM[iter->first].ActiveStatus = false;

                /** @todo 更改算法的ID, 取消没有激活状态即删除所有资源 */
                UpdateAlgorithmID(iter->first);
                
                iter->second.Status = SUCCESS;
            }
            //1:N模式下的添加操作; 不判断ActiveStatus
            else if (!mode && iter->second.dot & D_ADD)
            {
                //算法添加之前判断摄像头是否可用并未激活
                if (m_CameraDataM.find(iter->second.AlgorithmId) == m_CameraDataM.end()
                    || !m_CameraDataM[iter->second.AlgorithmId].ActiveStatus)
                {
                    /** @todo 向已有算法中添加一个摄像头监听 */
                    AlgorithmData& ad = fun(m_AlgorithmDataM, iter->second);
                    ad.dot = iter->second.dot;  //确保操作类型一致
                    ad.CurrentId = iter->second.CurrentId;
                        
                    if (!AddAlgorithResource(ad))
                    {
                        iter->second.Status = NORESOURCE;
                        continue;
                    }
                }
                iter->second.Status = EXISTED;
            }
            else iter->second.Status = EXISTED;
        }
    }
}

void TaskManager::UpdateAlgorithmID(const std::string& aid)
{
    CameraDataMap::iterator iter = m_CameraDataM.begin();
    for(; iter != m_CameraDataM.end(); iter++)
    {
        if (iter->first == aid) continue;
        if (iter->second.AvailableStatus && iter->second.ActiveStatus)
        {
            m_AlgorithmObjectM[iter->second.CameraId] = m_AlgorithmObjectM[aid];
            m_AlgorithmObjectM.erase(aid);
            m_AlgorithmDataM[aid].AlgorithmId = iter->second.CameraId;
            m_AlgorithmDataM[iter->second.CameraId] = m_AlgorithmDataM[aid];
            m_AlgorithmDataM.erase(aid);
            LOG4CPLUS_INFO("root", "Algorithm New ID: " << iter->second.CameraId);
            return;
        }
    }
}

int TaskManager::getAvailableCamera()
{
    int count = 0;
    for(auto& node : m_CameraDataM)
    {
        if (node.second.AvailableStatus)
            count++;
        else continue;
        if (node.second.PushFlow)
            count++;
    }
    return count;
}

bool TaskManager::InitHttpServerManager()
{
    /** @todo 加载名单信息表(eg: 人脸，车辆) */
    FromMongoDBGetTrustData(m_BaseData.AlgorithmType);

    if (   m_BaseData.AlgorithmType == FACE 
        || m_BaseData.AlgorithmType == VEHICLE
        || m_BaseData.AlgorithmType == CROWD_DENSITY)
    {
        m_HttpServerManager = std::make_shared<HttpServerManager>(m_BaseData.HttpHost);

        /** @todo 创建数据分析线程 */
        auto alarmAnalysis_fun = std::bind(&HttpServerManager::alarmAnalysis, m_HttpServerManager,
            m_BaseData, m_ZmqManager, m_ZeroMQIndexM, m_TrustDataTableM);
        m_ThreadPool->addJob(alarmAnalysis_fun);
    }
    else return true;

    //读取配置判断是否启用相关http服务
    if (!m_BaseData.EnableHttpServer) return true;

    //http服务资源初始化
    auto http_fun = std::bind(&HttpServerManager::initResource, m_HttpServerManager,
        m_BaseData, m_ZmqManager, m_ZeroMQIndexM);
    m_ThreadPool->addJob(http_fun);

    LOG4CPLUS_INFO("root", "InitHttpServerManager Init Ok!");

    return true;
}

//目前是人脸，车牌库更新, 算法增多可适当统一接口，减少代码量. 项目数据库接口无法保证原子写入
void TaskManager::updataTrustListData(const InternalDataSet& internal_data, bool flag)
{
    /** @todo 更新信任名单集合 */
    if (internal_data.idot == I_FACERESULT_HTTP)  //人脸
    {
        int i = 0;
        if (internal_data.AdditionInfoV.size() != internal_data.DetectBoxV.size())
        {
            LOG4CPLUS_INFO("root", "updataTrustListData <FaceInfoV != DetectBoxV>");
            return;
        }

        /** @todo 更新gsafety_face表内容,只实现添加能力(实现部分上层业务) */
        for (auto& node : internal_data.AdditionInfoV)
        {
            std::string msg;
            TrustDataTable tdt;
            std::vector<std::string> ret;
            std::map<std::string,std::string> key_value;

            tdt.id = node.Id;
            tdt.name = node.Name;
            tdt.department = node.Department;
            tdt.trust = internal_data.TrustFlag;
            tdt.feature = internal_data.DetectBoxV[i++].feature;

            if (tdt.id.empty()) continue;

            if (m_TrustDataTableM.find(node.Id) == m_TrustDataTableM.end())
            {
                m_TrustDataTableM[node.Id] = tdt;
                LOG4CPLUS_INFO("root", node.Id << " The Update of TrustDataTableMap");
            }

            if (!flag) continue;

            key_value["Id"] = node.Id;
            m_MongoDb->mongoQueryMatching<std::map<std::string,std::string>>("gsafety_face", key_value, ret);

            if (ret.size())
            {
                LOG4CPLUS_INFO("root", "face TrustList database already exists: " << node.Id);
                continue;
            }

            /** @todo 转换mongogdb json格式 */
            m_DataAnalysis->OutTrustDataSerialize(msg, tdt, I_ADDFACE);

            if (!m_MongoDb->mongoInsertMany("gsafety_face", msg))
            {
                LOG4CPLUS_ERROR("root", "The TrustData added to the database fail:\n" << msg);
                continue;
            }
            LOG4CPLUS_ERROR("root", "The TrustData added to the database Success:\n" << msg);


        }
    }
    else if (internal_data.idot == I_VEHICLERESULT_HTTP)  //车辆
    {
        /** @todo 更新gsafety_vehicle表内容 */
         int i = 0;
        if (internal_data.AdditionInfoV.size() != internal_data.DetectBoxV.size())
        {
            LOG4CPLUS_INFO("root", "updataTrustListData <VehicleInfoV != DetectBoxV>");
            return;
        }

        /** @todo 更新gsafety_vehicle表内容,只实现添加能力(实现部分上层业务) */
        for (auto& node : internal_data.AdditionInfoV)
        {
            std::string msg;
            TrustDataTable tdt;
            std::vector<std::string> ret;
            std::map<std::string,std::string> key_value;

            tdt.id = node.Id;
            tdt.name = node.Name;
            tdt.department = node.Department;
            tdt.trust = internal_data.TrustFlag;
            tdt.vehicle = internal_data.DetectBoxV[i++].vehicleNum;

            if (tdt.id.empty()) continue;

            if (m_TrustDataTableM.find(node.Id) == m_TrustDataTableM.end())
            {
                m_TrustDataTableM[node.Id] = tdt;
                LOG4CPLUS_INFO("root", node.Id << " The Update of TrustDataTableMap");
            }

            if (!flag) continue;

            key_value["Id"] = node.Id;
            m_MongoDb->mongoQueryMatching<std::map<std::string,std::string>>("gsafety_vehicle", key_value, ret);

            if (ret.size())
            {
                LOG4CPLUS_INFO("root", "vehicle TrustList database already exists: " << node.Id);
                continue;
            }

            /** @todo 转换mongogdb json格式 */
            m_DataAnalysis->OutTrustDataSerialize(msg, tdt, I_ADDVEHICLE);

            if (!m_MongoDb->mongoInsertMany("gsafety_vehicle", msg))
            {
                LOG4CPLUS_ERROR("root", "The TrustData added to the database fail:\n" << msg);
                continue;
            }
            LOG4CPLUS_ERROR("root", "The TrustData added to the database Success:\n" << msg);
        }
    }
}