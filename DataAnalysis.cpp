#include "DataAnalysis.hpp"

DataAnalysis::DataAnalysis()
{
    m_pLogger = GsafetyLog::GetInstance();
}

DataAnalysis::~DataAnalysis()
{
    /** don't release source */
}

std::string DataAnalysis::DotConvertName(enum DOT dot)
{
    std::string name;
    switch(dot)
    {
        case D_ADD :
            name = "ADD"; break;
        case D_AMODIFY :
            name = "Algorithm Modify"; break;
        case D_VMODIFY :
            name = "Video Modify"; break;
        case D_DEL :
            name = "Delete"; break;
        case D_STATUS :
            name = "Status"; break;
        case D_NOTICE :
            name = "Notice"; break;
        case D_RUN_STATUS :
            name = "Program running status"; break;
        case D_IDEL :
            name = "Internal Delete"; break;
        default :
            name = "unknown Operation";
    }
    return name;
}

std::string DataAnalysis::AITypeConvertName(uint32_t ai_type)
{
    std::string name;
    switch(ai_type)
    {
        case FIRE :
            name = "烟火隐患-"; break;
        case HELMET :
            name = "未佩戴安全帽-"; break;
        case PERSION :
            name = "人员在岗-"; break;
        case FACE :
            name = "人脸特征检测-"; break;
        case VEHICLE :
            name = "非法车辆-"; break;
        case WELDING :
            name = "动火作业-"; break;
        case REFLECTIVE_CLOSTHES :
            name = "未穿戴反光衣-"; break;
        case BLET_DEVIATION :
            name = "传输带偏离预值-"; break;
        case CROWD_DENSITY :
            name = ""; break;
        case CROWD_FLOW :
            name = ""; break;
        default :
            name = "未支持类别-";
    }
    return name;
}

void DataAnalysis::Split(const std::string& iLine, const char* iSymbol, CRect& rect)
{
    char *pSlice= strtok(const_cast<char*>(iLine.c_str()), iSymbol);
	if (pSlice)
        rect.x = atoi(pSlice);
	pSlice = strtok(NULL, iSymbol);
    if (pSlice)
        rect.y = atoi(pSlice);
	pSlice = strtok(NULL, iSymbol);
    if (pSlice)
        rect.width = atoi(pSlice);
	pSlice = strtok(NULL, iSymbol);
    if (pSlice)
        rect.height = atoi(pSlice);
}

void DataAnalysis::Split(const std::string& iLine, const char* iSymbol, std::vector<std::string>& oVecWord)
{
    char *pSlice= strtok(const_cast<char*>(iLine.c_str()), iSymbol);
	while (pSlice)
	{
		oVecWord.push_back(pSlice);
		pSlice = strtok(NULL, iSymbol);
	}
}

bool DataAnalysis::InstructMatch(const InstructionInfo& instruct)
{
    //添加指令操作类型判断,为后期指定算法操作过滤做准备
    if (STATUS == instruct.iot() || NOTICE == instruct.iot())
        return false;

    if (instruct.algorithmid().compare(m_BaseData.AlgorithmId))
        return false;

    if (instruct.has_containerid() && instruct.containerid().compare(m_BaseData.ContainerID))
        return false;
    
    return true;
}

