#include "HttpServerManager.hpp"
#include "GsFastDfsManager.h"

HttpServerManager::HttpServerManager(const std::string HttpHost) :
    m_zmqIndex_task(-1), m_zmqIndex_analysis(-1),
    m_HttpServer(std::make_shared<HttpServer>())
{
	registerHandlerProc();

	LOG4CPLUS_INFO("root", "initHttpServer Success!");
}

HttpServerManager::~HttpServerManager()
{
	/** @todo 释放AI算法资源 */

	LOG4CPLUS_INFO("root", "HttpServer Resource Release!");
}

void HttpServerManager::initResource(const BaseData& bd, std::shared_ptr<ZmqManager> zmq_mgmt, 
        ZeromqIndexDataMap& zermq_index_data)
{
	/** @brief 初始化模块通信索引,下表使用md5值 */
	m_ZmqManager = zmq_mgmt;
	m_zmqIndex_task = zermq_index_data["dad4668bc0b8e79cd460bf9f0e0a9700"].index;
    m_zmqIndex_analysis =  zermq_index_data["bc8027fcb377a875a3329b7706cc5431"].index;

		//create http server
	if (!m_HttpServer->initHttpServer(bd.HttpHost))
	{
		LOG4CPLUS_INFO("root", "initHttpServer Fail!");
		return;
	}

	/** @todo 加载AI算法资源 */
	if(bd.AlgorithmType == FACE)
	{
		faceDetectAndFeature = make_shared<CFaceDetectAndFeature>();
		init = faceDetectAndFeature->init();
	}

	/**@todo 初始化车牌算法 */
	if(bd.AlgorithmType == VEHICLE)
	{

	}

	/**@todo 初始化车型算法*/
	if(bd.AlgorithmType == VEHICLE_TYPE)
	{

	}

	LOG4CPLUS_INFO("root", "HttpServer initResource Success!");

	/** @brief 注释代码为 接收和处理 采用相对独立处理线程,来源于人脸识别代码复制 */
	// faceProcessThread = thread(bind(&CFaceDetectHttpServer::faceProcess, this));
	// compareFeatureThread = thread(bind(&CFaceDetectHttpServer::compareFaceFeature, this));
	// setFaceFeaturethread = thread(bind(&CFaceDetectHttpServer::setFaceFeature, this));

	m_HttpServer->start();
}

void HttpServerManager::registerHandlerProc()
{
	string handlerURL = "/getFaceALL";
	m_HttpServer->registerHandler(handlerURL, bind(&HttpServerManager::handlerFaceAll, this, placeholders::_1, placeholders::_2));
	handlerURL = "/getFaceFeatureCompare";
	m_HttpServer->registerHandler(handlerURL, bind(&HttpServerManager::handlerFaceFeatureCompare, this, placeholders::_1, placeholders::_2));
	handlerURL = "/setFaceFeature";
	m_HttpServer->registerHandler(handlerURL, bind(&HttpServerManager::handlerSetFaceFeature, this, placeholders::_1, placeholders::_2));
	handlerURL = "/clearData";
	m_HttpServer->registerHandler(handlerURL, bind(&HttpServerManager::handlerClearData, this, placeholders::_1, placeholders::_2));
}

