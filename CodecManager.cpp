
#include "CodecManager.h"
#include "DataAnalysis.hpp"
#include "GsFastDfsManager.h"
#include <json/json.h>
using namespace Json;

#define SEND_INTERVAL 60 * 1000 //发送时间间隔1分钟
#define FAILED_NUMBER 60        //60帧解码或者编码

CDecodeParam::CDecodeParam(/* args */)
{
    videoFps = 0;
    decodeTime = 0.0;
    skipFrameFrequency = 0;
}

CDecodeParam::~CDecodeParam()
{
}

void CDecodeParam::setDecodeID(string &decodeID)
{
    this->decodeID = decodeID;
}

string CDecodeParam::getDecodeID()
{
    return decodeID;
}

void CDecodeParam::setVideoFps(int fps)
{
    videoFps = fps;
}

int CDecodeParam::getVideoFps()
{
    return videoFps;
}

void CDecodeParam::setDecodeTime(double decodeTime)
{
    this->decodeTime = decodeTime;
}

double CDecodeParam::getDecodeTime()
{
    return decodeTime;
}

void CDecodeParam::setSkipFrameFrequency(uint32_t skipFrameFrequency)
{
    this->skipFrameFrequency = skipFrameFrequency;
}

uint32_t CDecodeParam::getSkipFrameFrequency()
{
    return skipFrameFrequency;
}

void CDecodeParam::setCameraID(string &cameraID)
{
    this->cameraID = cameraID;
}

string CDecodeParam::getCameraID()
{
    return cameraID;
}

string CDecodeParam::buildDecodeParamToString()
{
    Value root;
    StreamWriterBuilder writerBuilder;
    root["cameraId"] = cameraID;
    root["decodeId"] = decodeID;
    root["videoFPS"] = videoFps;
    root["decodeFPS"] = decodeTime;
    root["SkipFrameFrequency"] = skipFrameFrequency;

    ostringstream os;
    unique_ptr<StreamWriter> jsonWriter(writerBuilder.newStreamWriter());
    jsonWriter->write(root, &os);
    return os.str();
}

bool CDecodeParam::parseDecodeParam(string &message)
{
    Value root;
    CharReaderBuilder readerBuilder;
    unique_ptr<CharReader> reader(readerBuilder.newCharReader());
    JSONCPP_STRING err;
    bool res = reader->parse(message.c_str(), message.c_str() + message.length(), &root, &err);
    if (!res || !err.empty())
    {
        return false;
    }

    cameraID = root["cameraId"].asString();
    decodeID = root["decodeId"].asString();
    videoFps = root["videoFPS"].asInt();
    decodeTime = root["decodeFPS"].asDouble();
    skipFrameFrequency = root["SkipFrameFrequency"].asInt();
    return true;
}

CEncodeParam::CEncodeParam()
{
    
    fps = 0;
    encodeAvgTime = 0.0;
}

CEncodeParam::~CEncodeParam()
{
}

void CEncodeParam::setFPS(const int fps)
{
    this->fps = fps;
}

int CEncodeParam::getFPS()
{
    return fps;
}

void CEncodeParam::setEncodeAvgTime(const double avgTime)
{
    this->encodeAvgTime = avgTime;
}

double CEncodeParam::getEncodeAvgTime()
{
    return encodeAvgTime;
}

void CEncodeParam::setEncodeID(const string &encodeID)
{
    this->encodeID = encodeID;
}

string CEncodeParam::getEncodeID()
{
    return encodeID;
}

void CEncodeParam::setCameraID(const string &cameraID)
{
    this->cameraID = cameraID;
}

string CEncodeParam::getCameraID()
{
    return cameraID;
}

string CEncodeParam::buildEncodeParamToString()
{
    Value root;
    StreamWriterBuilder writerBuilder;
    root["cameraId"] = cameraID;
    root["encodeId"] = encodeID;
    root["fps"] = fps;
    root["encodeAvgTime"] = encodeAvgTime;
    ostringstream os;
    unique_ptr<StreamWriter> jsonWriter(writerBuilder.newStreamWriter());
    jsonWriter->write(root, &os);

    return os.str();
}