bool DataAnalysis::OutDataDeserialize(const std::string& msg, InstructionData& instruction_data)
{
    uint64_t instruct_time;
    InstructionInfo instruct;
    
    if (!instruct.ParseFromString(msg))
        return false;

    instruction_data.dot = (DOT)instruct.iot();
    instruction_data.TimeStamp = instruct.timestamp();
    instruction_data.CurrentId = instruct.currentid();
    
    LOG4CPLUS_INFO("root", "InstructInfo AlgorithmID: " << instruct.algorithmid());
    if (!InstructMatch(instruct))
    {
        instruction_data.CurrentId = instruct.containerid();
        return false;
    }

    //填充摄像头数据
    for(int i = 0; i < instruct.camerainfos_size(); i++)
    {
        CameraData camera_data;

        camera_data.CurrentId = instruct.currentid();

        camera_data.dot = (DOT)instruct.iot();
        if (camera_data.dot == D_AMODIFY)
            break;

        CameraInfo psn = instruct.camerainfos(i);
        if (!psn.has_cameraid() || !psn.has_cameraurl())
        {
            camera_data.Status = LOSEFIELD;
            camera_data.AvailableStatus = false;
        }
        else
        {
            camera_data.Status = SUCCESS;
            camera_data.AvailableStatus = true;
            camera_data.CameraId = psn.cameraid();
            camera_data.CameraUrl = psn.cameraurl();
        }

        if (psn.has_cameraname())
            camera_data.CameraName = psn.cameraname();
            
        if (psn.has_pushflow() && psn.has_desturl())
        {
            camera_data.PushFlow = psn.pushflow();
            camera_data.DestUrl = psn.desturl();
        }
        else
        {
            camera_data.PushFlow = false;
            //camera_data.PushFlow = psn.pushflow();
        }
        if (psn.has_skipframefrequency())
            camera_data.SkipFrameFrequency = psn.skipframefrequency();
        else
            camera_data.SkipFrameFrequency = 5;
        if (psn.has_width())
            camera_data.Width = psn.width();
        if (psn.has_height())
            camera_data.Height = psn.height();
        if (psn.has_cascade())
            camera_data.Cascade = psn.cascade();
        if (psn.has_endtimestamp())
            camera_data.EndTimeStamp = psn.endtimestamp();
        else
            camera_data.EndTimeStamp = 0;
        if (psn.has_savevideoduration())
            camera_data.SaveVideoDuration = psn.savevideoduration();
        else
            camera_data.SaveVideoDuration = 60;

        camera_data.ActiveStatus = false;
        instruction_data.CameraDataM[camera_data.CameraId] = camera_data;
    }

    //填充相关算法数据
    for(int i = 0; i < instruct.aparms_size(); i++)
    {
        AlgorithmData algorithm_data;

        algorithm_data.dot = (DOT)instruct.iot();
        if (algorithm_data.dot == D_VMODIFY)
            break;

        AlgorithmParm psn = instruct.aparms(i);

        //必须要有对应的摄像头id
        if (!psn.has_cameraid())
        {
            algorithm_data.AvailableStatus = false;
            algorithm_data.Status = LOSEFIELD;
        }
        else
        {
            algorithm_data.Status = SUCCESS;
            algorithm_data.AvailableStatus = true;
            algorithm_data.AlgorithmId = psn.cameraid();
        }

        algorithm_data.CurrentId = instruct.currentid();

        if (psn.has_batchsize())
            algorithm_data.BatchSize = psn.batchsize();
        else
            algorithm_data.BatchSize = 1;
        if (psn.has_nms())
            algorithm_data.Nms = psn.nms();
        else
            algorithm_data.Nms = 0.5;
        if (psn.has_threshold())
            algorithm_data.Threshold = psn.threshold();
        else
            algorithm_data.Threshold = 0.6;
        if (psn.has_alarminterval())
            algorithm_data.AlarmInterval = psn.alarminterval();
        else
            algorithm_data.AlarmInterval = 60;  
        if (psn.has_resolution())
            algorithm_data.Resolution = psn.resolution();
        else
            algorithm_data.Resolution = 680;
        if (psn.has_leavetimelimit())
            algorithm_data.LeaveTimeLimit = psn.leavetimelimit();
        else
            algorithm_data.LeaveTimeLimit = 600;
        if (psn.has_personnels())
            algorithm_data.Personnels = psn.personnels();
        else
            algorithm_data.Personnels = 1000;
        if (psn.has_trustlist())
            algorithm_data.TrustList = psn.trustlist();
        else
            algorithm_data.TrustList = 3;  //不使用信任名单
        //填充视频框区域信息
        if (psn.has_region())
        {
            if (psn.region().has_height())
                algorithm_data.Region.heigth = psn.region().height();
            if (psn.region().has_width())
                algorithm_data.Region.width = psn.region().width();
            if (psn.region().has_interesttype())
                algorithm_data.Region.interestType = psn.region().interesttype();
            if (psn.region().has_shutdownarea())
                algorithm_data.Region.shutDownArea = psn.region().shutdownarea();
            if (psn.region().has_thresholdarea())
                algorithm_data.Region.thresholdArea = psn.region().thresholdarea();
            for(int i = 0; i < psn.region().allpoints_size(); i++)
            {
                InterPoint inter_point;
                const InteractionPoint& ipoint = psn.region().allpoints(i);
                inter_point.horizontal = ipoint.horizontal();
                inter_point.ordinate = ipoint.ordinate();
                algorithm_data.Region.allPoints.push_back(inter_point);
            }
        }
        
        algorithm_data.ActiveStatus = false;
        instruction_data.AlgorithmDataM[algorithm_data.AlgorithmId] = algorithm_data;
    }

    LOG4CPLUS_INFO("root", "Out Instruction Parsing complete: " << instruct.currentid());
    return true;
}

bool DataAnalysis::InDataSerialize(void* metadata, std::string& msg, int type)
{  
    Json::Value root;
    Json::Value arrayObj;
    Json::StreamWriterBuilder writerBuilder;
    std::ostringstream os;

    if (type == CODEC)
    {
        /** @todo 编解码结构化数据处理 */
        CameraData* pcd = (CameraData*)metadata;
        root["dot"] = pcd->dot;
        root["DestUrl"] = pcd->DestUrl;
        root["Cascade"] = pcd->Cascade;
        root["PushFlow"] = pcd->PushFlow;
        root["CameraId"] = pcd->CameraId;
        root["CameraUrl"] = pcd->CameraUrl;
        root["CurrentId"] = pcd->CurrentId;
        root["CameraName"] = pcd->CameraName;
        root["EndTimeStamp"] = pcd->EndTimeStamp;
        root["SaveVideoDuration"] = pcd->SaveVideoDuration;
        root["SkipFrameFrequency"] = pcd->SkipFrameFrequency;
        LOG4CPLUS_INFO("root", "Codec Data Compile Complete: " << pcd->CameraId);
    }
    else if (type == ALGORITHM)
    {
        /** @todo 算法结构化数据处理 */
        AlgorithmData* pad = (AlgorithmData*)metadata;
        root["dot"] = pad->dot;
        root["Nms"] = pad->Nms;
        root["CurrentId"] = pad->CurrentId;
        root["AlgorithmId"] = pad->AlgorithmId;
        root["BatchSize"] = pad->BatchSize;
        root["Threshold"] = pad->Threshold;
        root["AlarmInterval"] = pad->AlarmInterval;
        root["Resolution"] = pad->Resolution;
        root["leavetimelimit"] = pad->LeaveTimeLimit;
        root["Personnels"] = pad->Personnels;
        root["TrustList"] = pad->TrustList;
        std::string region = "";
        AlgorithmRegionSerialize(region, pad->Region);
        root["Region"] = region;
        LOG4CPLUS_INFO("root", "Algorithm Data Compile Complete: " << pad->AlgorithmId);
    }
    else if (type == ANALYSIS)
    {
        /** @todo 依据春慧是否需要性能分析数据 */
        LOG4CPLUS_INFO("root", "Analysis Data Compile Complete: ????");
    }
    else return false;  //其他类型返回错误
#if 1
    //json格式,转换的字节数比非json格式的多1/3左右
    std::unique_ptr<Json::StreamWriter> jsonWriter(writerBuilder.newStreamWriter());
    jsonWriter->write(root, &os);
    msg = os.str();
#else
    Json::FastWriter jsonWriter;  //非json格式
    msg = jsonWriter.write(root);
#endif
    return true;
}

