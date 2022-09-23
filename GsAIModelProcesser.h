
#ifndef _GsAIModelProcesser_H
#define _GsAIModelProcesser_H

//烟火，安全帽，值班员脱岗，反光衣检测算法
#include "helmetdetect.h"
#include "yololayer.h"
//车辆检测算法
#include "class_detector.h"
#include "plate_detector.h"
#include "VPlateRRT.h"
//皮带跑偏算法
#include "trackIntegration.h"
//人群密度算法
//#include "denseSDK.h"
#include "counter.h"
//人脸识别算法
#include "gsFaceSDK.h"

#include "GsafetyLogLibrary.hpp"
#include "InternalDataSet.h"
#include "DataAnalysis.hpp"
#include "ZmqManager.hpp"
#include "MongoDB.h"
#include "SDKTime.h"

#include "AlarmManager.h"
#include "GsFastDfsManager.h"

#include <memory>
#include <vector>
#include <cuda.h>
#include <thread>
#include <json/json.h>

using namespace std;
using namespace log4cplus;
using namespace log4cplus::helpers;

/** @brief 人密使用参数 */
#define INPUT_TYPE 2    //cvMat 1、cvGpuMat 2
#define OUTPUT_TYPE 4	//hostPtr 3、 gpuPtr 4

class gsFaceSDK;
#define INPUTGPUMAT	    // face using cv::cuda::GpuMat as input data
/* gsFaceSDK init parameter */
#define GET_FACE_TYPE FACEALL				    	// gsFaceSKD switch for optional models combinations, default FACEALL
#define FACEDET_NMS_TSD 0.4							// face detect nms threshold, default 0.4
#define FACEDET_TSD 0.9								// face detect accuracy threshold, default 0.9
#define FACEDET_DEVICE_ID 0							// face detect model loaded to which GPU device, default 0
#define FACEDET_BTS	1								// face detect forward batchsize, default 1
#define FACEDET_MOD_PTH	"detect_19201080"	        // face detect model file name, default "detect/detect_19201080" or "detect/detect_360540"
#define FEAEXT_DEVICE_ID 0							// face feature model loaded to which GPU device, default 0
#define FEAEXT_BTS 1 								// face feature forward batchsize, default 1
#define FEAEXT_MOD_PTH "feature_101"		        // face feature model file name, default "feature/feature_101" or "feature/feature_50"
#define TRACK_NN_BUDGET 100							// face track maximum tracking objects, default 100
#define TRACK_COS_DIST 0.2							// face track feature similarity threshold, default 0.2
#define TRACK_IOU_DIST 0.7							// face track IOU distance threshold, default 0.7
#define TRACK_MAX_AGE 30							// face track max age before deleted, default 30
#define TRACK_N_INIT 3								// face track init steps before confirming, default 3
#define TRACK_DEVICE_ID 0							// face track model loaded to which GPU device, default 0
#define	TRACK_BTS 1									// face track forward batchsize, default 1
#define TRACK_MOD_PTH "track"					    // face track model file name, default "track/track"
#define POSE_DEVICE_ID 0							// face pose model loaded to which GPU device, default 0
#define POSE_BTS 1									// face pose forward batchsize, default 1
#define POSE_MOD_PTH "pose"					        // face pose model file name, default "pose/pose"
#define INPUT_TYPE 2								// input data type, default 1
#define WIDTH_SIZE_INPUT 1920						// input image width. default 1920
#define	SCAL_RATIO FACTOR_16_9						// input image scale factor, default W/H = 16/9
/** gsFaceSDK parameter end */

using GsAIModelProcesserHander=std::function<void(void)>;

typedef helmetdetect::HELMETDETECT helmetDetectEngine;

class GsAIModelProcesser
{
public:
    GsAIModelProcesser(ZeromqIndexDataMap& zero_index_dataM, TrustDataTableMap& trust_dataM);
    ~GsAIModelProcesser();

private:
   
    /**
     * @brief 初始化算法资源
     * 
     * @param modelPath 模型路径
     * @param alarmType 算法类型
     * @param ad 算法参数集合
     * @param deviceID 设备id
     * @return int 
     */
    int initYolov5(const std::string &modelPath, int alarmType, const AlgorithmData& ad, int deviceID=0);
    
    /**
     * @brief 做infer 结果存储在outputs里面
     * 
     * @param inputImage std::vector<MatType>类型的入参
     * @param outputRes std::vector<std::vector<DetectioRes>>出参
     * @param nms 过滤nms
     * @param conf 过滤置信度
     * @return int 
     */
    int inferYolov5(std::vector<cv::cuda::GpuMat>& inputImage,DetectioResV &outputRes,float nms,float conf);

    /**
     * @brief 画框

     * @param AIRes AI检测结果集
     * @param dvec 解码图片数据
     * @param evec 编码图片数据
     */
    void drawRectangle(const std::vector<DetectioRes>& AIRes, cv::cuda::GpuMat &dvec, cv::cuda::GpuMat &evec);

    /**
     * @brief 主任务指令反序列化
     * @param msg 序列化消息实体
     * @param algor_dataV 算法数据结构集合
     * @return 解析成功 true, 失败 false
     */
    bool taskDataDeserialize(const std::string& msg, AlgorithmData& algor_dataV);

