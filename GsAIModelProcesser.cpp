#include "GsAIModelProcesser.h"

GsAIModelProcesser::GsAIModelProcesser(ZeromqIndexDataMap& zero_index_dataM, TrustDataTableMap& trust_dataM) 
    : m_ZermqIndexDataM(zero_index_dataM), m_TrustDataTableM(trust_dataM),
    m_AlarmManager(std::make_shared<AlarmManager>(m_IndexMap, trust_dataM))
{
    m_spHelmetDetect=nullptr;
    m_spTrackIntegration = nullptr;
    m_spDenseSDK = nullptr;
    m_spFaceSDK = nullptr;
    m_spTrtDetector = nullptr;
    m_spPlateDetector = nullptr;
}

GsAIModelProcesser::~GsAIModelProcesser()
{
    // m_spFaceDetect=nullptr;
    m_spHelmetDetect=nullptr;
    m_spTrackIntegration = nullptr;
    m_spDenseSDK = nullptr;
    m_spFaceSDK = nullptr;
    m_spTrtDetector = nullptr;
    m_spPlateDetector = nullptr;
    if (vehicle_handle) VPlateRRT_Release(vehicle_handle);
    LOG4CPLUS_INFO("root", "AI source release!");
}

bool GsAIModelProcesser::initResource(AlgorithmData ad, std::shared_ptr<ZmqManager> zmq_mgmt,
        uint32_t alarm_type, std::string db_uri, bool use_private_library)
{
    m_DeviceId=0;
    m_classNum = -1;
    m_zmqIndex_task = m_ZermqIndexDataM["dad4668bc0b8e79cd460bf9f0e0a9700"].index;
    m_zmqIndex_pa = m_ZermqIndexDataM["0bdf2d9f480969c99e440756b1d85d86"].index;
    int zmqIndex_analysis = m_ZermqIndexDataM["bc8027fcb377a875a3329b7706cc5431"].index;
    m_zmqIndex_decode = m_ZermqIndexDataM[ad.AlgorithmId].index;

    m_privateLib = use_private_library;
    m_alarmType = alarm_type;
    m_DBHost = db_uri;

    /** @todo 加载人脸库信息,目标是实时渲染展示人脸信息 */

    m_PFastDfsManager = GsFastDfsManager::GetInstance();

    //算法id
    m_AlgorithmId=ad.AlgorithmId;
    //分辨率
    m_Resolution = ad.Resolution;

    std::string yoloModePath = "";
    getModePath(m_alarmType, yoloModePath);

    LOG4CPLUS_INFO("root", "AI Model Path: " << yoloModePath);
    
    m_nms=ad.Nms;
    m_conf=ad.Threshold;

    LOG4CPLUS_INFO("root", "Init AI Param { AlgorithmType: " << m_alarmType << " Threshold: " << ad.Threshold
        << " Nms: " << ad.Nms << " BatchSize: " << ad.BatchSize << " classNum: " << m_classNum);

    int getRes = this->initYolov5(yoloModePath, m_alarmType, ad, m_DeviceId);
    if(getRes==-1)
    {
        return false;
    }
        
    struct timeval tv;
	gettimeofday(&tv, NULL);

    m_IndexMap[m_zmqIndex_decode].taskId = ad.CurrentId;
    m_IndexMap[m_zmqIndex_decode].decodeId = ad.AlgorithmId;
    m_IndexMap[m_zmqIndex_decode].leaveTimeLimit = ad.LeaveTimeLimit;
    m_IndexMap[m_zmqIndex_decode].region = ad.Region;
    std::string eid = ad.AlgorithmId + 'E';
    m_IndexMap[m_zmqIndex_decode].encodeId = eid;
    if (m_ZermqIndexDataM.find(eid) != m_ZermqIndexDataM.end())
        m_IndexMap[m_zmqIndex_decode].pushFlow = true;
    m_IndexMap[m_zmqIndex_decode].alarmDuration = tv.tv_sec - 60*60*24; //实现启动即报警
    m_IndexMap[m_zmqIndex_decode].trustFlag = ad.TrustList;
    m_IndexMap[m_zmqIndex_decode].threshold = ad.Threshold;
    m_IndexMap[m_zmqIndex_decode].alarmPersonnels = ad.Personnels;
    m_IndexMap[m_zmqIndex_decode].alarmInterval = ad.AlarmInterval;

    /** @todo 初始化告警资源, 从任务管理模块迁移至此 */
    if (m_spFaceSDK) m_AlarmManager->addFaceSDKPointer(m_spFaceSDK);
    m_AlarmManager->init(m_alarmType, m_zmqIndex_task, zmqIndex_analysis, use_private_library, zmq_mgmt);

    //zmq 初始化
    m_ZmqManager=zmq_mgmt;

    return true;
}