void DataAnalysis::OutDataSerialize(const InstructionData& instruction_data, 
        std::string& msg, DATASTATE status)
{
    uint64 instruct_time;
    InstructionInfo instruct;
    
    std::time_t t = std::time(0);
    instruct.set_timestamp(t);

    /** @brief 算法报警数据处理分支 */
    if (D_NOTICE & instruction_data.dot)
    {
        /** @brief 非报警算法类型 使用http服务提供能力 */

        AlarmInfo alarm_info;
        alarm_info.set_algorithmid(m_BaseData.AlgorithmId);
        alarm_info.set_containerid(m_BaseData.ContainerID);
        alarm_info.set_algorithmtype(m_BaseData.AlgorithmType);
        //alarm_info.set_algorithmtype(instruction_data.AlarmData.AlgorithmType);
        alarm_info.set_alarmid(instruction_data.AlarmData.AlarmId);
        alarm_info.set_cameraid(instruction_data.AlarmData.CameraId);
        alarm_info.set_cameraname(instruction_data.AlarmData.CameraName);
        alarm_info.set_videopath(instruction_data.AlarmData.VideoPath);
        alarm_info.set_imagepath(instruction_data.AlarmData.ImagePath);
        alarm_info.set_timestamp(instruction_data.AlarmData.TimeStamp);
        alarm_info.set_currentid(instruction_data.AlarmData.CurrentId);

        for(auto& node : instruction_data.AlarmData.Aresult)
        {
            FRect* frect = alarm_info.add_rects();
            frect->set_x(node.rect.x);
            frect->set_y(node.rect.y);
            frect->set_width(node.rect.width);
            frect->set_height(node.rect.height);
            frect->set_confidence(node.Confidence);
        }
        alarm_info.SerializeToString(&msg);

        LOG4CPLUS_INFO("root", "Alarm Compile Complete: " << instruction_data.AlarmData.AlarmId);
        return;
    }

    instruct.set_iot((IOT)instruction_data.dot);
    instruct.set_currentid(instruction_data.CurrentId);
    instruct.set_algorithmid(m_BaseData.AlgorithmId);
    instruct.set_containerid(m_BaseData.ContainerID);
    instruct.set_algorithmtype(m_BaseData.AlgorithmType);
    
    instruct.set_status(status);

    //填充摄像头数据
    CameraDataMap::const_iterator citer = instruction_data.CameraDataM.begin();
    for(; citer != instruction_data.CameraDataM.end(); citer++)
    {
        CameraInfo* caminfo;
        caminfo = instruct.add_camerainfos();
        caminfo->set_cameraid(citer->second.CameraId);
        caminfo->set_status(citer->second.Status);
    }

    AlgorithmDataMap::const_iterator aiter = instruction_data.AlgorithmDataM.begin();
    for(; aiter != instruction_data.AlgorithmDataM.end(); aiter++)
    {
        AlgorithmParm* alparm;
        alparm = instruct.add_aparms();
        alparm->set_cameraid(aiter->second.AlgorithmId);
        if (instruction_data.dot & D_AMODIFY && aiter->second.dot & D_DEL)
            alparm->set_status(ALGORITHDELETE);
        else
            alparm->set_status(aiter->second.Status);
    }

    instruct.SerializeToString(&msg);
    LOG4CPLUS_INFO("root", "Out Instruction Compile Complete: " << instruction_data.CurrentId);
}