    /**
     * @brief 主任务指令序列化
     * @param ad 算法数据实体
     * @param msg 序列化后的数据载体,inout类型参数
     */
    void taskDataSerialize(const AlgorithmData& ad, std::string& msg);

    /**
     * @brief 主任务指令处理逻辑
     * @param algor_dataM 平台发送指令数据集合
     * @param index 监听索引二进制位合集
     */
    void taskDataProcess(std::map<int, AlgorithmData>& algor_dataM, uint64_t& index);

    /**
     * @brief 告警处理逻辑
     * @param infer_indexV 解码通信端索引集合 
     * @param inputputRes 算法检测结果
     * @param matV 图片数据集合
     */
    void encodeProcess(std::vector<int> infer_indexV, DetectioResV& inputputRes,
        std::vector<cv::cuda::GpuMat>& matV);

    /**
     * @brief 告警处理逻辑
     * @param path 图片路径,参数是inout类型
     * @param frame 图片数据
     * @return 保存成功 true, 失败 false
     */
    bool savePicture(std::string& path, const cv::Mat& frame);

    /**
     * @brief 告警处理逻辑
     * @param matV 图片数据集合
     * @param infer_indexV 解码通信端索引集合 
     * @param inter_areaV  区域框数据集合
     * @param inputputRes 算法检测结果
     */
    void alarmAnalysis(const std::vector<cv::cuda::GpuMat>& matV,
                       std::vector<int>& infer_indexV,
                       std::vector<InterArea>& inter_areaV,
                       DetectioResV& inResV);
    
    /**
     * @brief 告警推送
     * @param infer_indexV 解码通信端索引集合  
     * @param matV 图片数据集合 
     * @param inputputRes 算法检测结果
     */
    void alarmDelivery(const std::vector<int>& infer_indexV,
                       const std::vector<cv::cuda::GpuMat>& matV,
                       const DetectioResV& inputputRes);

    /** @brief 人脸,车牌告警递送过滤规则 */
    void noAlarmHandle(DetectioResV& detectionResV);

    /** @brief 发送主程序运行资源消耗数据 */
    void performanceAnalysis(uint64 frame);

public:

    /**
     * @brief 初始化资源
     * @param ad 算法类
     * @param zmq_mgmt zmq对象
     * @param alarmid 算法类型
     * @param db_uri  数据库地址
     * @param use_private_library 是否使用本地私有特征库
     * @return 初始化成功 true, 失败 false
     */
    bool initResource(AlgorithmData ad, std::shared_ptr<ZmqManager> zmq_mgmt,
        uint32_t alarmid, std::string db_uri, bool use_private_library);

    /**
     * @brief 模拟发送告警数据, 非正式函数
     */
    void sendAlarmInfo(std::vector<DetectioRes>& res, std::string id);

    /**
     * @brief 图像拼接
     * @param images 拼接图像集
     * @param result 拼接后的图像
     * @param type 拼接方式(0-横向;1-纵向)
     */
    void imageSplicing(vector<cv::Mat> images, cv::Mat& result, int type);

    /**
     * @brief 绘制人群密度图像曲线
     * @param images 绘制图像
     * @param width 输入图宽度
     * @param person_nums 人数
     */
    void drawArcLine(cv::Mat& images, int width, uint32_t person_nums);

    /** @brief 获取对应模型文件路径, 优先读取库表数据 */
    void getModePath(int atype, std::string& path);

    void inferProcessLoop();

private:
    int m_batchsize;
    int m_index_max;
    uint32_t m_zmqIndex_task;
    uint32_t m_zmqIndex_pa;
    uint32_t m_zmqIndex_decode;

    int m_classNum;
    uint32_t m_alarmType;

    DataAnalysis m_DataAnalysis;

    //比对库来源标记
    bool m_privateLib = true;

    //fire,helmet,persion,welding,reflective_closthes
    std::unique_ptr<helmetDetectEngine> m_spHelmetDetect;
    //blet_deviation
    std::unique_ptr<TrackIntegration> m_spTrackIntegration;
    //dense
    //std::unique_ptr<denseSDK> m_spDenseSDK;
    std::unique_ptr<counter::COUNTER> m_spDenseSDK;
    //face
    std::shared_ptr<gsFaceSDK> m_spFaceSDK;
    //vehicle
    std::unique_ptr<TrtDetector> m_spTrtDetector;
    std::unique_ptr<platedetector::PlateDetector> m_spPlateDetector;
    void* vehicle_handle = nullptr;

    //消息通信管理类
    std::shared_ptr<ZmqManager> m_ZmqManager;
    ZeromqIndexDataMap& m_ZermqIndexDataM;
    //信任名单列表
    TrustDataTableMap& m_TrustDataTableM;
    // std::queue<MsgData> m_QueData; 

    AIIndexDataMap m_IndexMap;

    std::string m_AlgorithmId;
    std::string m_DBHost;
    
    int m_Resolution;
    int m_DeviceId;

    float m_nms;
    float m_conf;

    std::shared_ptr<AlarmManager> m_AlarmManager;
    GsFastDfsManager* m_PFastDfsManager;
};

#endif