bool HttpServerManager::handlerFaceAll(mg_connection *mgConnection, http_message *message)
{
	/** @todo 判断队列是否为空,处理post上传请求,callback和handler非同一线程使用 */

	/** @todo 解析post数据，判断指令操作类型 */
	HttpPlatFormData httpPlatFormData;
	bool result = parseFaceAllData(mgConnection, message, httpPlatFormData);
	if(!result)
	{
		/** @todo 发送消息给客户端,解析数据失败*/
		return false;
	}
	string encodeImageData = httpPlatFormData.getImageData();
	if(encodeImageData.empty())
	{
		/**@brief 没有传输图片数据*/
		HttpServer::sendRsp(mgConnection, "客户端没有传输数据!");
		return false;
	}

	int len = httpPlatFormData.getImageWidth() * httpPlatFormData.getImageHeight() * 3;
	char *buf = new char[len];
	HttpServer::baseDecode((const unsigned char *)encodeImageData.c_str(), encodeImageData.size(), buf);
	InternalDataSet internalDataSet;
	Mat frame(httpPlatFormData.getImageHeight(), httpPlatFormData.getImageWidth(), CV_8UC3, buf);
	internalDataSet.mat = frame.clone();
	frame.release();
	delete []buf;
	buf = nullptr;

	/** @todo 执行人脸算法分析 */
	result = processFaceImage(mgConnection, internalDataSet);
	if(!result)
	{
		return false;
	}

	//组装发送给客户端和分析端的数据
	internalDataSet.idot = I_FACERESULT_HTTP;
	vector<AlgorithmResultSet> resultSetVec;
	for (size_t i = 0; i < internalDataSet.DetectBoxV.size(); i++)
	{
		AlgorithmResultSet algorithmResultSet;
		algorithmResultSet.setLeft(internalDataSet.DetectBoxV[i].rect.x);
		algorithmResultSet.setTop(internalDataSet.DetectBoxV[i].rect.y);
		algorithmResultSet.setWidth(internalDataSet.DetectBoxV[i].rect.width);
		algorithmResultSet.setHeight(internalDataSet.DetectBoxV[i].rect.height);
		algorithmResultSet.setFeature(internalDataSet.DetectBoxV[i].feature);
		algorithmResultSet.setConfidence(internalDataSet.DetectBoxV[i].Confidence);

		resultSetVec.push_back(algorithmResultSet);
	}
	httpPlatFormData.setResultSet(resultSetVec);
	assembleInternalDataset(internalDataSet, httpPlatFormData);
	/** @todo 推送分析结果到http客户端 */
	sendMessageToClient(mgConnection, httpPlatFormData);

	if(httpPlatFormData.getTrustFlag() == 1 ||
	   httpPlatFormData.getTrustFlag() == 2)
	{
		/** @todo 发给主任务 */
		sendToAIDataAnalysis(m_zmqIndex_task, internalDataSet);
	}
	else
	{
		/** @todo 推送数据至告警分析索引 */
		sendToAIDataAnalysis(m_zmqIndex_analysis, internalDataSet);
	}
}

bool HttpServerManager::parseFaceAllData(mg_connection *mgConnection, http_message *message, HttpPlatFormData &httpPlatFormData)
{
	string bodyMessage = string(message->body.p, message->body.len);
	string queryMessage = string(message->query_string.p, message->query_string.len);
	string contentTypeName("Content-Type");
	string contentLengthName("Content-Length");
	string contentType = getHttpHeaderNameValue(message, contentTypeName);
	string contentLength = getHttpHeaderNameValue(message, contentLengthName);
	int contentLenthSize = atoi(contentLength.c_str());
	if(contentLenthSize <= 0 || contentLenthSize != message->body.len)
	{
		/**@todo 接收到的数据不完整*/
		HttpServer::sendRsp(mgConnection, "数据接收不完整!");
		return false;
	}

	bool result = false;
	if(mg_vcmp(&message->method, "GET") == 0)
	{
		/**@todo 暂时不支持GET方法*/
		HttpServer::sendRsp(mgConnection, "此方法不支持!");
		return result;
	}
	else if(mg_vcmp(&message->method, "POST") == 0 &&
			!(contentType.compare("application/json")))
	{
		/**@todo 解析平台数据(protobuffer)*/
		DataAnalysis dataAnalysis;
		result =  dataAnalysis.PlatformDataSetDeserialize(bodyMessage, httpPlatFormData);
		if(!result)
		{
			HttpServer::sendRsp(mgConnection, "服务端解析数据失败!");
		}
		return result;
	}

	return result;
}