bool DataAnalysis::InDataDeserialize(const std::string& msg, InstructionData& inst_data)
{
    Json::Value root;
    Json::CharReaderBuilder readerBuilder;

    std::unique_ptr<Json::CharReader> const jsonReader(readerBuilder.newCharReader());

    if (!jsonReader->parse(msg.c_str(), msg.c_str() + msg.length(), &root, nullptr))
        return false;

    if (ALARMA == (METADATATYPE)root["Type"].asInt() || ALARMC == (METADATATYPE)root["Type"].asInt())
    {
        inst_data.dot = D_NOTICE;
        inst_data.AlarmData.CurrentId = root["TaskId"].asString();
        inst_data.AlarmData.CameraId = root["CameraId"].asString();
        inst_data.AlarmData.AlarmId = root["AlarmId"].asString();
        inst_data.AlarmData.Type = (METADATATYPE)root["Type"].asInt();
        if (root.isMember("ImagePath"))
            inst_data.AlarmData.ImagePath = root["ImagePath"].asString();
        if (root.isMember("VideoPath"))
            inst_data.AlarmData.VideoPath = root["VideoPath"].asString();
        if (root.isMember("TimeStamp"))
            inst_data.AlarmData.TimeStamp = root["TimeStamp"].asInt64();
        if (root.isMember("AlgorithmType"))
            inst_data.AlarmData.AlgorithmType = root["AlgorithmType"].asUInt();
        if (root.isMember("CameraName"))
            inst_data.AlarmData.CameraName = root["CameraName"].asString();
        for(int i = 0; i < root["Aresult"].size(); i++)
        {
            AlgorithmResult algor_result;
            algor_result.rect.x = root["Aresult"][i]["x"].asInt();
            algor_result.rect.y = root["Aresult"][i]["y"].asInt();
            algor_result.rect.width = root["Aresult"][i]["width"].asInt();
            algor_result.rect.height = root["Aresult"][i]["height"].asInt();
            algor_result.Confidence = root["Aresult"][i]["Confidence"].asDouble();
            if (root["Aresult"][i].isMember("vehicleNum"))
                algor_result.vehicleNum = root["Aresult"][i]["vehicleNum"].asDouble();

            inst_data.AlarmData.Aresult.emplace_back(std::move(algor_result));
        }
        LOG4CPLUS_INFO("root", "Alarm Parsing complete: " <<  inst_data.AlarmData.AlarmId);
        return true;
    }

    inst_data.TimeStamp = std::time(0);
    
    inst_data.dot = (DOT)root["dot"].asInt();

    if (D_IDEL == inst_data.dot) inst_data.dot = D_DEL;
    
    inst_data.CurrentId = root["CurrentId"].asString();
    if (CODEC == (METADATATYPE)root["Type"].asInt())
    {
        CameraData cd;
        std::string id = root["CameraId"].asString();
        cd.CameraId = id;
        cd.dot = (DOT)root["dot"].asInt();
        cd.Status = (DATASTATE)root["Status"].asInt();
        inst_data.CameraDataM[id] = cd;
    }
    else if (ALGORITHM == (METADATATYPE)root["Type"].asInt())
    {
        AlgorithmData ad;
        std::string id = root["AlgorithmId"].asString();
        ad.AlgorithmId = id;
        ad.dot = (DOT)root["dot"].asInt();
        ad.Status = (DATASTATE)root["Status"].asInt();
        inst_data.AlgorithmDataM[id] = ad;
    }
    LOG4CPLUS_INFO("root", "In Instruction Parsing complete: " <<  inst_data.CurrentId);
    return true;
}

void DataAnalysis::OutRunStateSerialize(std::string& msg, volatile bool flag)
{
    InstructionInfo instruct;

    instruct.set_containerid(m_BaseData.ContainerID);
    instruct.set_iot(RUN_STATUS);

    instruct.set_currentid(m_BaseData.CurrentId);

    std::time_t t = std::time(0);
    instruct.set_timestamp(t);

    //程序状态判断,后面与平台确认具体值
    if (flag)
        instruct.set_status(SUCCESS);
    else
        instruct.set_status(FAILED);
    
    instruct.set_algorithmid(m_BaseData.AlgorithmId);
    instruct.set_algorithmtype(m_BaseData.AlgorithmType);

    instruct.SerializeToString(&msg);
}

// std::string DataAnalysis::FromRegionToString(const CRect& rect) = delete
// {
//     std::string region = std::to_string(rect.x)+",";
//     region += std::to_string(rect.y)+",";
//     region += std::to_string(rect.width)+",";
//     region += std::to_string(rect.height);
//     return region;
// }

bool DataAnalysis::InDataDeserialize(const string &msg, CameraData& cameraData, AlarmResult& alarmData, int& type)
{
    Json::Value root;
    Json::CharReaderBuilder readerBuilder;
    std::unique_ptr<Json::CharReader> const jsonReader(readerBuilder.newCharReader());

    if (!jsonReader->parse(msg.c_str(), msg.c_str() + msg.length(), &root, nullptr))
        return false;
    
    if (root.isMember("AlarmId"))
    {
        type = ALARMC;
        alarmData.AlarmId = root["AlarmId"].asString();
        alarmData.CameraId = root["CameraId"].asString();
    }
    else
    {
        type = CODEC;
        cameraData.dot = static_cast<DOT>(root["dot"].asUInt());
        cameraData.Cascade = root["Cascade"].asBool();
        cameraData.PushFlow = root["PushFlow"].asBool();
        cameraData.DestUrl = root["DestUrl"].asString();
        cameraData.CurrentId = root["CurrentId"].asString();
        cameraData.CameraName = root["CameraName"].asString();
        cameraData.CameraId = root["CameraId"].asString();
        cameraData.CameraUrl = root["CameraUrl"].asString();
        cameraData.EndTimeStamp = root["EndTimeStamp"].asUInt64();
        cameraData.SaveVideoDuration = root["SaveVideoDuration"].asUInt();
        cameraData.SkipFrameFrequency = root["SkipFrameFrequency"].asUInt();
    }

    return true;
}

bool DataAnalysis::InDataSerialize(string &msg, const CameraData &cameraData,
    const AlarmResult& alarmData, int type)
{
    Json::Value root;
    Json::StreamWriterBuilder writerBuilder;
    std::ostringstream os;

    if (ALARMC == type)
    {
        root["Type"] = ALARMC;
        root["AlarmId"] = alarmData.AlarmId;
        root["CameraId"] = alarmData.CameraId;
        root["CameraName"] = alarmData.CameraName;
        root["TaskId"] = alarmData.CurrentId;
        root["VideoPath"] = alarmData.VideoPath;
    }
    else if (CODEC == type)
    {
        root["dot"] = cameraData.dot;
        root["Type"]= CODEC;
        root["Status"]= cameraData.Status;
        root["CurrentId"]= cameraData.CurrentId;
        root["CameraId"]= cameraData.CameraId;
        root["CameraName"]= cameraData.CameraName;
    }

    std::unique_ptr<Json::StreamWriter> jsonWriter(writerBuilder.newStreamWriter());
    jsonWriter->write(root, &os);
    msg = os.str();
}

