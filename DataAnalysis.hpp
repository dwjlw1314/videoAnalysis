/*
Copyright (C) 2020 - 2021, Beijing Chen An Co.Ltd.All rights reserved.
File Name: DataAnalysis.hpp
Description: 数据序列化和反序列化处理
Change History:
Date         Version      Changes
2022/02/11   1.0.0.0      Create
*/
#ifndef _DATAANALYSIS_H_
#define _DATAANALYSIS_H_

#include "GsafetyLogLibrary.hpp"
#include "ExternalDataSet.pb.h"
#include "InternalDataSet.h"
#include "HttpPlatFormData.h"
#include <json/json.h>
#include <iostream>
#include <string>
#include <ctime>

using namespace std;
using namespace VideoEngine2;

class DataAnalysis
{
public:
    DataAnalysis();
    ~DataAnalysis();

    /** 
     * @brief 添加主机名和算法md5值
     * @param cid 主机名称或者容器id
     * @param md5 算法名称的md5串
     * @param atype 算法类型
     */
    inline void SetBaseInfo(const BaseData& base_data) { m_BaseData = base_data;}

        /** 
     * @brief 内部数据序列化
     * @param metadata 结构化数据实体泛型指针
     * @param msg 序列化后数据实体,参数方式为inout
     * @param type 结构化数据类型(1.编解码，2.算法，3.性能分析)
     * @return 序列化成功返回true
     */
    bool InDataSerialize(void* metadata, std::string& msg, int type);

    /** 
     * @brief 平台数据序列化
     * @param msg 序列化后数据实体,参数方式为inout
     * @param inst_data 已处理指令参数数据
     * @param status 标记指令状态
     */
    void OutDataSerialize(const InstructionData& inst_data, std::string& msg, DATASTATE status);

    /** 
     * @brief 内部数据反序列化
     * @param msg 序列化前数据实体
     * @param inst_data 已处理指令参数数据,参数方式为inout
     * @return 解析成功返回true
     */

    bool InDataDeserialize(const std::string& msg, InstructionData& inst_data);

    /**
     * @brief 内部数据反序列化
     * 
     * @param msg 需要反序列化的数据
     * @param cameraData 摄像头反序列化的数据
     * @param alarmData 告警反序列化之后的数据
     * @param type 数据类型 METADATATYPE
     * @return true 反序列化成功
     * @return false 反序列化失败
     */
    bool InDataDeserialize(const string &msg, CameraData& cameraData, AlarmResult& alarmData, int& type);

    /**
     * @brief 内部数据序列化
     * 
     * @param msg 序列化后数据实体,参数方式为inout
     * @param cameraData 序列化基的数据
     * @return true 序列化成功
     * @return false 序列化失败
     */
    bool InDataSerialize(string &msg, const CameraData &cameraData, const AlarmResult& alarmData, int type);

    /** 
     * @brief 平台数据反序列化
     * @param msg 序列化数据实体
     * @param cameraDataM 摄像头参数数据，参数方式为inout
     * @param algorithmDataM 算法参数数据，参数方式为inout
     * @return 解析成功返回true，解析失败或者非该算法指令返回false
     */
    bool OutDataDeserialize(const std::string& msg, InstructionData& instruction_data);
    
    /** 
     * @brief 平台数据反序列化
     * @param msg 序列化数据实体
     * @param flag 标记指令状态
     */
    void OutRunStateSerialize(std::string& msg, volatile bool flag);

    /** 
     * @brief 数据库基础数据表反序列化
     * @param msg 序列化数据实体
     * @param bd 主程序启动数据，参数方式为inout
     * @return 解析成功返回true，解析失败返回false 
     */
    bool OutBaseDataDeserialize(const std::string& msg, BaseData& bd);

    /** 
     * @brief 数据库容器表数据反序列化
     * @param msg 序列化数据实体
     * @param cd 摄像头数据，参数方式为inout
     * @param ad 算法数据，参数方式为inout
     * @return 解析成功返回true，解析失败返回false 
     */
    bool OutContainerDataDeserialize(const std::string& msg, CameraData& cd, AlgorithmData& ad);