bool HttpServerManager::processFaceImage(mg_connection *mgConnection, InternalDataSet &internalDataSet)
{
	vector<CFaceInformation> faceInformationVec;
	faceDetectAndFeature->processImage(internalDataSet.mat, internalDataSet.mat.cols, internalDataSet.mat.rows, internalDataSet.mat.channels(), FACEALL, faceInformationVec);
	if(faceInformationVec.empty())
	{
		/**@todo 没有人脸信息*/
		HttpServer::sendRsp(mgConnection, "没有人脸信息!");
		return false;
	}
	for (size_t i = 0; i < faceInformationVec.size(); i++)
	{
		AlgorithmResult algorithmData;
		algorithmData.Confidence = faceInformationVec[i].facePosScore;
		algorithmData.rect.x = faceInformationVec[i].left;
		algorithmData.rect.y = faceInformationVec[i].top;
		algorithmData.rect.width = faceInformationVec[i].width;
		algorithmData.rect.height = faceInformationVec[i].height;
		memcpy(algorithmData.feature.data(), faceInformationVec[i].feature, _FEATURESIZE * sizeof(float));
		internalDataSet.DetectBoxV.push_back(algorithmData);
	}
	
	return true;
}

void HttpServerManager::sendMessageToClient(mg_connection *mgConnection, HttpPlatFormData &httpPlatFormData)
{
	DataAnalysis dataAnalysis;
	string msg;
	dataAnalysis.platformDataSetSerialize(msg, httpPlatFormData);
	HttpServer::sendRsp(mgConnection, msg);
}

void HttpServerManager::sendToAIDataAnalysis(int zmqIndex, InternalDataSet &internalDataSet)
{
	DataAnalysis dataAnalysis;
	string msg;
	dataAnalysis.InternalDataSetSerialize(msg, internalDataSet);
	m_ZmqManager->SendMessage(zmqIndex, msg, msg.size(), true);
}

bool HttpServerManager::handlerFaceFeatureCompare(mg_connection *mgConnection, http_message *message)
{
    /** @todo 解析post数据,获取客户端发送过来的消息*/
	HttpPlatFormData httpPlatFormData;
	bool result = parseFaceAllData(mgConnection, message, httpPlatFormData);
	if(!result)
	{
		/** @todo 发送消息给客户端,解析数据失败*/
		return false;
	}
	/** @todo 执行人脸比对*/
	if(httpPlatFormData.getResultSet().size() != 2)
	{
		/**@todo 人脸比对数不正确*/
		HttpServer::sendRsp(mgConnection, "人脸比对数目不对,1:1模式,只需要两个人脸特征!");
		return false;
	}
	
	vector<FEATUREM> features;
	vector<AlgorithmResultSet> resultSetVec = httpPlatFormData.getResultSet();
	FEATUREM featureTemp = resultSetVec[0].getFeature();
	features.push_back(resultSetVec[1].getFeature());
	float similarity =  faceDetectAndFeature->similarity(featureTemp, features);
	httpPlatFormData.setSimilarity(similarity);
	resultSetVec.clear();
	httpPlatFormData.setResultSet(resultSetVec);
	/** @todo 发送结果给客户端*/
	sendMessageToClient(mgConnection, httpPlatFormData);
}

bool HttpServerManager::handlerSetFaceFeature(mg_connection *mgConnection, http_message *message)
{
	/** @todo 解析post数据,获取客户端发送过来的数据*/
	HttpPlatFormData httpPlatformData;
	bool result = parseFaceAllData(mgConnection, message, httpPlatformData);
	if(!result)
	{
		return false;
	}
	/** @todo 发给平台,平台存储*/
	DataAnalysis dataAnalysis;
	InternalDataSet internalDataSet;
	internalDataSet.idot = I_FACERESULT_HTTP;
	vector<AlgorithmResultSet> algorithmResultSetVec = httpPlatformData.getResultSet();
	for (size_t i = 0; i < algorithmResultSetVec.size(); i++)
	{
		AlgorithmResult algorithmResult;
		algorithmResult.rect.x = algorithmResultSetVec[i].getLeft();
		algorithmResult.rect.y = algorithmResultSetVec[i].getTop();
		algorithmResult.rect.width = algorithmResultSetVec[i].getWidth();
		algorithmResult.rect.height = algorithmResultSetVec[i].getHeight();
		memcpy(algorithmResult.feature.data(), algorithmResultSetVec[i].getFeature().data(), sizeof(float) * _FEATURESIZE);
		internalDataSet.DetectBoxV.push_back(algorithmResult);
	}
	
	assembleInternalDataset(internalDataSet, httpPlatformData);
	string msg;
	dataAnalysis.InternalDataSetSerialize(msg, internalDataSet);
	m_ZmqManager->SendMessage(m_zmqIndex_task, msg, msg.size(), true);
	/** @todo 发送存储结果给客户端*/
	sendMessageToClient(mgConnection, httpPlatformData);
}