bool DataAnalysis::OutBaseDataDeserialize(const std::string& msg, BaseData& bd)
{
    bool _status = false;
    JSONCPP_STRING errs;
    Json::Value root;
    Json::CharReaderBuilder readerBuilder;
    std::unique_ptr<Json::CharReader> const jsonReader(readerBuilder.newCharReader());

    if (jsonReader->parse(msg.c_str(), msg.c_str()+msg.length(), &root, &errs))
    {
        if (root.isMember("ContainerID"))
        {
            std::string cid = root["ContainerID"].asString();
            if (bd.ContainerID.compare(cid))
                return _status;
        }
        if (root["_id"].isMember("$oid"))
            bd.CurrentId = root["_id"]["$oid"].asString();
        if (root.isMember("KafkaServer"))
            bd.kafkaHost = root["KafkaServer"].asString();
        if (root.isMember("HttpServer"))
            bd.HttpHost = root["HttpServer"].asString();
        if (root.isMember("HttpPeerHost"))
            bd.HttpPeerHost = root["HttpPeerHost"].asString();
        if (root.isMember("MsgBrokerHost"))
            bd.MsgBrokerHost = root["MsgBrokerHost"].asString();
        if (root.isMember("EnableHttpServer"))
            bd.EnableHttpServer = root["EnableHttpServer"].asBool();
        if (root.isMember("UsePrivateLib"))
            bd.UsePrivateLib = root["UsePrivateLib"].asBool();
        if (root.isMember("StorageServer"))
            bd.StorageServer = root["StorageServer"].asString();
        if (root.isMember("AlgorithmType"))
            bd.AlgorithmType = root["AlgorithmType"].asUInt();
        if (root.isMember("KafkaStatusPubTopic"))
            bd.KafkaStatusPubTopic = root["KafkaStatusPubTopic"].asString();
        if (root.isMember("KafkaAlarmPubTopic"))
            bd.KafkaAlarmPubTopic = root["KafkaAlarmPubTopic"].asString();
        if (root.isMember("KafkaAnalysisPubTopic"))
            bd.KafkaAnalysisPubTopic = root["KafkaAnalysisPubTopic"].asString();
        if (root.isMember("KafkaSubTopic"))
        {
            Split(root["KafkaSubTopic"].asString(), ",", bd.kafkaSubTopicName);
        }
        //m_BaseData = bd;
        _status = true;
    }
    else LOG4CPLUS_ERROR("root", bd.AlgorithmId << " : " << errs);

    return _status;
}

bool DataAnalysis::OutContainerDataDeserialize(const std::string& msg, CameraData& cd, AlgorithmData& ad)
{
    JSONCPP_STRING errs;
    Json::Value root;
    Json::CharReaderBuilder readerBuilder;
    std::unique_ptr<Json::CharReader> const jsonReader(readerBuilder.newCharReader());
    if (jsonReader->parse(msg.c_str(), msg.c_str()+msg.length(), &root, &errs))
    {
        if (root.isMember("CameraId"))
        {
            cd.CameraId = root["CameraId"].asString();
            ad.AlgorithmId = root["AlgorithmId"].asString();
        }
        if (root.isMember("CameraStatus"))
        {
            cd.AvailableStatus = root["CameraStatus"].asBool();
            ad.AvailableStatus = cd.AvailableStatus;
        }
        if (root.isMember("AlgorithmAlarmInterval"))
            ad.AlarmInterval = root["AlgorithmAlarmInterval"].asInt64();
        else
            ad.AlarmInterval = 60;     //如果库中没有数据默认使用
        if (root.isMember("AlgorithmPersonnels"))
            ad.Personnels = root["AlgorithmPersonnels"].asInt();
        else
            ad.Personnels = 1000;     //如果库中没有数据默认使用
        if (root.isMember("AlgorithmTrustList"))
            ad.TrustList = root["AlgorithmTrustList"].asInt();
        else
            ad.TrustList = 3;         //如果库中没有数据默认不使用 
        if (root.isMember("AlgorithmNms"))
            ad.Nms = root["AlgorithmNms"].asDouble();
        if (root.isMember("AlgorithmThreshold"))
            ad.Threshold = root["AlgorithmThreshold"].asDouble();
        if (root.isMember("AlgorithLeaveTimeLimit"))
            ad.LeaveTimeLimit = root["AlgorithLeaveTimeLimit"].asUInt();
        if (root.isMember("AlgorithmBatchSize"))
            ad.BatchSize = root["AlgorithmBatchSize"].asUInt();
        if (root.isMember("AlgorithmResolution"))
            ad.Resolution = root["AlgorithmResolution"].asUInt();
        if (root.isMember("AlgorithmRegion"))
            AlgorithmRegionDeserialize(root["AlgorithmRegion"].asString(), ad.Region);
        //Split(root["AlgorithmRegion"].asString(), ",", ad.Region);
        if (root.isMember("CameraName"))
            cd.CameraName = root["CameraName"].asString();
        if (root.isMember("CameraUrl"))
            cd.CameraUrl = root["CameraUrl"].asString();
        if (root.isMember("Cascade"))
            cd.Cascade = root["Cascade"].asBool();
        if (root.isMember("DestUrl"))
            cd.DestUrl = root["DestUrl"].asString();
        if (root.isMember("PushFlow"))
            cd.PushFlow = root["PushFlow"].asBool();
        if (root.isMember("Height"))
            cd.Height = root["Height"].asUInt();
        if (root.isMember("Width"))
            cd.Height = root["Width"].asUInt();
        if (root.isMember("EndTimeStamp"))
            cd.EndTimeStamp = root["EndTimeStamp"].asUInt64();
        else
            cd.EndTimeStamp = 0;        //如果库中没有数据默认使用
        if (root.isMember("SaveVideoDuration"))
            cd.SaveVideoDuration = root["SaveVideoDuration"].asUInt();
        else
            cd.SaveVideoDuration = 60;  //告警保存视频时长
        if (root.isMember("SkipFrameFrequency"))
            cd.SkipFrameFrequency = root["SkipFrameFrequency"].asUInt();
        else
            cd.SkipFrameFrequency = 5;  //如果库中没有数据默认使用

        //初始化即添加状态
        cd.dot = ad.dot = D_ADD;
        cd.ActiveStatus = ad.ActiveStatus = false;

        //启动暂时不用发送执行状态
        if (root.isMember("TaskID"))
            cd.CurrentId = root["TaskID"].asString();
        else
            cd.CurrentId = root["_id"]["$oid"].asString();
        ad.CurrentId = cd.CurrentId;
        return true;
    }
    else
    {
        LOG4CPLUS_ERROR("root", errs);
        return false;
    }
}