void GsAIModelProcesser::getModePath(int atype, std::string& path)
{
    path = "";
    MongoDB mongodb;
    if (mongodb.Init(m_DBHost))
    {
        std::vector<std::string> ret_value;
        std::map<string,int> key_value;
        key_value["AlgorithmType"] = atype;

        mongodb.mongoQueryMatching<std::map<string,int>>("gsafety_model", key_value, ret_value);

        if (ret_value.size())
        {
            Json::Value value;
            Json::CharReaderBuilder readerBuilder;
            std::unique_ptr<Json::CharReader> const jsonReader(readerBuilder.newCharReader());
            if (jsonReader->parse(ret_value.at(0).c_str(), ret_value.at(0).c_str() + ret_value.at(0).length(), &value, nullptr))
            {
                LOG4CPLUS_INFO("root", "Database used by algorithm model file");
                path = value["ModelPath"].asString();
                m_classNum = value["classNum"].asInt();
                return;
            }
        }
    }

    LOG4CPLUS_INFO("root", "Default used by algorithm model file");
    switch(atype)
    {
        case FIRE : path = "/usr/local/cuda/tensorRT/models/fire.wts"; m_classNum = 6; break;
        case HELMET : path = "/usr/local/cuda/tensorRT/models/hpw.wts"; m_classNum = 5; break;
        case PERSION : path = "/usr/local/cuda/tensorRT/models/hpw.wts"; m_classNum = 5; break;
        case WELDING : path = "/usr/local/cuda/tensorRT/models/weld.wts"; m_classNum = 2; break;
        case REFLECTIVE_CLOSTHES : path = "/usr/local/cuda/tensorRT/models/hpw.wts"; m_classNum = 5; break;
        case BLET_DEVIATION : path = "/usr/local/cuda/tensorRT/models/blet_deviation.wts"; break;
        case CROWD_DENSITY : path = "/usr/local/cuda/tensorRT/models/yolov5s_count.wts"; m_classNum = 80; break;
        case VEHICLE : path = "/usr/local/cuda/tensorRT/models/vehicle/"; break;
        case VEHICLE_TYPE : path = "/usr/local/cuda/tensorRT/models/vehicle/"; break;
        case FACE : path = "/usr/local/cuda/tensorRT/models/face/"; break;
        /** @brief 添加新算法模型文件,非库读取模式 */
    }
}

int GsAIModelProcesser::inferYolov5(std::vector<cv::cuda::GpuMat>& inputImage,DetectioResV &outputRes,float nms,float conf)
{
    outputRes.resize(inputImage.size());
    if(m_spHelmetDetect)
    {
        try {
            ADetectioResV ares(inputImage.size());
			m_spHelmetDetect->infer(inputImage,ares,nms,conf);
            for(int i = 0; i < ares.size(); i++)
            {
                outputRes[i].resize(ares[i].size());
                for(int j = 0; j < ares[i].size(); j++)
                    memcpy(&outputRes[i][j], &ares[i][j], sizeof(Yolo::Detection));
            }
		}
		catch(cv::Exception &obj)
		{
			return -1;
		}
    }
    else if(m_spTrackIntegration)
    {
        int i = 0;
        for(auto& node : inputImage) //利用gpuMat引用计数
        {
            cv::Mat frame;
            node.download(frame);
            flag_img ret = m_spTrackIntegration->process_img(frame);
            DetectioRes res;
            res.bbox[0] = 0; res.bbox[1] = 0; res.bbox[2] = 0; res.bbox[3] = 0;  //不画框
            res.class_id = ret.flag;
            res.conf = 1.0;  //不设置置信度
            outputRes[i++].push_back(res);
            node.upload(ret.frame);
        }
    }
    else if (m_spDenseSDK)
    {
        try {
            ADetectioResV ares(inputImage.size());
			m_spDenseSDK->infer(inputImage,ares,nms,conf);
            for(int i = 0; i < ares.size(); i++)
            {
                for (int j = 0; j < ares.at(i).size(); j++)
                {
                    if (ares.at(i)[j].class_id != 0)
                    {
                        ares.at(i).erase(ares.at(i).begin()+j--);
                        continue;
                    }
                }
                outputRes[i].resize(ares[i].size());
                for(int j = 0; j < ares[i].size(); j++)
                {
                    memcpy(&outputRes[i][j], &ares[i][j], sizeof(Yolo::Detection));
                    outputRes[i][j].isAlarm = NOALARM;
                    outputRes[i][j].count = ares[i].size();
                }
            }
		}
		catch(cv::Exception &obj)
		{
			return -1;
		}

    }
    else if (m_spFaceSDK)
    {
        DetectioRes res;
        std::vector<FaceRect> result;

        // cv::Mat _src;
		// _src = imread("/opt/video-algorithm/gsfaceModelCvt/jj.jpeg");
        // assert(_src.data);
		// cv::cuda::GpuMat _g_src;
        // _g_src.upload(_src);
        cv::cuda::GpuMat g_src;
		m_spFaceSDK->BGR2RGB(g_src, inputImage[0]);

#if defined(INPUTCHAR)
		FACE_NUMBERS num = m_spFaceSDK->getFacesAllResult(src.data, src.cols, src.rows, src.channels(), result, FACEGETALLPARAM);
#elif defined(INPUTGPUMAT)
		m_spFaceSDK->getFacesAllResult(g_src, result, GET_FACE_TYPE);
        //m_spFaceSDK->getFacesAllResult(inputImage[0], result, GET_FACE_TYPE);
#endif
        for (auto& node: result)
        {
            res.conf = node.score;
            res.isAlarm = NOALARM;
            res.feature = node.Rfeature;
            res.bbox[0] = node.facebox(0,0); res.bbox[1] = node.facebox(0,1);
            res.bbox[2] = node.facebox(0,2) - node.facebox(0,0);
            res.bbox[3] = node.facebox(0,3) - node.facebox(0,1);
            if (!m_privateLib) inputImage[0].download(res.image);
            outputRes[0].push_back(res);
        }
    }
    else if (vehicle_handle)
    {
        cv::cuda::GpuMat g_src;
        DetectioRes res;
        std::vector<TrackingBox> result;
#if 0
        cv::Mat _src;
		_src = imread("/opt/video-algorithm/vehicle/combind/images/ALT719.jpg");
        assert(_src.data);
        cv::cuda::GpuMat _g_src;
        _g_src.upload(_src);
        cv::cuda::cvtColor(_g_src, g_src, cv::COLOR_BGR2RGB);
#else
        //必须有BGR2RGB的转换，否则m_spPlateDetector检测结果为空
        cv::cuda::cvtColor(inputImage[0], g_src, cv::COLOR_BGR2RGB);
#endif

        bool b_detect = m_spTrtDetector->detect(g_src, 1, result);

        for (const auto &node : result)
		{
			// 只针对车辆做处理， 还有person/motorbike/bycycle 由于未有业务，暂无处理
			if (node.class_name == "car" || node.class_name == "bus" || node.class_name == "truck")
            {
                // 2. 截取车辆部分区域作为车牌检测的输入
                std::vector<platedetector::PlateBox> pres;
				cv::cuda::GpuMat car_gimg = g_src(node.box).clone();
				m_spPlateDetector->detect(car_gimg, pres);

				for (const auto &pr : pres)
				{					
                    // 3. 截取车牌部分区域作为车牌识别的输入
                    VPLATERRT_RESULT result;
                    cv::cuda::GpuMat plate_gimg = car_gimg(pr.rect).clone();

					if (RETURN_CODE_OK == VPlateRRT_Process(vehicle_handle, plate_gimg, &result))
                    {
                        res.isAlarm = NOALARM;
                        res.conf = result.prob;
                        res.info = result.result;
                        res.bbox[0] = node.box.x + pr.rect.x;
                        res.bbox[1] = node.box.y + pr.rect.y;
                        res.bbox[2] = pr.rect.width;
                        res.bbox[3] = pr.rect.height;
                        if (!m_privateLib) inputImage[0].download(res.image);
                        outputRes[0].push_back(res);
                    }
				}
			}
		}
    }
    else if (m_spTrtDetector && !m_spPlateDetector && !vehicle_handle)
    {
        cv::cuda::GpuMat g_src;
        DetectioRes res;
        std::vector<TrackingBox> result;
        cv::cuda::cvtColor(inputImage[0], g_src, cv::COLOR_BGR2RGB);

        bool b_detect = m_spTrtDetector->detect(g_src, 1, result);

        for (const auto &node : result)
		{
            // 只针对车辆做处理， 还有person/motorbike/bycycle 由于未有业务，暂无处理
			if (node.class_name == "car" || node.class_name == "bus" || node.class_name == "truck")
            {
                res.bbox[0] = node.box.x; res.bbox[1] = node.box.y;
                res.bbox[2] = node.box.width; res.bbox[3] = node.box.height;
                res.info = node.class_name;
                res.trackerId = node.id;
                res.isAlarm = NOALARM; //车辆类型不产生告警
                res.conf = node.prob;
                inputImage[0].download(res.image);
                outputRes[0].push_back(res);
            }
        }
    }
    else
    {
        return -1;
    }
}