bool CEncodeParam::parseEncodeParam(string &message)
{
    CharReaderBuilder readerBuilder;
    unique_ptr<CharReader> reader(readerBuilder.newCharReader());
    Value root;
    JSONCPP_STRING err;
    bool res = reader->parse(message.c_str(), message.c_str() + message.length(), &root, &err);
    if (!res || !err.empty())
    {
        cout << "parse encode param failed!" << endl;
        return false;
    }

    cameraID = root["cameraId"].asString();
    encodeID = root["encodeId"].asString();
    fps = root["fps"].asInt();
    encodeAvgTime = root["encodeAvgTime"].asDouble();
    return true;
}

CodecManager::CodecManager()
{
    decodeInit = false;
    serverIsRun = false;
    encodeServiceRun = false;
    destURLChanged = false;
    zmqAIDecodeIndex = -1;
    zmqAIEncodeIndex = -1;
    zmqDecodeAlarmIndex = -1;
    zmqEncodeAlarmIndex = -1;
}

CodecManager::~CodecManager()
{
    LOG4CPLUS_INFO("root", "CM source release: " << cameraData.CameraId);
}

void CodecManager::setDecodeInfo(CameraData &cameraData)
{
    decodeInfo.setURL(cameraData.CameraUrl);
    decodeInfo.setHardwareAccelerate(true);
    decodeInfo.setHWDeviceType(HWDEVICE_TYPE_CUDA);
    decodeInfo.setPushStream(cameraData.PushFlow);
    decodeInfo.setFileDuration(cameraData.SaveVideoDuration);
    decodeInfo.setVideoStopTime(cameraData.EndTimeStamp);
    decodeInfo.setSkipFrameFrequency(cameraData.SkipFrameFrequency);
    decodeInfo.setGpuID(1);
}

void CodecManager::setEncodeInfo(CameraData &cameraData)
{
    string urlName(cameraData.DestUrl);
    if (cameraData.DestUrl.rfind("/") == (cameraData.DestUrl.size() - 1)) //最后一个字符是"/"
    {
        urlName += cameraData.CameraId;
    }
    else
    {
        urlName += "/";
        urlName += cameraData.CameraId;
    }
   
    encodeInfo.setFileName(urlName.c_str());
    encodeInfo.setFps(decodeInfo.getFps());
    encodeInfo.setVideoIndex(decodeInfo.getVideoIndex());
    encodeInfo.setVideoWidth(decodeInfo.getVideoWidth());
    encodeInfo.setVideoHeight(decodeInfo.getVideoHeight());
    encodeInfo.setVideoInGpu(true);
    encodeInfo.setHardwareAcceleration(true);
    encodeInfo.setHwDeviceType(HWDEVICE_TYPE_CUDA);
    encodeInfo.setIsVideo(true);
    encodeInfo.setProtocolName("RTMP");
    encodeInfo.setNumerator(decodeInfo.getNumerator());
    encodeInfo.setDenominator(decodeInfo.getDenominator());
    encodeInfo.setGpuID(1);
}

bool CodecManager::initAudioAndVideoCodec(CameraData &cameraData, shared_ptr<ZmqManager> &zmqObject, map<string, IndexData> &IndexDataMap)
{
    zmqManager = zmqObject;
    this->cameraData = cameraData;
    setDecodeInfo(cameraData);

    taskZmqIndex = IndexDataMap["dad4668bc0b8e79cd460bf9f0e0a9700"].index;
    
    if (!decoder.initializeDecodeInformation(decodeInfo))
    {
    //    codecMessage.sendMessage(zmqManager, taskZmqIndex, cameraData.dot, FAILED, cameraData.CameraId, cameraData.CurrentId);
        return false;
    }
    auto ptr = make_shared<MatType>(decodeInfo.getVideoWidth(), decodeInfo.getVideoHeight(), CV_8UC3);
    IndexDataMap[cameraData.CameraId].DecodeGpu.swap(ptr);
    ptr = make_shared<MatType>(decodeInfo.getVideoWidth(), decodeInfo.getVideoHeight(), CV_8UC3);
    string encodeID = cameraData.CameraId + "E";
    IndexDataMap[encodeID].EncodeGpu.swap(ptr);
    decoder.setVideoFileDuration(decodeInfo.getFileDuration());
    decodeInit = true;
    serverIsRun = true;
    return true;
}

