#ifndef INTERNALDATASET_H
#define INTERNALDATASET_H

#include <iostream>
#include <memory>
#include <atomic>
#include <Eigen/Dense>
#include <opencv2/opencv.hpp>
#include <opencv2/freetype.hpp>
#include <map>

#define TRUST_TOPIC "gsafety_trust"

#define _FEATURESIZE 512
typedef Eigen::Matrix<float, 1, _FEATURESIZE, Eigen::RowMajor> FEATUREM;

#ifndef HAVE_GPU
using MatType = cv::Mat;
#else
using MatType = cv::cuda::GpuMat;
#endif

//指令操作类型
enum DOT
{
    D_ADD        = 0x01,
    D_AMODIFY    = 0x02, //算法参数修改
    D_VMODIFY    = 0x04, //视频参数修改
    D_DEL        = 0x08,
    D_STATUS     = 0x10, //指令执行状态-->递送平台端
    D_NOTICE     = 0x20, //模块内部数据-->递送平台端
    D_RUN_STATUS = 0x40, //程序启动状态-->递送平台端
    D_IDEL       = 0x80, //内部视频删除, 新增摄像头过期释放
};

enum DATASTATE
{
    SUCCESS          = 0x0,
    FAILED           = 0X01,
    //缺少必要字段
    LOSEFIELD        = 0x10,
    //忽略算法id
    IGNOREAID        = 0X11,
    //算法或摄像头已存在
    EXISTED          = 0x12,
    //算法或摄像头不存在
    NOEXISTED        = 0X13,
    //资源不足
    NORESOURCE       = 0x14,
    //超出计算能力
    NOCAPACITY       = 0x15,
    //平台下发指令顺序异常
    INSTRUCTMISMATCH = 0X16,
    //算法修改变更删除操作
    ALGORITHDELETE   = 0x17,
    //平台数据解析失败
    PARSINGERROR     = 0X18
};

//指令操作归属分类
enum METADATATYPE
{
    CODEC      = 0x30,  //编解码类型
    ALGORITHM  = 0x40,  //算法类型
    ALARMA     = 0x50,  //算法告警类型
    ALARMC     = 0x51,  //解码告警类型
    ALARMH     = 0x52,  //HM告警类型
    ANALYSIS   = 0x60,  //性能分析类型
};

class CRect
{
public:
	int32_t x;
	int32_t y;
	int32_t width;
	int32_t height;
};

class InterPoint
{
public:
    float horizontal;
    float ordinate;
};

class InterArea
{
public:
    int width;
    int heigth;
    int interestType;
    bool shutDownArea;
    float thresholdArea;
    std::vector<InterPoint> allPoints;
};

/** @brief 算法分析结果定义,字段递增模式 */
class AlgorithmResult
{
public:
	CRect rect;
    float Confidence;
    uint64_t trackerId;
    std::string vehicleNum;
    FEATUREM feature;
};

/** @brief 平台指令执行结果定义 */
class InstructionResult
{
public:
    DOT dot;
    METADATATYPE Type;
    //执行状态
    DATASTATE Status;
    //当前指令ID
    std::string CurrentId;
    //算法和视频id
    std::string CameraId;
};

/** @brief 告警数据样式定义 */
class AlarmResult
{
public:
    METADATATYPE Type;
    std::string AlarmId;
    //告警摄像头ID
    std::string CameraId;
    //当前指令id(任务ID)
    std::string CurrentId;
    //告警摄像头名称
    std::string CameraName;
    //告警图片路径
    std::string ImagePath;
    //告警视频路径
    std::string VideoPath;
    //告警时间
    uint64_t TimeStamp;
    //当前算法类型值设定
    uint32_t AlgorithmType;
    //算法分析结果数据集
    std::vector<AlgorithmResult> Aresult;
};

/** @brief 算法参数定义 */
class AlgorithmData
{
public:
    //指定算法处理模式
    DOT dot;
    DATASTATE Status;
    //算法参数可用状态
    bool AvailableStatus;
    //摄像头使用状态
    bool ActiveStatus;
    //当前指令id
    std::string CurrentId;
    //算法对应视频id
    std::string AlgorithmId;
    //告警间隔时间
    uint64_t AlarmInterval;
    //多通道并行数
    uint32_t BatchSize;
    //极大值和置信度阈值
    float Nms;
    //过滤阈值
    float Threshold;
    //算法输入图分辨率,add接口参数
    uint32_t Resolution;
    //视频区域框
    InterArea Region;
    //人员脱岗时间限定
    uint32_t LeaveTimeLimit;
    //使用信任名单(白名单=1,黑名单=2,未使用=3)
    uint32_t TrustList;
    //人群密度告警人数上限值,负值表示下限值
    int32_t Personnels;
};

//继承可以让数据分析类使用成员函数模板
//class CameraData : public AlgorithmData
class CameraData
{
public:
    //指定摄像头处理模式
    DOT dot;
    DATASTATE Status;
    //摄像头可用状态,参数是否满足
    bool AvailableStatus;
    //摄像头使用状态
    bool ActiveStatus;
    //是否推流
    bool PushFlow;
    //摄像头地址是否级联
    bool Cascade;
    //当前指令id
    std::string CurrentId;
    //摄像头名称
    std::string CameraName;
    //摄像头地址
    std::string CameraUrl;
    //摄像头ID,推拉流地址末尾字段
    std::string CameraId;
    //推流地址
    std::string DestUrl;
    //摄像头分辨率
    uint32_t Width;
    uint32_t Height;
    //摄像头解析结束时间
    uint64_t EndTimeStamp;
    //跳帧频率
    uint32_t SkipFrameFrequency;
    //告警视频保存时长,单位秒
    uint32_t SaveVideoDuration;
};

