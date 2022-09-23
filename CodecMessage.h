
#ifndef _CODEC_MESSAGE_H_
#define _CODEC_MESSAGE_H_

#include "InternalDataSet.h"
#include "ZmqManager.hpp"

#include <memory>
using namespace std;

#include <json/json.h>
using namespace Json;

class CodecMessage
{
public:
    CodecMessage();
    ~CodecMessage();

public:
    /**
     * @brief 
     * 
     * @param zmqObject zmq
     * @param index 发送的Index
     * @param dot 操作模式
     * @param dataState 数据状态
     * @param cameraID 摄像头ID
     * @param currentID 当前指令ID
     * @param sendPair true 是send ,false 是recv
     * @return 发送成功返回 true
     */
    bool sendMessage(shared_ptr<ZmqManager>& zmqObject, int index, DOT dot, DATASTATE dataState, string &cameraID, string &currentID, bool sendPair = true);

private:
};

#endif