void CodecManager::audioAndVideoDecoder(IndexData &decodeAndAIData, IndexData &decodeAndAlarmData)
{
    if (decodeAndAIData.index < 0 ||
        decodeAndAlarmData.index < 0)
    {
        return;
    }

    uint64_t zmqAIDecodeIndexRfd = 0, zmqDecodeAlarmIndexRfd = 0;
    uint8_t *data[NUM_DATA_POINTERS];
    int lineSize[NUM_DATA_POINTERS];
    bool isVideo = false, startTime = true, firstDecode = true, receiveResult = false;
    double timeBegin = 0.0, timeEnd = 0.0;

    zmqAIDecodeIndexRfd |= (uint64_t)0x0000000000000001 << decodeAndAIData.index;
    zmqDecodeAlarmIndexRfd |= (uint64_t)0x0000000000000001 << decodeAndAlarmData.index;
    zmqAIDecodeIndex = decodeAndAIData.index;
    zmqDecodeAlarmIndex = decodeAndAlarmData.index;
    decodeFrame = decodeAndAIData.DecodeGpu;

//    thread uploadFileThread = thread(bind(&CodecManager::uploadFileToFastDfs, this));
    //uploadFileThread.detach();
    while (serverIsRun)
    {
        string receive_data;
        if (firstDecode && cameraData.dot == D_ADD) //如果是第一次解码
        {
            codecMessage.sendMessage(zmqManager, taskZmqIndex, cameraData.dot, SUCCESS, cameraData.CameraId, cameraData.CurrentId);
        }

        if (startTime)
        {
            timeBegin = Time::getTimeToMillisecond();
        }
        receive_data.clear();
        if (!firstDecode)//接收来自AI和平台的的消息
        {
            receiveResult = receiveData(zmqAIDecodeIndexRfd, decodeAndAIData.index, receive_data, false);
            if (!receiveResult)//没有接收到消息
            {
                continue;
            }
        }
        if (!receive_data.empty() && receive_data != "1") //平台发送
        {
            AlarmResult alarmResult;
            DataAnalysis dataAnalysis;
            CameraData cameraDataResult;
            int _type;
            std::string msg;

            dataAnalysis.InDataDeserialize(receive_data, cameraDataResult, alarmResult, _type);
            
            if (_type == CODEC)
            {
                if (cameraDataResult.dot == D_DEL) //停止解码
                {
                    serverIsRun = false;
                    encodeServiceRun = false;
                    decoder.stop();
                    cameraData.dot = cameraDataResult.dot;
                    LOG4CPLUS_INFO("root", "CM from TM Instruction (Delete) recvMsg:\n" << receive_data);
                }
                else if (cameraDataResult.dot == D_VMODIFY) //改变解码参数
                {
                    modifyCameraParam(cameraDataResult);
                    LOG4CPLUS_INFO("root", "CM from TM Instruction (Modify) recvMsg:\n" << receive_data);
                }
                cameraDataResult.Status = SUCCESS;
                dataAnalysis.InDataSerialize(msg, cameraDataResult, alarmResult, _type);
                if (cameraDataResult.dot == D_DEL) 
                    LOG4CPLUS_INFO("root", "CM to TM Instruction (Delete) Data:\n" << msg);
                else if (cameraDataResult.dot == D_VMODIFY) 
                    LOG4CPLUS_INFO("root", "CM to TM Instruction (Modify) Data:\n" << msg);
                zmqManager->SendMessage(taskZmqIndex, msg, msg.size());
            }
            else if(_type == ALARMC)//告警
            {
                LOG4CPLUS_INFO("root", "CM from AI (Alarm) recvMsg:\n" << receive_data);
                decoder.saveAlarmFile(alarmResult.AlarmId);
            }
        }
        else //AI发送
        {
            firstDecode = false;
            int ret = decoder.decodeFrame(data, lineSize, isVideo);
            if(ret >= 0 && isVideo)
            {
                dataToGPUMat(data, lineSize);
                string sendData("1");
                bool sendSendPair = true;
                //发给recv
                zmqManager->SendMessage(decodeAndAIData.index, sendData, sendData.size(), sendSendPair);
            }

            if(ret == -1)//取流失败
            {
                serverIsRun = false;//解码停止
                encodeServiceRun = false;//编码停止
                decoder.stop();//停止写文件
                LOG4CPLUS_INFO("root", "CM to TM (Get VideoStream Fail) Data: " << cameraData.CameraId);
                codecMessage.sendMessage(zmqManager, taskZmqIndex, D_IDEL, SUCCESS, cameraData.CameraId, cameraData.CurrentId);
            }

            if(decoder.stopDecode())//停止解码
            {
                serverIsRun = false;//解码停止
                encodeServiceRun = false;//编码停止
                decoder.stop();//停止写文件
                LOG4CPLUS_INFO("root", "CM to TM (Stop Decode) Data: " << cameraData.CameraId);
                codecMessage.sendMessage(zmqManager, taskZmqIndex, D_IDEL, SUCCESS, cameraData.CameraId, cameraData.CurrentId);
            }
            alarmFinishedVecMutex.lock();
            decoder.getAlarmFinished(alarmFinished);
            alarmFinishedVecMutex.unlock();
            timeEnd = Time::getTimeToMillisecond();
            bool sendResult = sendDataToAlarm(timeEnd, timeBegin, DECODE_MESSAGE);
            if (sendResult)
            {
                startTime = true;
            }

            startTime = false;
        }
    }

//    uploadFileThread.join();

    LOG4CPLUS_INFO("root", "Decode end: " << cameraData.CameraId);
}