int GsAIModelProcesser::initYolov5(const std::string &modelPath, int alarmType, const AlgorithmData& ad, int deviceID)
{
    if (alarmType == FIRE || alarmType == HELMET
     || alarmType == PERSION || alarmType == WELDING
     || alarmType == REFLECTIVE_CLOSTHES)
    {
        if(!m_spHelmetDetect)
        {
            m_spHelmetDetect = std::unique_ptr<helmetDetectEngine>(
                new helmetDetectEngine(modelPath,ad.Nms,deviceID,ad.BatchSize,ad.Threshold,m_classNum));
        }

        m_batchsize=ad.BatchSize;

        if(!m_spHelmetDetect->serilize(m_batchsize))
            return -1;

        if(!m_spHelmetDetect->build(m_batchsize,deviceID))
            return -1;
    }
    else if (alarmType == BLET_DEVIATION)
    {
        if (ad.Region.interestType != 4 || ad.Region.allPoints.size() != 4)
        {
            LOG4CPLUS_ERROR("root", "BLET_DEVIATION type or points incorrect");
            return -1;
        }

        Point2i left_top = Point2i(ad.Region.allPoints[0].horizontal, ad.Region.allPoints[0].ordinate);
        Point2i left_bottom = Point2i(ad.Region.allPoints[1].horizontal, ad.Region.allPoints[1].ordinate);
        Point2i right_bottom = Point2i(ad.Region.allPoints[2].horizontal, ad.Region.allPoints[2].ordinate);
        Point2i right_top = Point2i(ad.Region.allPoints[3].horizontal, ad.Region.allPoints[3].ordinate);
        
        m_spTrackIntegration = std::unique_ptr<TrackIntegration>(
            new TrackIntegration(left_top,left_bottom,right_bottom,right_top));
    }
    else if (alarmType == CROWD_DENSITY)
    {
        if(!m_spDenseSDK)
        {
            m_spDenseSDK =std::unique_ptr<counter::COUNTER>(
                new counter::COUNTER(modelPath,ad.Nms,deviceID,ad.BatchSize,ad.Threshold,m_classNum));
        }
        m_batchsize=ad.BatchSize;
        if(!m_spDenseSDK->serilize(m_batchsize))
            return -1;

        if(!m_spDenseSDK->build(m_batchsize, deviceID))
            return -1;
    }
    else if (alarmType == FACE)
    {
        if(!m_spFaceSDK)
        {
            gsFaceParam gsfaceParam {
                FACEDET_NMS_TSD, FACEDET_TSD, FACEDET_DEVICE_ID, FACEDET_BTS, FACEDET_MOD_PTH,
				FEAEXT_DEVICE_ID, FEAEXT_BTS, FEAEXT_MOD_PTH,
				TRACK_NN_BUDGET, TRACK_COS_DIST, TRACK_IOU_DIST, TRACK_MAX_AGE, TRACK_N_INIT,
                TRACK_DEVICE_ID, TRACK_BTS, TRACK_MOD_PTH, POSE_DEVICE_ID, POSE_BTS, POSE_MOD_PTH,
				INPUT_TYPE, WIDTH_SIZE_INPUT, SCAL_RATIO, GET_FACE_TYPE };
            m_spFaceSDK = std::make_shared<gsFaceSDK>(modelPath.c_str(),gsfaceParam);
            // if(!m_spFaceSDK->serilize())
            //     return -1;

            // if(!m_spFaceSDK->build(deviceID))
            //     return -1;
        }
    }
    else if (alarmType == VEHICLE)
    {
        if(!vehicle_handle)
        {
            DetectConfig config;
            config.net_type = MODEL_S;
            config.detect_thresh = ad.Threshold;
            config.file_model_weights = modelPath + "model_s.wts";
            config.gpu_id = deviceID;
            //FP16：开启GPU半精度计算，可以加快推理速度，准确度稍稍降低
            config.inference_precison = Precision(2);
            config.track_type = SORT;

            m_spTrtDetector = std::unique_ptr<TrtDetector>(new TrtDetector());
            if (!m_spTrtDetector->init(config))
            {
                LOG4CPLUS_ERROR("root", "init Vehicle Detect Fail!");
                return -1;
            }

            platedetector::PlateDConfig pconfig;
            pconfig.detect_thresh = ad.Threshold;
            pconfig.file_model_weights = modelPath + "chinaplate0D.wts";
            pconfig.gpu_id = deviceID;
            pconfig.inference_precison = platedetector::PlateDPrecision(2);
            m_spPlateDetector = std::unique_ptr<platedetector::PlateDetector>(new platedetector::PlateDetector());    
            if (!m_spPlateDetector->init(pconfig))
            {
                LOG4CPLUS_ERROR("root", "init Vehicle plateDetect Fail!");
                return -1;
            }
    
            VPLATERRT_PARAM plateParam;
            plateParam.mode = GPU_MODE;
            plateParam.gpuid = deviceID;
            std::string plateRPath = modelPath + "chineseplateR_sim.trt";
            if(VPlateRRT_Init(vehicle_handle, plateRPath.c_str(), &plateParam, China) != 0)
            {
                LOG4CPLUS_ERROR("root", "init Vehicle plateR Fail!");
                return -1;
            }
        }
    }
    else if (alarmType == VEHICLE_TYPE)
    {
        if(!m_spTrtDetector)
        {
            DetectConfig config;
            config.net_type = MODEL_S;
            config.detect_thresh = ad.Threshold;
            config.file_model_weights = modelPath + "model_s.wts";
            config.gpu_id = deviceID;
            //FP16：开启GPU半精度计算，可以加快推理速度，准确度稍稍降低
            config.inference_precison = Precision(2);
            config.track_type = SORT;
            if (!m_spTrtDetector->init(config))
            {
                LOG4CPLUS_ERROR("root", "init Vehicle Detect Fail!");
                return -1;
            }
        }
    }

    return 0;

};