void DataAnalysis::OutContainerDataSerialize(std::string& msg, const CameraData& cd, const AlgorithmData& ad)
{
    std::string region = "";
    Json::Value root;
    Json::StreamWriterBuilder writerBuilder;
    std::ostringstream os;
    root["ContainerID"] = m_BaseData.ContainerID;
    root["TaskID"] = cd.CurrentId;
    root["CameraStatus"] = cd.AvailableStatus;
    root["CameraId"] = cd.CameraId;
    root["CameraName"] = cd.CameraName;
    root["CameraUrl"] = cd.CameraUrl;
    root["Cascade"] = cd.Cascade;
    root["DestUrl"] = cd.DestUrl;
    root["Height"] = (uint32_t)cd.Height;
    root["PushFlow"] = cd.PushFlow;
    root["Width"] = (uint32_t)cd.Width;
    root["SaveVideoDuration"] = (uint32_t)cd.SaveVideoDuration;
    root["SkipFrameFrequency"] = (uint32_t)cd.SkipFrameFrequency;
    root["EndTimeStamp"] = (uint64_t)cd.EndTimeStamp;
    root["AlgorithmId"] = ad.AlgorithmId;
    root["AlgorithmNms"] = (double)ad.Nms;
    root["AlgorithmThreshold"] = (double)ad.Threshold;
    root["AlgorithmBatchSize"] = (uint32_t)ad.BatchSize;
    root["AlgorithmAlarmInterval"] = (uint64_t)ad.AlarmInterval;
    root["AlgorithmResolution"] = (uint32_t)ad.Resolution;
    root["AlgorithLeaveTimeLimit"] = (uint32_t)ad.LeaveTimeLimit;
    root["AlgorithmPersonnels"] = (int32_t)ad.Personnels;
    root["AlgorithmTrustList"] = (uint32_t)ad.TrustList;
    //root["AlgorithmRegion"] = m_DataAnalysis->FromRegionToString(ad.Region);
    AlgorithmRegionSerialize(region, ad.Region);
    root["AlgorithmRegion"] = region;

    //json格式,转换的字节数比非json格式的多1/3左右
    std::unique_ptr<Json::StreamWriter> jsonWriter(writerBuilder.newStreamWriter());
    jsonWriter->write(root, &os);
    msg = os.str();
}

void DataAnalysis::AlgorithmRegionDeserialize(const std::string& msg, InterArea& ia)
{
    JSONCPP_STRING errs;
    Json::Value root;
    Json::CharReaderBuilder readerBuilder;
    std::unique_ptr<Json::CharReader> const jsonReader(readerBuilder.newCharReader());
    try 
    {
        if (jsonReader->parse(msg.c_str(), msg.c_str()+msg.length(), &root, &errs))
        {
            if (root.isMember("width"))
                ia.width = root["width"].asInt();
            if (root.isMember("heigth"))
                ia.heigth = root["heigth"].asInt();
            if (root.isMember("interestType"))
                ia.interestType = root["interestType"].asInt();
            if (root.isMember("shutDownArea"))
                ia.shutDownArea = root["shutDownArea"].asBool();
            if (root.isMember("thresholdArea"))
                ia.thresholdArea = root["thresholdArea"].asFloat();
            if (root.isMember("allPoints"))
            {
                for (int i = 0; i < root["allPoints"].size(); i++)
                {
                    InterPoint inter_point;
                    if (root["allPoints"][i].isMember("horizontal"))
                        inter_point.horizontal = root["allPoints"][i]["horizontal"].asFloat();
                    if (root["allPoints"][i].isMember("ordinate"))
                        inter_point.ordinate = root["allPoints"][i]["ordinate"].asFloat();
                    ia.allPoints.push_back(inter_point);
                }
            }
        }
    }
    catch(Json::LogicError& err)
    {
        //不使用区域框数据
        ia.shutDownArea = false;
        LOG4CPLUS_ERROR("root", "Region parse: " << err.what());
    }
}