bool HttpServerManager::handlerClearData(mg_connection *mgConnection, http_message *message)
{
	return true;
}

void HttpServerManager::alarmAnalysis(const BaseData& bd, std::shared_ptr<ZmqManager> zmq_mgmt, 
	ZeromqIndexDataMap& zermq_index_data, const TrustDataTableMap& trust_dataM)
{
	/** @brief 初始化模块通信索引,下表使用md5值 */
	m_zmqIndex_task = zermq_index_data["dad4668bc0b8e79cd460bf9f0e0a9700"].index;
    m_zmqIndex_analysis =  zermq_index_data["bc8027fcb377a875a3329b7706cc5431"].index;
	
	DataAnalysis m_DataAnalysis;
	m_DataAnalysis.SetBaseInfo(bd);

	uint64_t index = 0;

    index |= (uint64_t)0x0000000000000001 << m_zmqIndex_analysis;

    LOG4CPLUS_INFO("root", "alarmAnalysis init IndexNum = " << m_zmqIndex_analysis);

	if (zmq_mgmt->AddPublisherSocket(bd.MsgBrokerHost))
		LOG4CPLUS_INFO("root", "AddPublisherSocket Ok");

    while(index)
    {
        uint64_t bit = zmq_mgmt->MessagePool(index);
        if(bit == 0) continue;

        std::size_t msg_len = 0;
        std::string zmqMsg = "";
        zmq_mgmt->ReceiveMessage(m_zmqIndex_analysis, zmqMsg, msg_len);

		/** @todo 解析protobuf序列化数据 */
		InternalDataSet internal_dataset;
		if (!m_DataAnalysis.InternalDataSetDeserialize(zmqMsg, internal_dataset))
			continue;

		/** 视频结果数据分析 */
		processResult(bd, internal_dataset, trust_dataM);

		/** 分析结果告警推送,判断InternalDataSet::FaceInfoV*/
		int index = zermq_index_data[internal_dataset.CameraId].index;
		alarmPush(zmq_mgmt, internal_dataset, index);

		/** @todo 结果数据推送至代理服务器,格式：taskID:body */
		std::string sendMsg = internal_dataset.CurrentId + ":";
		sendMsg += zmqMsg;
		zmq_mgmt->SendtoBroker(sendMsg, sendMsg.size());
	}
}

void HttpServerManager::processResult(const BaseData &bd, InternalDataSet& internal_dataS, const TrustDataTableMap& trust_dataM)
{
	/** @brief 人脸算法处理逻辑 */
	if (I_FACERESULT_VIDEO == internal_dataS.idot || I_FACERESULT_HTTP == internal_dataS.idot)
	{
		if (bd.HttpPeerHost.empty())
		{
			/** @todo 当前使用私有库数据比对 */
			for (auto& res : internal_dataS.DetectBoxV)
			{
				float sim = 0.0;
				AdditionalData face_data;
				face_data.IsAlarm = false;
				float max_sim = res.Confidence;
				for(auto &node : trust_dataM)
				{
					if (internal_dataS.TrustFlag == node.second.trust)
						sim = compares(node.second.feature, res.feature);
					else continue;

					if (sim < max_sim) continue;

					max_sim = sim;
					face_data.Name = node.second.name;
					face_data.Department = node.second.department;
					face_data.Id = node.first;
				}

				if (max_sim > res.Confidence)
				{
					face_data.IsAlarm = true;
					res.Confidence = max_sim;
				}

				internal_dataS.AdditionInfoV.push_back(face_data);
			}
		}
		else
		{
			/** @todo 外部数据源比对处理，根据项目定制 */
		}
	}
	/** @brief 车辆算法处理逻辑 */
	else if (I_VEHICLERESULT_VIDEO == internal_dataS.idot || I_VEHICLERESULT_HTTP == internal_dataS.idot)
	{
		if (bd.HttpPeerHost.empty())
		{
			/** @todo 当前使用私有库数据比对 */
			for (auto& res : internal_dataS.DetectBoxV)
			{
				AdditionalData vehicle_data;
				vehicle_data.IsAlarm = false;
				for(auto &node : trust_dataM)
				{
					if (internal_dataS.TrustFlag != node.second.trust)
						continue;

					if (!node.second.vehicle.compare(res.vehicleNum))
					{
						vehicle_data.IsAlarm = true;
						vehicle_data.Id = node.first;
						vehicle_data.Name = node.second.name;
						vehicle_data.Department = node.second.department;
					}					
				}

				internal_dataS.AdditionInfoV.push_back(vehicle_data);
			}
		}
		else
		{
			/** @todo 外部数据源比对处理，根据项目定制 */
		}
	}
}