void GsAIModelProcesser::inferProcessLoop()
{
    uint64_t count = 1000, index = 0;
    std::map<int, AlgorithmData> algor_dataM;
    std::vector<InterArea> inter_areaV;

    index |= (uint64_t)0x0000000000000001 << m_zmqIndex_decode;

    m_index_max = m_ZmqManager->GetIndexNums();

    LOG4CPLUS_INFO("root", "AI init IndexNum = " << m_zmqIndex_decode);
    LOG4CPLUS_INFO("root", "AI MaxIndexNum = " << m_index_max);
    LOG4CPLUS_INFO("root", "AI ListenSocket = " << index);

    while(index)
    {
        uint64_t bit=m_ZmqManager->MessagePool(index);
        if(bit == 0) continue;

        //算法分析所需要的变量资源
        DetectioResV ppRes;
        std::vector<int> infer_indexV;
        std::vector<cv::cuda::GpuMat> vec;

        //索引最大值预先计算,在主任务指令处理中进行更新
        for (int i = 0; i < m_index_max; i++)
        {
            if(bit & 0x01)
            {
                std::size_t msg_len = 0;
                std::string zmqDecodeMsg = "";
                m_ZmqManager->ReceiveMessage(i, zmqDecodeMsg, msg_len);

                if(zmqDecodeMsg=="1")
                {
                    //解码发送包含id信息,可以直接提取gpu地址,当前只能使用内部记录m_IndexMap
                    vec.push_back(*m_ZermqIndexDataM[m_IndexMap[i].decodeId].DecodeGpu);
                    infer_indexV.push_back(i);
                    inter_areaV.push_back(m_IndexMap[i].region);
                    if (count++ > 100) //测试代码
                    {
                        count = 0;
                        LOG4CPLUS_INFO("root", "AI Running: " << i);
                    }
                }
                else
                {
                    //主任务指令解析处理, 添加到指令处理结果索引集合
                    AlgorithmData ad;
                    LOG4CPLUS_INFO("root", "AI from TM recvMsg:\n" << zmqDecodeMsg);
                    if (taskDataDeserialize(zmqDecodeMsg, ad))
                        algor_dataM[i] = ad;
                }
            }
            bit >>= 0x01;
        }

        /** @brief 主任务指令处理 */
        taskDataProcess(algor_dataM, index);

        /** @brief 性能分析处理 */
        performanceAnalysis(vec.size());

        if (0 == vec.size()) continue;
        
        /** @brief AI推理分析 */
        inferYolov5(vec, ppRes,m_nms,m_conf);

        /** @brief 告警分析处理 */
        alarmAnalysis(vec, infer_indexV, inter_areaV, ppRes);

        /** @brief 编码推流处理 */
        encodeProcess(infer_indexV, ppRes, vec);

        /** @todo 清理人脸,车辆非告警检测数据 */
        if (FACE == m_alarmType || VEHICLE == m_alarmType ||
            CROWD_DENSITY == m_alarmType || PERSION == m_alarmType)
            noAlarmHandle(ppRes);

        /** @brief 发送告警并保存图片到存储服务 */
        alarmDelivery(infer_indexV, vec, ppRes);

        //推送算法分析完成动作到解码模块
        for(auto decode_index : infer_indexV)
            m_ZmqManager->SendMessage(decode_index, "1", 1, false);
    }
    //this_thread::sleep_for(std::chrono::seconds(1));
    LOG4CPLUS_INFO("root", "AI ProcessLoop Finish");
}