void CodecManager::audioAndVideoEncoder(IndexData &encodeAndAIData, IndexData &encodeAndAlarmData)
{
    if (encodeAndAIData.index < 0 ||
        encodeAndAlarmData.index < 0)
    {
        return;
    }

    bool encodeInit = false;
    while (true)
    {
        int count = 0;
        if (!decodeInit)
        {
            this_thread::sleep_for(chrono::seconds(1));
            count++;
            continue;
        }

        encodeInit = initEncodeInfo();
        break;
    }

    // if (!encodeInit)
    // {
    //     //通知平台编码信息初始化失败
    //     codecMessage.sendMessage(zmqManager, taskZmqIndex, cameraData.dot, FAILED, cameraData.CameraId, cameraData.CurrentId);
    //     return;
    // }
    // else
    // {
    //     codecMessage.sendMessage(zmqManager, taskZmqIndex, cameraData.dot, SUCCESS, cameraData.CameraId, cameraData.CurrentId);
    // }

    encodeServiceRun = true;
    //发送消息
     uint64_t zmqAIEncodeIndexRfd = 0, zmqEncodeAlarmIndexRfd = 0;
     this->zmqAIEncodeIndex = encodeAndAIData.index;
     this->zmqEncodeAlarmIndex = encodeAndAlarmData.index;
     this->encodeFrame = encodeAndAIData.EncodeGpu;
     bool startTime = true, firstEncode = true, receiveResult = false;
     double timeBegin = 0.0, timeEnd = 0.0;
     zmqAIEncodeIndexRfd |= (uint64_t)0x0000000000000001 << zmqAIEncodeIndex;
     zmqEncodeAlarmIndexRfd |= (uint64_t)0x0000000000000001 << zmqEncodeAlarmIndex;
    while (encodeServiceRun)
    {
        if (cameraData.dot == D_VMODIFY && destURLChanged)
        {
            encoder.releaseReSource();
            encodeInit = initEncodeInfo();
            if (!encodeInit) //初始化失败
            {
                codecMessage.sendMessage(zmqManager, taskZmqIndex, D_VMODIFY, FAILED, cameraData.CameraId, cameraData.CurrentId);
                break;
            }
            else //初始化成功
            {
                codecMessage.sendMessage(zmqManager, taskZmqIndex, D_VMODIFY, SUCCESS, cameraData.CameraId, cameraData.CurrentId);
            }
            destURLChanged = false;
            continue;
        }

        if (startTime)
        {
            timeBegin = Time::getTimeToMillisecond();
        }
        string receive_data;
        receiveResult = receiveData(zmqAIEncodeIndexRfd, zmqAIEncodeIndex, receive_data, false);
        if (!receiveResult)
        {
            continue;
        }
        uint8_t *data[NUM_DATA_POINTERS] = {0};
        int lineSize[NUM_DATA_POINTERS] = {0};
        bool gpuMatResult = gpuMatToData(data, lineSize);
        if(!gpuMatResult)
        {
            continue;
        }
        int ret = encoder.encodeFrame(data, lineSize);
        if(ret < 0)
        {
            //codecMessage.sendMessage(zmqManager, taskZmqIndex, cameraData.dot, FAILED, cameraData.CameraId, cameraData.CurrentId);
            encodeServiceRun = false;
        }
        timeEnd = Time::getTimeToMillisecond();
        bool sendResult = sendDataToAlarm(timeEnd, timeBegin, ENCODE_MESSAGE);
        if (sendResult)
        {
            startTime = true;
        }

        startTime = false;
    }

    if (cameraData.dot == D_VMODIFY && !cameraData.PushFlow)
    {
        codecMessage.sendMessage(zmqManager, taskZmqIndex, cameraData.dot, SUCCESS, cameraData.CameraId, cameraData.CurrentId);
    }

    // if (cameraData.dot == D_DEL)
    // {
    //     codecMessage.sendMessage(zmqManager, zmqAIDecodeIndex, cameraData.dot, SUCCESS, cameraData.CameraId, cameraData.CurrentId);
    // }

    LOG4CPLUS_INFO("root", "Encode end: " << cameraData.CameraId);
}