float HttpServerManager::compares(const FEATUREM& f1, const FEATUREM& f2)
{
	float theta = acos(f1.dot(f2));
	float sim;
	if(!theta || isnan(theta))
		sim = 0;
	else
		sim = 1-(theta/3.141592653);
    return sim;
}

void HttpServerManager::alarmPush(std::shared_ptr<ZmqManager> zmq_mgmt, InternalDataSet& internal_dataS, int camera_index)
{
	/** @todo 根据需求增加摄像头对应的告警时长处理 */
	struct timeval tv;
	thread_local std::map<std::string, uint64_t> _AlarmIntervalM;
	if (_AlarmIntervalM.find(internal_dataS.CameraId) == _AlarmIntervalM.end())
	{
		gettimeofday(&tv, NULL);
		_AlarmIntervalM[internal_dataS.CameraId] = tv.tv_sec - 60*60*24;
	}

	/** @todo 图片处理 */
	bool onceAlarm = false;
	std::string imagePath;
    for (int i = 0; i < internal_dataS.AdditionInfoV.size(); i++)
    {
		const AdditionalData& addition_info = internal_dataS.AdditionInfoV[i];
        
		if (!addition_info.IsAlarm) continue;

		const AlgorithmResult& detect_info = internal_dataS.DetectBoxV[i];
		cv::Rect rect = cv::Rect(detect_info.rect.x,detect_info.rect.y,detect_info.rect.width,detect_info.rect.height);
		cv::rectangle(internal_dataS.mat,rect,cv::Scalar(0,0,255),2);

		cv::Ptr<cv::freetype::FreeType2> ft2;
		ft2 = cv::freetype::createFreeType2();
		ft2->loadFontData("/usr/share/fonts/truetype/gsafety/fangsong.ttf", 0);
		if (!addition_info.Name.empty() || !addition_info.Department.empty())
		{
			std::string infos = addition_info.Name + " : " + addition_info.Department;
			cv::Point point(detect_info.rect.x, detect_info.rect.y - 3);
			ft2->putText(internal_dataS.mat, infos, point,25, cv::Scalar(0,0,255),1,8,true);
		}
		onceAlarm = true;
	}

	if (!onceAlarm) return;

	gettimeofday(&tv, NULL);
	if (tv.tv_sec - _AlarmIntervalM[internal_dataS.CameraId] < internal_dataS.AlarmInterval)
		return;

	_AlarmIntervalM[internal_dataS.CameraId] = tv.tv_sec;
	
	//保存图片到存储服务
	if (!savePicture(imagePath, internal_dataS.mat))
	{
		LOG4CPLUS_ERROR("root",  internal_dataS.CurrentId << ":" << internal_dataS.CameraId << "->HM Save failure alarm picture");
		return;
	}

	/** @todo 告警送至TM或者httpserver; METADATATYPE=ALARMH */
	Json::Value root;
    Json::StreamWriterBuilder writerBuilder;
    std::ostringstream os;

    root["Type"] = ALARMH;
    root["TaskId"] = internal_dataS.CurrentId;
    root["AlarmId"] = getUuid(true);
    root["CameraId"] = internal_dataS.CameraId;
    root["CameraName"] = internal_dataS.CameraName;
    root["TimeStamp"] = tv.tv_sec;
    root["ImagePath"] = imagePath;

    std::unique_ptr<Json::StreamWriter> jsonWriter(writerBuilder.newStreamWriter());
    jsonWriter->write(root, &os);
    std::string msg = os.str();
	
	//I_FACERESULT_VIDEO、I_VEHICLERESULT_VIDEO、I_CROWDDENSITY_VIDEO
	if (I_FACERESULT_VIDEO == internal_dataS.idot || I_VEHICLERESULT_VIDEO == internal_dataS.idot)
	{
		LOG4CPLUS_INFO("root", "HM send TO TM->" << internal_dataS.CameraId);
		zmq_mgmt->SendMessage(m_zmqIndex_task, msg, msg.size());
		zmq_mgmt->SendMessage(camera_index, msg, msg.size());
	}
	//I_FACERESULT_HTTP、I_VEHICLERESULT_HTTP、I_CROWDDENSITY_HTTP
	else if (I_FACERESULT_HTTP == internal_dataS.idot || I_VEHICLERESULT_HTTP == internal_dataS.idot)
	{
		LOG4CPLUS_INFO("root", "HM send TO HM->" << internal_dataS.CameraId);
		zmq_mgmt->SendMessage(m_zmqIndex_analysis, msg, msg.size(), false);
	}
}