bool GsAIModelProcesser::savePicture(std::string& path, const cv::Mat& frame)
{
    /** @brief 发送至fastdfs */
    std::vector<uchar> buff;
    int MAX_MODEL_IMAGE_SIZE=frame.cols* frame.rows*frame.channels();
    char *modelImage = new char[MAX_MODEL_IMAGE_SIZE]; //char数组

    bool flag=cv::imencode(".jpg", frame, buff);
    memcpy(modelImage, reinterpret_cast<char*>(&buff[0]), buff.size());
    
    if (m_PFastDfsManager->uploadFile(modelImage, buff.size(), path))
    {
        delete []modelImage;
        modelImage = nullptr;
        return false;
    }
        
    delete []modelImage;
    modelImage = nullptr;
    return true;
}

bool GsAIModelProcesser::taskDataDeserialize(const std::string& msg, AlgorithmData& algor_data)
{
   //json 反序列化
    Json::Value value;
    DATASTATE Status;
    Json::CharReaderBuilder readerBuilder;
    std::unique_ptr<Json::CharReader> const jsonReader(readerBuilder.newCharReader());
    if (!jsonReader->parse(msg.c_str(), msg.c_str() + msg.length(), &value, nullptr))
        return false;

    algor_data.dot = (DOT)value["dot"].asInt();
    algor_data.AlgorithmId = value["AlgorithmId"].asCString();
    algor_data.CurrentId = value["CurrentId"].asCString();
    algor_data.Nms = value["Nms"].asFloat();
    algor_data.BatchSize = value["BatchSize"].asUInt();
    algor_data.Threshold = value["Threshold"].asFloat();
    algor_data.AlarmInterval = value["AlarmInterval"].asInt64();
    algor_data.Resolution = value["Resolution"].asUInt();
    algor_data.Personnels = value["Personnels"].asUInt();
    algor_data.LeaveTimeLimit = value["LeaveTimeLimit"].asUInt();
    algor_data.TrustList = value["TrustList"].asUInt();

    std::string values = value["Region"].asString();

    /** algor_data.Region数据解析需要春慧确定后处理 */
    m_DataAnalysis.AlgorithmRegionDeserialize(value["Region"].asString(), algor_data.Region);

    return true;
}

void GsAIModelProcesser::taskDataSerialize(const AlgorithmData& ad, std::string& msg)
{
    //发送给平台端口 组成json
    Json::Value root;
    Json::StreamWriterBuilder writerBuilder;
    std::ostringstream os;
    root["dot"] = ad.dot;
    root["Type"]= ALGORITHM;
    root["Status"]= ad.Status;
    root["CurrentId"]= ad.CurrentId;
    root["AlgorithmId"]=ad.AlgorithmId;
    std::unique_ptr<Json::StreamWriter> jsonWriter(writerBuilder.newStreamWriter());
    jsonWriter->write(root, &os);
    msg = os.str();
}

void GsAIModelProcesser::taskDataProcess(std::map<int, AlgorithmData>& algor_dataM, uint64_t& index)
{
    std::string jsonStr;

    if (0 == algor_dataM.size()) return;

    /** @brief 指令执行逻辑处理 */
    std::map<int, AlgorithmData>::iterator iter = algor_dataM.begin();
    for(; iter != algor_dataM.end(); iter++)
    {
        /** @brief 消费每一条指令信息 */
        int _index = m_ZermqIndexDataM[iter->second.AlgorithmId].index;
        if (iter->second.dot & D_ADD)
        {
            /** @brief m_IndexMap 更新 */
            m_IndexMap[_index].taskId = iter->second.CurrentId;
            m_IndexMap[_index].decodeId = iter->second.AlgorithmId;
            m_IndexMap[_index].region = iter->second.Region;
            std::string eid = iter->second.AlgorithmId + 'E';
            m_IndexMap[_index].encodeId = eid;
            if (m_ZermqIndexDataM.find(eid) != m_ZermqIndexDataM.end())
                m_IndexMap[_index].pushFlow = true;
                
            struct timeval tv;
            gettimeofday(&tv, NULL);

            m_IndexMap[_index].alarmDuration = tv.tv_sec - 60*60*24;
            m_IndexMap[_index].threshold = iter->second.Threshold;
            m_IndexMap[_index].trustFlag = iter->second.TrustList;
            m_IndexMap[_index].leaveTimeLimit = iter->second.LeaveTimeLimit;
            m_IndexMap[_index].alarmPersonnels = iter->second.Personnels;
            m_IndexMap[_index].alarmInterval = iter->second.AlarmInterval;
            index |= (uint64_t)0x0000000000000001 << _index;
        }
        else if (iter->second.dot & D_DEL)
        {
            m_IndexMap.erase(_index);
    
            /** @brief index 监听描述符更新 */
            index &= ~((uint64_t)0x0000000000000001 << _index);
        }
        else if (iter->second.dot & D_AMODIFY)
        {      
            /** @brief 重新加载模型参数 */
            m_nms=iter->second.Nms;
            m_conf=iter->second.Threshold;
            m_IndexMap[_index].region = iter->second.Region;
            m_IndexMap[_index].threshold = iter->second.Threshold;
            m_IndexMap[_index].trustFlag = iter->second.TrustList;
            m_IndexMap[_index].leaveTimeLimit = iter->second.LeaveTimeLimit;
            m_IndexMap[_index].alarmPersonnels = iter->second.Personnels;
            m_IndexMap[_index].alarmInterval = iter->second.AlarmInterval;
        }

        iter->second.Status = SUCCESS;
        //指令消息执行状态数据序列化
        taskDataSerialize(iter->second, jsonStr);

        if (iter->second.dot == D_ADD)
            LOG4CPLUS_INFO("root", "AI to TM Instruction (Add) Data:\n" << jsonStr);
        else if (iter->second.dot == D_DEL)
            LOG4CPLUS_INFO("root", "AI to TM Instruction (Delete) Data:\n" << jsonStr);
        else if (iter->second.dot == D_AMODIFY)
            LOG4CPLUS_INFO("root", "AI to TM Instruction (Modify) Data:\n" << jsonStr);

        //指令处理状态数据分发给平台
        m_ZmqManager->SendMessage(m_zmqIndex_task, jsonStr, jsonStr.size());
    }

    m_index_max = m_ZmqManager->GetIndexNums();
    algor_dataM.clear();
}