    /** 
     * @brief 数据库容器表数据序列化
     * @param msg 需要序列化数据实体，参数方式为inout
     * @param cd 摄像头数据
     * @param ad 算法数据
     */
    void OutContainerDataSerialize(std::string& msg, const CameraData& cd, const AlgorithmData& ad);

    /** 
     * @brief 数据库表gsafety_container数据序列化
     * @param msg 序列化数据实体
     * @param ia 摄像头区域数据，参数方式为inout
     */
    void AlgorithmRegionDeserialize(const std::string& msg, InterArea& ia);

    /** 
     * @brief 数据库表AlgorithmRegion字段序列化
     * @param msg 序列化数据实体，参数方式为inout
     * @param ia 摄像头区域数据
     */
    void AlgorithmRegionSerialize(std::string& msg, const InterArea& ia);

    /** 
     * @brief 数据库表gsafety_face/vehicle反序列化
     * @param msg 序列化数据实体
     * @param tdt 分析类黑白名单数据集合,参数方式为inout
     * @return 解析成功返回true，解析失败返回false 
     */
    bool OutTrustDataDeserialize(const std::string& msg, TrustDataTableMap& tdt);

    /** 
     * @brief 数据库表gsafety_face/vehicle反序列化
     * @param msg 序列化数据实体，参数方式为inout
     * @param tdt 分析类黑白名单数据集合单个节点
     * @param idot 算法内部操作类型
     */
    void OutTrustDataSerialize(std::string& msg, const TrustDataTable& tdt, IDOT idot);

    /** 
     * @brief VideoEngine2::InternalDataSet结构数据序列化
     * @param msg 输出序列化数据实体，参数方式为inout
     * @param ids 分析类算法结果集
     */
    void InternalDataSetSerialize(std::string& msg, const InternalDataSet& ids);

    /** 
     * @brief VideoEngine2::InternalDataSet结构数据反序列化
     * @param msg 输入序列化数据实体
     * @param ids 分析类算法结果集，参数方式为inout
     * @return 解析成功返回true，解析失败返回false 
     */
    bool InternalDataSetDeserialize(const std::string& msg, InternalDataSet& ids);

    /**
     * @brief 平台数据序列化为InternalDataSet
     * 
     * @param msg 输出序列化数据实体，参数方式为inout
     * @param internalDataSet 分析类算法结果集
     */
    void platformDataSetSerialize(string &msg, const HttpPlatFormData &httpPlatFormData);

    /**
     * @brief InternalDataSet结构数据反序列化为平台数据
     * 
     * @param msg 输入序列化数据实体
     * @param internalDataSet 分析类算法结果集，参数方式为inout
     * @return 解析成功返回true，解析失败返回false 
     */
    bool PlatformDataSetDeserialize(const string &msg, HttpPlatFormData &httpPlatFormData);

    /** 
     * @brief 解析特征字符串
     * @param featureString 特征字符串
     * @param feature Matrix矩阵特征，参数方式为inout
     */
    void ParseFeature(const string &featureString, FEATUREM& feature);

    /** 
     * @brief 视频摄像头算法区域框结构转换
     * @param rect 坐标结构体
     * @return 返回字节序列
     */
    std::string FromRegionToString(const CRect& rect) = delete;

    /** 操作类型枚举转字符串名称 */
    std::string DotConvertName(enum DOT dot);

    /** 算法告警类型枚举转字符串名称 */
    std::string AITypeConvertName(uint32_t ai_type);

protected:

    /** 判断指令是否属于当前主机或容器, 匹配=true */
    bool InstructMatch(const InstructionInfo& instruct);

    /** @brief 字符串分割函数 */
    void Split(const std::string& iLine, const char* iSymbol, CRect& rect);

    /** @brief 字符串分割函数 */
    void Split(const std::string& iLine, const char* iSymbol, std::vector<std::string>& oVecWord);


private:
    BaseData m_BaseData;
    
    GsafetyLog* m_pLogger;
};

#endif /* _DATAANALYSIS_H_ */