bool HttpServerManager::savePicture(std::string& path, const cv::Mat& frame)
{
    /** @brief 发送至fastdfs */
    std::vector<uchar> buff;
    int MAX_MODEL_IMAGE_SIZE=frame.cols* frame.rows*frame.channels();
    char *modelImage = new char[MAX_MODEL_IMAGE_SIZE]; //char数组

    bool flag=cv::imencode(".jpg", frame, buff);
    memcpy(modelImage, reinterpret_cast<char*>(&buff[0]), buff.size());
    
    if (GsFastDfsManager::GetInstance()->uploadFile(modelImage, buff.size(), path))
    {
        delete []modelImage;
        modelImage = nullptr;
        return false;
    }
        
    delete []modelImage;
    modelImage = nullptr;
    return true;
}

void HttpServerManager::assembleInternalDataset(InternalDataSet &internalDataSet, HttpPlatFormData &httpPlatformData)
{
	internalDataSet.CameraId = httpPlatformData.getCameraID();
	internalDataSet.CameraName = httpPlatformData.getCameraName();
	internalDataSet.AlgorithmType = baseData.AlgorithmType;
	internalDataSet.CurrentId = httpPlatformData.getCurrentID();
	vector<FaceAdditional> faceAdditionalVec = httpPlatformData.getFaceAdditionalInfo();
	for (size_t i = 0; i < faceAdditionalVec.size(); i++)
	{
		AdditionalData faceAdditionalData;
		faceAdditionalData.Id = faceAdditionalVec[i].getFaceID();
		faceAdditionalData.Name = faceAdditionalVec[i].getName();
		faceAdditionalData.Department = faceAdditionalVec[i].getDepartment();
		internalDataSet.AdditionInfoV.push_back(faceAdditionalData);
	}

	internalDataSet.TrustFlag = httpPlatformData.getTrustFlag();
	string imageData;
	httpPlatformData.setImageData(imageData);
		
}

string HttpServerManager::getHttpHeaderNameValue(http_message *message, string &name)
{
	string response;
	for(int i = 0; i < MG_MAX_HTTP_HEADERS; i++)
	{
		string node(message->header_names[i].p, message->header_names[i].len);
		if(name != node)
		{
			continue;
		}

		response = string(message->header_values[i].p, message->header_values[i].len);
		size_t index = response.find(";");
		if(index == string::npos)
		{
			continue;
		}

		response = response.substr(0, index);

	}
	return response;
}