void GsAIModelProcesser::encodeProcess(std::vector<int> infer_indexV, DetectioResV& inResV,
        std::vector<cv::cuda::GpuMat>& decodeV)
{
    for(int i = 0; i < inResV.size(); i++)
	{
        if (!m_IndexMap[infer_indexV[i]].pushFlow) continue;

        //inResV经过处理后只包含告警数据
        std::string eid = m_IndexMap[infer_indexV[i]].encodeId;
        cv::cuda::GpuMat& encode_gpu = *m_ZermqIndexDataM[eid].EncodeGpu;
        int encode_index = m_ZermqIndexDataM[eid].index;

        /** @todo 人密图像拼接实现推流展示,未确认功能不做优化 */
        //if (CROWD_DENSITY == m_alarmType)
        //{
            /** @todo 人密图像拼接代码逻辑已注释 */
            // thread_local cv::Mat tmp;
            // thread_local uint64_t preTime = 0;
            // struct timeval tv;
	        // gettimeofday(&tv, NULL);
            
            // if (tv.tv_sec - preTime > 2) //2秒一次绘制
            // {
            //     preTime = tv.tv_sec;
            //     drawArcLine(tmp, inResV.at(i)[0].image.cols, inResV.at(i)[0].count);
            // }
            
            // vector<cv::Mat> images;
            // images.push_back(inResV.at(i)[0].image);
            // images.push_back(tmp);

            // cv::Mat result;
            // imageSplicing(images, result, 1);
            // cv::resize(result, result, cv::Size(inResV.at(i)[0].image.cols, inResV.at(i)[0].image.rows));
            // encode_gpu.upload(result);
            //encode_gpu.upload(inResV.at(i)[0].image);
        //}
        
        drawRectangle(inResV.at(i), decodeV[i], encode_gpu);

        /** @brief 发送消息至编码线程 */
        std::string msg = "1";
        m_ZmqManager->SendMessage(encode_index, msg, msg.size(), false);
	}
}

void GsAIModelProcesser::alarmAnalysis(const std::vector<cv::cuda::GpuMat>& matV,
                                       std::vector<int>& infer_indexV,
                                       std::vector<InterArea>& inter_areaV,
                                       DetectioResV& inResV)
{
    std::vector<int> image_dim;
    for(auto node : matV)
    {
        image_dim.push_back(node.cols);
        image_dim.push_back(node.rows);
    }

    /** @brief 添加告警判断处理逻辑 */
    try {
        m_AlarmManager->isAlarmHandler(infer_indexV, inter_areaV, inResV, image_dim);
    }catch(const char* msg)
    {
        //LOG4CPLUS_INFO("root", msg);
    }
    inter_areaV.clear();
}

void GsAIModelProcesser::alarmDelivery(const std::vector<int>& infer_indexV,
                                       const std::vector<cv::cuda::GpuMat>& matV,
                                       const DetectioResV& inResV)
{
    struct timeval tv;
    for (int i = 0; i < inResV.size(); i++)
    {
        if (!inResV.at(i).size()) continue;

        AlarmResult alarm_result;

        /** @brief 告警时间限定判断 */
        uint64_t interval = m_IndexMap[infer_indexV[i]].alarmInterval;
	    gettimeofday(&tv, NULL);
        if (tv.tv_sec-m_IndexMap[infer_indexV[i]].alarmDuration < interval)
            continue;

        m_IndexMap[infer_indexV[i]].alarmDuration = tv.tv_sec;

        cv::Mat frame(matV[i].cols,matV[i].rows,CV_8UC3);
        matV[i].download(frame);

        for (auto& Res : inResV.at(i))
        {
            cv::Rect rect=cv::Rect(Res.bbox[0],Res.bbox[1],Res.bbox[2],Res.bbox[3]);
            cv::rectangle(frame,rect,cv::Scalar(0,0,255),2);
            if (!Res.info.empty())
            {
                cv::Point point(Res.bbox[0], Res.bbox[1] - 3);
                cv::Ptr<cv::freetype::FreeType2> ft2;
                ft2 = cv::freetype::createFreeType2();
                ft2->loadFontData("/usr/share/fonts/truetype/gsafety/fangsong.ttf", 0);
                ft2->putText(frame, Res.info, point,25, cv::Scalar(0,0,255),1,8,true);
            }
        }

        alarm_result.TimeStamp = tv.tv_sec;
        alarm_result.AlgorithmType = m_alarmType;
        alarm_result.CurrentId = m_IndexMap[infer_indexV[i]].taskId;
        alarm_result.CameraId = m_IndexMap[infer_indexV[i]].decodeId;
        alarm_result.AlarmId = getUuid(true);
        
        //保存图片到存储服务
        if (!savePicture(alarm_result.ImagePath, frame))
        {
            LOG4CPLUS_INFO("root",  alarm_result.CurrentId << ":" << alarm_result.CameraId << "->AI Save failure alarm picture");
            continue;
        }

        //发送告警数据到平台
        m_AlarmManager->sendOneAlarm(infer_indexV[i], alarm_result);
    }
}

