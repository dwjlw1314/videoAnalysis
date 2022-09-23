#include "MonitorParameters.h"


MonitorParameter::MonitorParameter(/* args */)
{
}

MonitorParameter::~MonitorParameter()
{
}

bool MonitorParameter::_checkDecDelay(int videoFPS, int decodeFPS)
{
    if (decodeFPS < videoFPS / 2)
    {
        return true;
    }

    return false;
};

bool MonitorParameter::_checkAlgDelay(int algfps)
{
    if (algfps < _decode_frame_all / 2)
    {
        return true;
    }
    return false;
};

void MonitorParameter::setTimeInterval(int elseTimeInterval)
{
    this->timeInterval = elseTimeInterval;
};

string MonitorParameter::checkDelay()
{
    // 代表一行参数记录
    string tempcameraId, algotithmId, templine,tempdecodeid, algName;
    int tempvideoFPS, tempdecodeFPS, algFPS;
    uint32_t tempSkipFrameFrequency;
    map<string, DecodeParam>::reverse_iterator deciter;
    map<string, AIParam>::reverse_iterator algiter;
    pair<string, int> tempexception;
    DelayInfo delayInfo;
    string delayMsg;
    isDelay = false;

    data_metrics = "";
    _decode_frame_all = 0;

    //添加decode参数
    for (deciter = GsDecodeParam.rbegin(); deciter != GsDecodeParam.rend(); deciter++)
    {
        templine = "";
        tempcameraId = deciter->first;
        tempvideoFPS = deciter->second.videoFPS;
        tempdecodeFPS = deciter->second.decodeFPS;
        tempSkipFrameFrequency = deciter->second.SkipFrameFrequency;
        tempdecodeid = deciter->second.decodeId;

        _decode_frame_all += tempdecodeFPS;

        templine = "contionerId=\"" + contionerId.substr(0, 12) + "\"";
        templine = templine + ",cameraId=\"" + tempcameraId.substr(0, 12) + "\"";
        templine = templine + ",videoFPS=\"" + to_string(tempvideoFPS) + "\"";
        templine = "decodeFPS{" + templine + "} " + to_string(tempdecodeFPS) + "\n";
        //cout << templine << endl;

        if (this->_checkDecDelay(tempvideoFPS, tempdecodeFPS))
        {
            DecodeParamAll *onedecodeparam;
            onedecodeparam = delayInfo.add_decparamall();
            onedecodeparam->set_cameraid(tempcameraId);
            onedecodeparam->set_decodeid(tempdecodeid);
            onedecodeparam->set_decodefps(tempdecodeFPS);
            onedecodeparam->set_videofps(tempvideoFPS);
            onedecodeparam->set_skipframefrequency(tempSkipFrameFrequency);
            isDelay = true;
        };

        //添加到_data_metrics中
        data_metrics = data_metrics + templine;        
    };

    //添加ai参数
    for (algiter = GsAIParam.rbegin(); algiter != GsAIParam.rend(); algiter++)
    {
        templine = "";
        algotithmId = algiter->first;
        algFPS = algiter->second.algorithmFPS;
        algName = algiter->second.algorithmName;
        
        // algName = algiter->second.algorithmName;

        templine = "contionerId=\"" + contionerId.substr(0, 12) + "\"";
        templine = templine + ",algorithm=\"" + algName + "\"";
        templine = templine + ",algotithmId=\"" + algotithmId.substr(0, 12) + "\"";
        templine = "algFPS{" + templine + "} " + to_string(algFPS) + "\n";
        //cout << templine << endl;

        if (this->_checkAlgDelay(algFPS))
        {
            AIParamAll *oneaiparam;
            oneaiparam = delayInfo.add_aiparamall();
            oneaiparam->set_algorithmid(algotithmId);
            oneaiparam->set_algorithmname(algName);
            oneaiparam->set_algorithmfps(algFPS);
            oneaiparam->set_defaultfps(1);
            isDelay = true;
        };

        data_metrics = data_metrics + templine;
    };

    delayInfo.SerializeToString(&delayMsg);
    GsDecodeParam.clear();
    GsAIParam.clear();    
    return delayMsg;
};

void MonitorParameter::init(string contionerId, int timeInterval, int algorithmNum)
{
    this->timeInterval = timeInterval;
    this->algorithmNum = algorithmNum;
    this->contionerId = contionerId;
    GsMonitorClient.init();
    //cout << "MonitorParam类实例初始化完成!" << endl;
};



void MonitorParameter::insertDecodeParam(pair<string, DecodeParam> &oneDecodeParamPair)
{
    GsDecodeParam.insert(oneDecodeParamPair);
    //cout << "DecodeParamArray 插入一条数据" << endl;
};

void MonitorParameter::insertAlgorithmParam(pair<string, AIParam> &oneAIParamPair)
{
    GsAIParam.insert(oneAIParamPair);
    //cout << "AIParamArray 插入一条数据" << endl;
};


// #todo 推送到gateway

void MonitorParameter::pushPrameter2Gateway()
{
    // string oneline = _data_metrics.find();
    // gsMonitorClient.send(_data_metrics);
    
    //cout << "推送监控参数到gateway" << endl;
};