void DataAnalysis::AlgorithmRegionSerialize(std::string& msg, const InterArea& ia)
{
    Json::Value root;
    Json::StreamWriterBuilder writerBuilder;
    std::ostringstream os;

    root["width"] = ia.width;
    root["heigth"] = ia.heigth;
    root["interestType"] = ia.interestType;
    root["shutDownArea"] = ia.shutDownArea;
    root["thresholdArea"] = ia.thresholdArea;
    for (int i = 0; i < ia.allPoints.size(); i++)
    {
        root["allPoints"][i]["horizontal"] = ia.allPoints[i].horizontal;
        root["allPoints"][i]["ordinate"] = ia.allPoints[i].ordinate;
    }

    std::unique_ptr<Json::StreamWriter> jsonWriter(writerBuilder.newStreamWriter());
    jsonWriter->write(root, &os);
    msg = os.str();
}

void DataAnalysis::OutTrustDataSerialize(std::string& msg, const TrustDataTable& tdt, IDOT idot)
{
    Json::Value root;
    Json::StreamWriterBuilder writerBuilder;
    std::ostringstream os;

    root["Id"] = tdt.id;
    if (I_ADDFACE == idot)
    {
        FRect frect;
        std::string features;
        root["FaceName"] = tdt.name;
        root["Department"] = tdt.department;

        for(int i = 0; i < _FEATURESIZE; i++)
        {
            ostringstream oss;
            oss.precision(10);//覆盖默认精度
            oss << tdt.feature(0,i);
            features += oss.str();
            features += ",";
        }
        features.erase(features.end()-1);
        root["FaceFeature"] = features;

    }
    else if (I_ADDVEHICLE == idot)
    {
        root["Owner"] = tdt.name;
        root["Company"] = tdt.department;
        root["Vehicle"] = tdt.vehicle;
    }
    
    root["Trust"] = tdt.trust;
   
    std::unique_ptr<Json::StreamWriter> jsonWriter(writerBuilder.newStreamWriter());
    jsonWriter->write(root, &os);
    msg = os.str();
}

bool DataAnalysis::OutTrustDataDeserialize(const std::string& msg, TrustDataTableMap& tdt_map)
{
    JSONCPP_STRING errs;
    Json::Value root;
    Json::CharReaderBuilder readerBuilder;
    std::unique_ptr<Json::CharReader> const jsonReader(readerBuilder.newCharReader());
    if (jsonReader->parse(msg.c_str(), msg.c_str()+msg.length(), &root, &errs))
    {
        TrustDataTable tdt;
        std::string oid = root["Id"].asString();
        if (root.isMember("FaceName"))
            tdt.name = root["FaceName"].asString();
        if (root.isMember("Owner"))
            tdt.name = root["Owner"].asString();
        if (root.isMember("Company"))
            tdt.department = root["Company"].asString();
        if (root.isMember("Department"))
            tdt.department = root["Department"].asString();
        if (root.isMember("Trust"))
            tdt.trust = root["Trust"].asUInt();
        if (root.isMember("Vehicle"))
            tdt.vehicle = root["Vehicle"].asString();
        if (root.isMember("FaceFeature"))
            ParseFeature(root["FaceFeature"].asString(), tdt.feature);
       
        //自动增加可读字段

        tdt.id = oid;
        tdt_map[oid] = tdt;

        LOG4CPLUS_INFO("root", "TrustData ID: " << oid);

        return true;
    }
    else
    {
        LOG4CPLUS_ERROR("root", errs);
        return false;
    }
}

void DataAnalysis::ParseFeature(const string &featureString, FEATUREM& feature)
{
	vector<string> featureVec;
    float tmp_feature[_FEATURESIZE];
	string featureSub;
	int pos = 0;
	int pos1 = featureString.find(",");
	while(pos1 != string::npos)
	{
		featureSub = featureString.substr(pos, pos1 - pos);
		featureVec.push_back(featureSub);
		pos = pos1 + 1;
		pos1 = featureString.find(',', pos);
	}

	int size = featureString.size();
	featureSub = featureString.substr(pos, size - pos -1);
	featureVec.push_back(featureSub);

	if(featureVec.size() != _FEATURESIZE)
		return;

	for(size_t i = 0; i < featureVec.size(); i++)
	{
		tmp_feature[i] = atof(featureVec[i].c_str());
	}

    memcpy(feature.data(), tmp_feature, _FEATURESIZE*sizeof(float));

	featureVec.clear();
}