/** @brief 平台指令数据存储定义 */
class InstructionData
{
public:
    DOT dot;
    std::string CurrentId;
    uint64_t TimeStamp;
    AlarmResult AlarmData;
    std::map<std::string, CameraData> CameraDataM;
    std::map<std::string, AlgorithmData> AlgorithmDataM;
};

/** @brief 程序启动基础数据存储定义 */
class BaseData
{
public:
    //每一条指令id
    std::string CurrentId;
    //指令对应大算法的唯一标识
    std::string AlgorithmId;
    //算法类型
    uint32_t AlgorithmType;
    //容器ID
    std::string ContainerID;
    //存储服务
    std::string StorageServer;
    //http服务
    std::string HttpHost;
    //http对端服务地址
    std::string HttpPeerHost;
    //消息代理服务器
    std::string MsgBrokerHost;
    //是否启用http服务,默认不开启
    bool EnableHttpServer = false;
    //内部或外部名单库选择开关,默认使用私有库
    bool UsePrivateLib = true;
    ///kafka服务
    std::string kafkaHost;
    std::string KafkaStatusPubTopic;
    std::string KafkaAlarmPubTopic;
    std::string KafkaAnalysisPubTopic;
    std::vector<std::string> kafkaSubTopicName;
};

typedef struct
{
    uint32_t trustFlag = 3;
    uint32_t leaveTimeLimit = 300;
    int32_t alarmPersonnels = 1000;
    uint64_t alarmInterval = 60;
    /** @brief 动态更新字段 */
    uint64_t alarmDuration = 0;
    float threshold = 0.6;
	bool pushFlow = false;
    std::string taskId;   //任务id
    std::string decodeId; //解码id
    std::string encodeId; //编码id
    InterArea region;
} IndexDataAI;

/** @brief 模块通信和gpu数据定义 */
class IndexData
{
public:
    //gpu资源分配位置待定
    IndexData(bool enable = false)
    {
        index = -1;
        used = std::make_shared<std::atomic_bool>(false);
        if (!enable) return;
        DecodeGpu = std::make_shared<MatType>(1, 1, CV_8UC3);
        EncodeGpu = std::make_shared<MatType>(1, 1, CV_8UC4);
    }
public:
    //zeromq可用索引号
    int index;
    //索引是否使用
    std::shared_ptr<std::atomic_bool> used;
    //索引对应bind_name
    std::string BindName;
    //解码gpu资源
    std::shared_ptr<MatType> DecodeGpu;
    //编码gpu资源
    std::shared_ptr<MatType> EncodeGpu;
};

class Global_Detect
{
public:
    //x,y,w,h
    float bbox[4];
    float conf;//可信度
    float class_id; //告警类算法类别
    FEATUREM feature;
    uint32_t count;
    uint32_t isAlarm;
    uint64_t trackerId;
    std::string info;
    cv::Mat image;
};

class TrustDataTable
{
public:
    std::string id;
    std::string name;
    std::string department;
    std::string vehicle;
    //修改成 Eigen::Matrix 结构
    FEATUREM feature;
    uint32_t trust;
};

//指令操作类型
enum IDOT
{
    I_ADDFACE              = 0x80, //人脸信息录入
    I_ADDVEHICLE           = 0x81, //车辆信息录入
    I_FACEDETECT           = 0X82, //人脸图检测
    I_VEHICLEDETECT        = 0X83, //车辆检测
    I_FACERESULT_HTTP      = 0X84, //人脸图结果http传入
    I_VEHICLERESULT_HTTP   = 0X85, //车辆结果
    I_FACERESULT_VIDEO     = 0X94, //人脸图结果video传入
    I_VEHICLERESULT_VIDEO  = 0X95, //车辆结果
    I_CROWDDENSITY_VIDEO   = 0X96, //人群密度结果
    I_CROWDFLOW_VIDEO      = 0X97, //人流量统计结果
};

class AdditionalData
{
public:
    //id
    std::string Id;
    //姓名
    std::string Name;
    //部门
    std::string Department;
    //附加告警逻辑标记
    bool IsAlarm;
    //人流量进入进出数
    std::string inout;
};

class InternalDataSet
{
public:
    //指令操作类型
    IDOT idot;
    //摄像头ID
    std::string CameraId;
    //当前指令id(任务ID)
    std::string CurrentId;
    //摄像头名称
    std::string CameraName;
    //当前算法类型值设定
    uint32_t AlgorithmType;
    //告警间隔
    uint32_t AlarmInterval;
    //附加显示信息
    std::vector<AdditionalData> AdditionInfoV;
    //黑白名单(白名单=1,黑名单=2)
    uint32_t TrustFlag;
    //数据生成时间戳
    uint64_t TimeStamp;
    //人群密度人数
    int32_t PersonnelNum;
    //检测框坐标集合
    std::vector<AlgorithmResult> DetectBoxV;
    //图片数据
    cv::Mat mat;
};

using ZeromqIndexDataMap = std::map<std::string, IndexData>;
using AlgorithmDataMap = std::map<std::string, AlgorithmData>;
using CameraDataMap = std::map<std::string, CameraData>;
using AIIndexDataMap = std::map<int, IndexDataAI>;
using TrustDataTableMap = std::map<std::string, TrustDataTable>;

#endif  /* INTERNALDATASET_H */