void GsAIModelProcesser::noAlarmHandle(DetectioResV& detectionResV)
{
    for (int i = 0; i < detectionResV.size(); i++)
    {
        if (!detectionResV.at(i).size()) continue;
        auto iter = detectionResV.at(i).begin();
        for (; iter != detectionResV.at(i).end() ;)
        {
            if (NOALARM == iter->isAlarm)
                iter = detectionResV.at(i).erase(iter);
            else iter++;
        }
    }
}

void GsAIModelProcesser::performanceAnalysis(uint64 frame)
{
    /** @brief 时间限定间隔 */
    struct timeval tv;
	gettimeofday(&tv, NULL);
    thread_local uint64 interval = tv.tv_sec;
    thread_local uint64 total_frame = 0;

    total_frame += frame;
    
    if (tv.tv_sec-interval<300) return;

    uint32_t fps = (tv.tv_sec-interval)/total_frame;

    interval = tv.tv_sec;

    /** @brief 消息发送拼接 */
    Json::Value root;
    Json::StreamWriterBuilder writerBuilder;
    std::ostringstream os;
    Json::Value obj;
    obj["algorithmId"]=m_AlgorithmId;
    obj["algorithmName"]="noName";
    obj["algorithmFPS"]=fps;
    obj["defaultFPS"]=25;
    root.append(obj);
    std::unique_ptr<Json::StreamWriter> jsonWriter(writerBuilder.newStreamWriter());
    jsonWriter->write(root, &os);
    std::string jsonStr = os.str();
    /** 发送性能分析部分 */
    m_ZmqManager->SendMessage(m_zmqIndex_pa, jsonStr, jsonStr.size());
}
#ifdef ENABLERZ
void GsAIModelProcesser::drawRectangle(AIResult& AIRes, cv::cuda::GpuMat &dvec, cv::cuda::GpuMat &evec)
{
    cuInit(0);
    cudaSetDevice(m_DeviceId);
    
    //模拟没有报警的编码推流
    //AIRes.vecBbox.clear();

    cv::cuda::cvtColor(dvec,dvec,cv::COLOR_BGR2BGRA);

    //开始画框
    if(AIRes.vecBbox.size() != 0)
    {
        cv::Mat frame(dvec.cols, dvec.rows, CV_8UC4);
        dvec.download(frame);

        for(auto Res : AIRes.vecBbox)
        {
            cv::Rect rect=cv::Rect(Res[0],Res[1],Res[2],Res[3]);
            cv::rectangle(frame,rect,cv::Scalar(0,0,255),4);
            char info[50];
            sprintf(info, "conf:= %.2f", Res[4]);
            std::string str(info);
            cv::Point org(Res[0],Res[1]);
            cv::putText(frame,str,org,CV_FONT_HERSHEY_PLAIN,2.0,cv::Scalar(0,0,0),3);
        }

        evec.upload(frame);
        //cv::imwrite("/opt/2.jpg", frame);
    }
    else
    {
        dvec.copyTo(evec);
    }
}
#else
void GsAIModelProcesser::drawRectangle(const std::vector<DetectioRes>& AIRes, cv::cuda::GpuMat &dvec, cv::cuda::GpuMat &evec)
{
    cuInit(0);
    cudaSetDevice(m_DeviceId);
    
    //模拟没有报警的编码推流
    //AIRes.vecBbox.clear();

    cv::Mat frame(dvec.cols, dvec.rows, CV_8UC3);

    if (AIRes.size()) dvec.download(frame);

    //开始画框
    for(const auto& node : AIRes)
    {
        cv::Ptr<cv::freetype::FreeType2> ft2;
        ft2 = cv::freetype::createFreeType2();
        ft2->loadFontData("/usr/share/fonts/truetype/gsafety/fangsong.ttf", 0);
        //获取文本框的长宽
	    cv::Size text_size = cv::getTextSize(node.info, cv::FONT_HERSHEY_COMPLEX, 1.0, 2, nullptr);

        cv::Rect rect = cv::Rect(node.bbox[0], node.bbox[1], node.bbox[2], node.bbox[3]);
        cv::rectangle(frame,rect,cv::Scalar(0,255,0),2);
        std::string prefix = m_DataAnalysis.AITypeConvertName(m_alarmType);
        char info[50] = {0};
        sprintf(info, "-可信度≈%.1f", node.conf);
        prefix += info;
        int y = node.bbox[1] - text_size.height/2;
        cv::Point point(node.bbox[0], (y <= 0 ? 0 : y));
        ft2->putText(frame, prefix, point,25, cv::Scalar(0,255,0),1,8,true);

        if (!node.info.empty())
        {
            cv::Point point(node.bbox[0], node.bbox[1] + node.bbox[3] + text_size.height + 3);
            ft2->putText(frame, node.info, point,25, cv::Scalar(0,255,0),1,8,true);
        }
        //cv::imwrite("/opt/2.jpg", frame);
    }

    if (!AIRes.size())
        dvec.copyTo(evec);
    else
        evec.upload(frame);
}
#endif