void CodecManager::dataToGPUMat(uint8_t **data, int *lineSize)
{
    Mat cpuMat(decodeInfo.getVideoHeight(), decodeInfo.getVideoWidth(), CV_8UC3, data[0], lineSize[0]);
    decodeFrame->upload(cpuMat);
}

bool CodecManager::gpuMatToData(uint8_t **data, int *lineSize)
{
    cpuMat.release();
    encodeFrame->download(cpuMat);
    if (cpuMat.empty())
    {
        return false;
    }
    //cv::imwrite("/opt/2.jpg", cpuMat);
    data[0] = cpuMat.data;
    lineSize[0] = cpuMat.cols * cpuMat.channels();

    return true;
}

bool CodecManager::sendDataToAlarm(double timeEnd, double timeBegin, messageType type)
{
    if (timeEnd - timeBegin < SEND_INTERVAL)
    {
        return false;
    }

    if (type == ENCODE_MESSAGE) //发送编码信息
    {
        CDecodeParam decodeParam;
        decodeParam.setCameraID(cameraData.CameraId);
        string decodeID = decoder.getDecodeID();
        decodeParam.setDecodeID(decodeID);
        decodeParam.setVideoFps(decodeInfo.getFps());
        decodeParam.setDecodeTime(decoder.calculateDecodeAverageTime());
        string sendData(decodeParam.buildDecodeParamToString());
        zmqManager->SendMessage(zmqDecodeAlarmIndex, sendData, sendData.size());
    }
    else if (type == DECODE_MESSAGE) //发送解码信息
    {
        CEncodeParam encodeParam;
        encodeParam.setCameraID(cameraData.CameraId);
        string encodeID = encoder.getEncodeID();
        encodeParam.setEncodeID(encodeID);
        encodeParam.setFPS(decodeInfo.getFps());
        encodeParam.setEncodeAvgTime(encoder.calculateEncodeAverageTime());
        string sendData(encodeParam.buildEncodeParamToString());
        zmqManager->SendMessage(zmqEncodeAlarmIndex, sendData, sendData.size());
    }

    return true;
}

bool CodecManager::receiveData(uint64_t rfd, uint32_t zmqIndex, string &receive_data, bool prw)
{
    size_t receiveDataLen = 0;
    uint64_t bit = zmqManager->MessagePool(rfd, prw);
    if (bit == 0)
    {
        return false;
    }

    bool receiveResult = zmqManager->ReceiveMessage(zmqIndex, receive_data, receiveDataLen, prw);
    if (!receiveResult)
    {
        return false;
    }

    return true;
}

