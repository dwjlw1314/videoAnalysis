#ifndef _CODEC_MANAGER_H_

#define _CODEC_MANAGER_H_

#include "AudioAndVideoDecode.h"
#include "AudioAndVideoEncode.h"
#include "DecodeInformation.h"
#include "EncodeInformation.h"
#include "InternalDataSet.h"
#include "ZmqManager.hpp"
#include "Time.h"
#include "CodecMessage.h"

#include <thread>
#include <memory>
#include <string>
#include <map>
#include <vector>
#include <mutex>
#include <functional>
using namespace std;

#include "opencv2/opencv.hpp"
using namespace cv;
using namespace cv::cuda;

typedef enum messageType 
{
    ENCODE_MESSAGE = 0,
    DECODE_MESSAGE = 1
}messageType;

class CDecodeParam
{
public:
    CDecodeParam();
    ~CDecodeParam();

public:
    void setDecodeID(string &decodeID);
    string getDecodeID();
    void setVideoFps(int fps);
    int getVideoFps();
    void setDecodeTime(double decodeTime);
    double getDecodeTime();
    void setSkipFrameFrequency(uint32_t skipFrameFrequency);
    uint32_t getSkipFrameFrequency();
    void setCameraID(string &cameraID);
    string getCameraID();

    string buildDecodeParamToString();
    bool parseDecodeParam(string &message);

private:   
    int videoFps;
    double decodeTime;
    uint32_t skipFrameFrequency;
    string decodeID;
    string cameraID;
};

class CEncodeParam
{
public:
    CEncodeParam();
    ~CEncodeParam();

public:
    void setFPS(const int fps);
    int getFPS();
    void setEncodeAvgTime(const double avgTime);
    double getEncodeAvgTime();
    void setEncodeID(const string &encodeID);
    string getEncodeID();
    void setCameraID(const string &cameraID);
    string getCameraID();

    string buildEncodeParamToString();
    bool parseEncodeParam(string &message);
    
private:
    int fps;
    double encodeAvgTime;
    string encodeID;
    string cameraID;
};


class CodecManager
{
public:
    CodecManager();
    ~CodecManager();

public:
    /**
     *@brief 初始化编解码信息
     *@param cameraData 摄像头的信息
     *@param zmqObject zeroMQ 对象
    */
    bool initAudioAndVideoCodec(CameraData &cameraData, shared_ptr<ZmqManager>& zmqObject, map<string, IndexData> &IndexDataMap);
    /**
     * @brief 解码线程
     * 
     * @param decodeAndAIData decode和AI之间的数据 
     * @param decodeAndAlarmData decode和告警信息之间的数据
     */
    void audioAndVideoDecoder(IndexData &decodeAndAIData, IndexData &decodeAndAlarmData);
    /**
     * @brief 编码推流
     * 
     * @param encodeAndAIData encode和AI之间的数据 
     * @param encodeAndAlarmData encode和告警信息之间的数据
     */
    void audioAndVideoEncoder(IndexData &encodeAndAIData, IndexData &encodeAndAlarmData);
    /**
     * @brief 上传文件到fastDfs
     * 
     */
    void uploadFileToFastDfs();
private:
    /**
     *@brief 解码后的数据转换成GpuMat
     *@param data 解码的后数据
     *@param lineSize 解码后的数据的大小
    */
    void dataToGPUMat(uint8_t **data, int *lineSize);
    //GpuMat数据转化为要编码的数据
    /**
     *@brief GpuMat转换成要编码的数据
     *@param data 要编码的数据
     *@param lineSize 数据的大小
    */
    bool gpuMatToData(uint8_t **data, int *lineSize);
    /**
     *@brief 是否要发送数据给报警端
     *@param timeEnd 结束时间
     *@param timeBegin 开始时间
    */
    bool sendDataToAlarm(double timeEnd, double timeBegin, messageType type);
    /**
     *@brief 设置编码信息
     *@param cameraData 摄像头的信息
    */
    void setEncodeInfo(CameraData &cameraData);
    /**
     *@brief 设置解码信息
     *@param cameraData 摄像头的信息
    */
    void setDecodeInfo(CameraData &cameraData);
    /**
     * @brief 接收数据是否成功
     * @param rfd 监听的rfd
     * @param receive_data 接收到的数据
     * @param prw true是recv，false是send
     */
    bool receiveData(uint64_t rfd, uint32_t zmqIndex, string &receive_data, bool prw);
    /**
     * @brief 初始化编码信息
     * 
     * @return true success
     * @return false failed
     */
    bool initEncodeInfo();

    /**
     * @brief 修改摄像头的参数
     * 
     * @param cameraData 摄像头的数据
     */
    void modifyCameraParam(CameraData &cameraData);
private:
    bool encodeServiceRun;//编码服务是否运行
    bool serverIsRun;
    bool decodeInit;//解码是否初始化
    bool destURLChanged;
    uint32_t zmqAIDecodeIndex;
    uint32_t zmqAIEncodeIndex;
    uint32_t zmqDecodeAlarmIndex;
    uint32_t zmqEncodeAlarmIndex;
    uint32_t taskZmqIndex;
    shared_ptr<MatType> decodeFrame;
    shared_ptr<MatType> encodeFrame;
    DecodeInformation decodeInfo;
    EncodeInformation encodeInfo;
    AudioAndVideoDecode decoder;
    AudioAndVideoEncode encoder;
    std::shared_ptr<ZmqManager> zmqManager;
    CameraData cameraData;
    CodecMessage codecMessage;
    Mat cpuMat;
    mutex alarmFinishedVecMutex;
    vector<string> alarmFinished;
};



#endif