void GsAIModelProcesser::drawArcLine(cv::Mat& images, int width, uint32_t person_nums)
{
    thread_local std::queue<cv::Point> myqueue;
    thread_local cv::Mat origin_image;
    thread_local uint32_t start = 0;
	thread_local float origin_scale = 1.0;

	int maxnums = images.cols/20;

    if (myqueue.empty()) myqueue.push(cv::Point(0,0));
    if (0 == origin_image.size.dims())
    {
        origin_image = imread("/usr/local/cuda/crowd_density.jpeg");
        cv::resize(origin_image, origin_image, cv::Size(width, origin_image.rows));
    }
    
    images = origin_image.clone();

	myqueue.push(cv::Point(start, person_nums*origin_scale));
	if (myqueue.size() > maxnums)
		myqueue.pop();

	uint32_t maxValue = 0;
	for(int i = 0; i < myqueue.size(); i++)
	{
		//myqueue_size 必须是固定值
		cv::Point& point = myqueue.front();
		maxValue = maxValue < point.y ? point.y : maxValue;
		myqueue.push(cv::Point(point.x-20, point.y));
		myqueue.pop();
	}

	float real_scale = (float)images.rows/(float)maxValue;
	if (real_scale > 1.0) real_scale = 1.0;

	if (real_scale != origin_scale)
		origin_scale = real_scale;

	cv::Point point1 = myqueue.front();
	myqueue.push(point1);
	myqueue.pop();
	point1.y = images.rows - point1.y * real_scale;
    for(int i = 1; i < myqueue.size(); i++)
	{
		cv::Point point2 = myqueue.front();
		myqueue.push(point2);
		myqueue.pop();
		point2.y = images.rows - point2.y * real_scale;
        cv::line(images, point1, point2, cv::Scalar(255,255,255), 2, 8);
		point1 = point2;
	}
    start += 20;
    if (start >= images.cols) start = images.cols;
}

// 图像拼接
void GsAIModelProcesser::imageSplicing(vector<cv::Mat> images, cv::Mat& result, int type)
{
	if (type != 0 && type != 1)
		type = 0;
	
	int num = images.size();
	int newrow = 0;
	int newcol = 0;
	// 横向拼接
	if (type == 0)
	{
		int minrow = 10000;
		for (int i = 0; i < num; ++i)
		{
			if (minrow > images[i].rows)
				minrow = images[i].rows;
		}
		newrow = minrow;
		for (int i = 0; i < num; ++i)
		{
			int tcol = images[i].cols*minrow / images[i].rows;
			int trow = newrow;
			cv::resize(images[i], images[i], cv::Size(tcol, trow));
			newcol += images[i].cols;
			if (images[i].type() != images[0].type())
				images[i].convertTo(images[i], images[0].type());
		}
		result = cv::Mat(newrow, newcol, images[0].type(), cv::Scalar(255, 255, 255));
		cv::Range rangerow, rangecol;
		int start = 0;
		for (int i = 0; i < num; ++i)
		{
			rangerow = cv::Range((newrow - images[i].rows) / 2, (newrow - images[i].rows) / 2 + images[i].rows);
			rangecol = cv::Range(start, start + images[i].cols);
			images[i].copyTo(result(rangerow, rangecol));
			start += images[i].cols;
		}
	}
	// 纵向拼接
	else if (type == 1) {
		int mincol = 10000;
		for (int i = 0; i < num; ++i)
		{
			if (mincol > images[i].cols)
				mincol = images[i].cols;
		}
		newcol = mincol;
		for (int i = 0; i < num; ++i)
		{
			int trow = images[i].rows*mincol / images[i].cols;
			int tcol = newcol;
			cv::resize(images[i], images[i], cv::Size(tcol, trow));
			newrow += images[i].rows;
			if (images[i].type() != images[0].type())
				images[i].convertTo(images[i], images[0].type());
		}
		result = cv::Mat(newrow, newcol, images[0].type(), cv::Scalar(255, 255, 255));

		cv::Range rangerow, rangecol;
		int start = 0;
		for (int i = 0; i < num; ++i)
		{
			rangecol= cv::Range((newcol - images[i].cols) / 2, (newcol - images[i].cols) / 2 + images[i].cols);
			rangerow = cv::Range(start, start + images[i].rows);
			images[i].copyTo(result(rangerow, rangecol));
			start += images[i].rows;
		}
	}
}

/** 测特告警模块告警发送 */
void GsAIModelProcesser::sendAlarmInfo(std::vector<DetectioRes>& res, std::string id)
{
    Json::Value root;
    Json::StreamWriterBuilder writerBuilder;
    std::ostringstream os;

    /** @todo 算法结构化数据处理 */
    AlgorithmData metadata;
    Json::Value arrayObj;

    root["Type"] = ALARMA;
    root["CameraId"] = "123456";
    root["ImagePath"] = "/opt";
    root["TimeStamp"] = 1645755312;
    root["Aresult"][0]["x"] = 5;
    root["Aresult"][0]["y"] = 6;
    root["Aresult"][0]["width"] = 50;
    root["Aresult"][0]["height"] = 60;
    root["Aresult"][0]["Confidence"] = 0.5;
    // arrayObj["x"] = 1;
    // arrayObj["y"] = 1;
    // arrayObj["width"] = 100; 
    // arrayObj["height"] = 100;
    // root["Region"] = arrayObj;

    std::unique_ptr<Json::StreamWriter> jsonWriter(writerBuilder.newStreamWriter()); //json格式
    //Json::FastWriter jsonWriter;  //非json格式
    //ret = jsonWriter.write(root);
    jsonWriter->write(root, &os);
    std::string msg = os.str();
    std::cout <<msg<<std::endl;
    m_ZmqManager->SendMessage(m_zmqIndex_task, msg, msg.size());
}