bool CodecManager::initEncodeInfo()
{
    setEncodeInfo(cameraData);
    if (!encoder.initializeEncodeInformation(encodeInfo))
    {
        return false;
    }
    return true;
}

void CodecManager::modifyCameraParam(CameraData &cameraData)
{
    if (this->cameraData.dot != cameraData.dot)
    {
        this->cameraData.dot = cameraData.dot;
    }

    if (this->cameraData.DestUrl != cameraData.DestUrl) //推流地址改变,需要重新初始化编码信息
    {
        this->cameraData.DestUrl = cameraData.DestUrl;
        destURLChanged = true;
    }

    if (this->cameraData.EndTimeStamp != cameraData.EndTimeStamp) //推流截止时间改变
    {
        this->cameraData.EndTimeStamp = cameraData.EndTimeStamp;
        decoder.setStopTime(cameraData.EndTimeStamp);
    }

    if (this->cameraData.PushFlow != cameraData.PushFlow) //是否推流改变
    {
        this->cameraData.PushFlow = cameraData.PushFlow;
        if (cameraData.PushFlow == false) //原来是编码了，现在不需要编码了，则停掉编码
        {
            encodeServiceRun = false;
        }
    }

    if (this->cameraData.SkipFrameFrequency != cameraData.SkipFrameFrequency &&
        cameraData.SkipFrameFrequency > 0) //跳帧频率改变
    {
        decoder.setFrameSkip(true, cameraData.SkipFrameFrequency);
        this->cameraData.SkipFrameFrequency = cameraData.SkipFrameFrequency;
    }

    if(this->cameraData.SaveVideoDuration != cameraData.SaveVideoDuration &&
       cameraData.SaveVideoDuration > 0)
       {
           decoder.setVideoFileDuration(cameraData.SaveVideoDuration);
           this->cameraData.SaveVideoDuration = cameraData.SaveVideoDuration;
       }
}

void CodecManager::uploadFileToFastDfs()
{
    GsFastDfsManager *fastDfsManager = GsFastDfsManager::GetInstance();   
    while(serverIsRun)
    {
        vector<string> finishedAlarmFile;
        alarmFinishedVecMutex.lock();
        if(alarmFinished.empty())
        {
            alarmFinishedVecMutex.unlock();
            this_thread::sleep_for(chrono::seconds(1));
            continue;
        }
        finishedAlarmFile.insert(finishedAlarmFile.end(), alarmFinished.begin(), alarmFinished.end());
        alarmFinished.clear();
        alarmFinishedVecMutex.unlock();
        DataAnalysis dataAnalysis;

        string msg;
        //上传文件
        for(auto iter = finishedAlarmFile.begin(); iter != finishedAlarmFile.end(); iter++)
        {
            string fileName;
            fileName += iter->c_str();
            fileName += ".mp4";
            //上传文件
            string uploadFilePath;
            bool result = fastDfsManager->uploadVideo((char *)fileName.c_str(), uploadFilePath);
            //组装消息并发送消息
            if(!result)
            {
                LOG4CPLUS_INFO("root", "file upload failed: " << fileName);
                continue;
            }
            CameraData cameraDataResult;
            AlarmResult alarmResult;
            alarmResult.AlarmId = *iter;
            alarmResult.CameraId = cameraData.CameraId;
            alarmResult.CameraName = cameraData.CameraName;
            alarmResult.CurrentId = cameraData.CurrentId;
            alarmResult.VideoPath = uploadFilePath;
            alarmResult.Type = ALARMC;
            dataAnalysis.InDataSerialize(msg, cameraDataResult, alarmResult, ALARMC);
            zmqManager->SendMessage(taskZmqIndex, msg, msg.size());
            remove(fileName.c_str());
        }
        finishedAlarmFile.clear();
    }
    LOG4CPLUS_INFO("root", "uploadFileToFastDfs end: " << cameraData.CameraId);
}