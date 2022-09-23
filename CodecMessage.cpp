

#include "CodecMessage.h"

CodecMessage::CodecMessage()
{
}

CodecMessage::~CodecMessage()
{
}

bool CodecMessage::sendMessage(shared_ptr<ZmqManager>& zmqObject, int index, DOT dot, DATASTATE dataState, string &cameraID, string &currentID, bool sendPair)
{
    Value root;
    StreamWriterBuilder writerBuilder;
    root["dot"] = dot;
    root["Type"]= CODEC;
    root["Status"]= dataState;
    root["CurrentId"]= currentID;
    root["CameraId"]= cameraID;
    //root["CameraName"]= "123";
    ostringstream os;
    unique_ptr<StreamWriter> jsonWriter(writerBuilder.newStreamWriter());
    jsonWriter->write(root, &os);

    return zmqObject->SendMessage(index, os.str(), os.str().size(), sendPair);
}