void DataAnalysis::InternalDataSetSerialize(std::string& msg, const InternalDataSet& ids)
{
    InternalDataSetInfo internal_data;

    internal_data.set_iiot((VideoEngine2::IIOT)ids.idot);
    internal_data.set_currentid(ids.CurrentId);
    internal_data.set_trustflag(ids.TrustFlag);
    internal_data.set_cameraid(ids.CameraId);
    internal_data.set_cameraname(ids.CameraName);
    internal_data.set_algorithmtype(ids.AlgorithmType);
    internal_data.set_alarminterval(ids.AlarmInterval);

    std::time_t t = std::time(0);
    internal_data.set_timestamp(t);

    if (I_FACERESULT_HTTP == ids.idot || I_FACERESULT_VIDEO == ids.idot)
    {
        //填充人脸数据
        internal_data.set_trustflag(ids.TrustFlag);
        for (auto& dbv : ids.DetectBoxV)
        {
            FRect* FRect = internal_data.add_detectinfo();
            FRect->set_confidence(dbv.Confidence);
            FRect->set_x(dbv.rect.x); FRect->set_y(dbv.rect.y);
            FRect->set_width(dbv.rect.width); FRect->set_height(dbv.rect.height);
            FRect->set_feature((const void*)dbv.feature.data(), _FEATURESIZE*sizeof(float));
        }
        for (auto& fiv : ids.AdditionInfoV)
        {
            AdditionalInfo *Finfo = internal_data.add_additioninfo();
            Finfo->set_id(fiv.Id);
            Finfo->set_name(fiv.Name);
            Finfo->set_department(fiv.Department);
        }
    }
    if (I_VEHICLERESULT_HTTP == ids.idot || I_VEHICLERESULT_VIDEO == ids.idot)
    {
        //填充车辆数据
        internal_data.set_trustflag(ids.TrustFlag);
        for (auto& dbv : ids.DetectBoxV)
        {
            FRect* FRect = internal_data.add_detectinfo();
            FRect->set_confidence(dbv.Confidence);
            FRect->set_x(dbv.rect.x); FRect->set_y(dbv.rect.y);
            FRect->set_width(dbv.rect.width); FRect->set_height(dbv.rect.height);
            FRect->set_vehicle(dbv.vehicleNum);
            FRect->set_trackerid(dbv.trackerId);
        }
        for (auto& fiv : ids.AdditionInfoV)
        {
            AdditionalInfo *Finfo = internal_data.add_additioninfo();
            Finfo->set_id(fiv.Id);
            Finfo->set_name(fiv.Name);
            Finfo->set_department(fiv.Department);
        }
    }
    else if (I_CROWDDENSITY_VIDEO == ids.idot)
    {
        internal_data.set_personnelnum(ids.PersonnelNum);
    }

    /** @todo 填充图片数据 */
    //internal_data.set_mat(ids.mat.data);
    if (ids.mat.data)
    {
        std::string tmp;
        std::vector<unsigned char> buff;
        cv::imencode(".jpg", ids.mat, buff);
        tmp.resize(buff.size());
        memcpy(&tmp[0], buff.data(), buff.size());
        internal_data.set_mat(tmp);
    }

    internal_data.SerializeToString(&msg);
}

bool DataAnalysis::InternalDataSetDeserialize(const std::string& msg, InternalDataSet& ids)
{
    InternalDataSetInfo internal_data;

    if (!internal_data.ParseFromString(msg))
        return false;

    ids.idot = (IDOT)internal_data.iiot();

    if (ids.idot < 0x80) return false; //0x80-->IDOT最小值
    ids.CurrentId = internal_data.currentid();
    if (internal_data.has_personnelnum())
        ids.PersonnelNum = internal_data.personnelnum();
    if (internal_data.has_cameraid())
        ids.CameraId = internal_data.cameraid();
    if (internal_data.has_cameraname())
        ids.CameraName = internal_data.cameraname();
    if (internal_data.has_algorithmtype())
        ids.AlgorithmType = internal_data.algorithmtype();
    if (internal_data.has_alarminterval())
        ids.AlarmInterval = internal_data.alarminterval();
    else
        ids.AlarmInterval = 120; //默认2分钟
    if (internal_data.has_trustflag())
        ids.TrustFlag = internal_data.trustflag();
    else
        ids.TrustFlag = 3;  //非黑非白
    if (internal_data.has_timestamp())
        ids.TimeStamp = internal_data.timestamp();

    /** @todo 实现字符串转换Mat类型 */
    if (internal_data.has_mat())
    {
        cv::Mat mat2(1, internal_data.mat().size(), CV_8U, (char*)internal_data.mat().data());
        ids.mat = cv::imdecode(mat2, CV_LOAD_IMAGE_COLOR);
    }

    /** @todo 解析InternalDataSet.FaceInfo */
    for(int i = 0; i < internal_data.additioninfo_size(); i++)
    {
        AdditionalData face_data;
        const AdditionalInfo& face_info = internal_data.additioninfo(i);
        face_data.Id = face_info.id();
        if (face_info.has_name())
            face_data.Name = face_info.name();
        if (face_info.has_department())
            face_data.Department = face_info.department();
        ids.AdditionInfoV.push_back(face_data);
    }

    /** @todo 解析InternalDataSet.DetectBoxV */
    for(int i = 0; i < internal_data.detectinfo_size(); i++)
    {
        /**人脸特征修改为Eigen::Matrix结构(盼盼改进意见) */
        AlgorithmResult algorith_result;
        const FRect& res = internal_data.detectinfo(i);
        if (res.has_confidence())
            algorith_result.Confidence = res.confidence();
        if (res.has_vehicle())
            algorith_result.vehicleNum = res.vehicle();
        if (res.has_trackerid())
            algorith_result.trackerId = res.trackerid();

        algorith_result.rect.x = res.x();
        algorith_result.rect.y = res.y();
        algorith_result.rect.width = res.width();
        algorith_result.rect.height = res.height();

        if (res.has_feature())
        {
            const std::string& feature = res.feature();
            memcpy(algorith_result.feature.data(), feature.data(), _FEATURESIZE*sizeof(float));
        }
        
        ids.DetectBoxV.emplace_back(algorith_result);
    }
    return true;
}

void DataAnalysis::platformDataSetSerialize(string &msg, const HttpPlatFormData &httpPlatFormData)
{
    msg =  httpPlatFormData.buildUpMessage();
}

bool DataAnalysis::PlatformDataSetDeserialize(const string &msg, HttpPlatFormData &httpPlatFormData)
{
    bool result =  httpPlatFormData.parseMessage(msg);
    return result;
}