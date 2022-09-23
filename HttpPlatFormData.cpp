
#include "HttpPlatFormData.h"
#include "GsafetyLogLibrary.hpp"

#include"json/json.h"
using namespace Json;

#define FEATURE_SIZE 512

AlgorithmResultSet::AlgorithmResultSet()
{
    confidence = 0.0;
    left = 0;
    top = 0;
    height = 0;
    width = 0;
}

AlgorithmResultSet::~AlgorithmResultSet()
{

}

AlgorithmResultSet::AlgorithmResultSet(const AlgorithmResultSet &other)
{
    confidence = other.confidence;
    left = other.left;
    top = other.top;
    width = other.width;
    height = other.height;
    vehiclePlate = other.vehiclePlate;
    memcpy(feature.data(), other.feature.data(), sizeof(float) * FEATURE_SIZE);
}

AlgorithmResultSet& AlgorithmResultSet::operator=(const AlgorithmResultSet &other)
{
    if(this == &other)
    {
        return *this;
    }
    confidence = other.confidence;
    left = other.left;
    top = other.top;
    width = other.width;
    height = other.height;
    vehiclePlate = other.vehiclePlate;
    memcpy(feature.data(), other.feature.data(), sizeof(float) * FEATURE_SIZE);
    return *this;
}

void AlgorithmResultSet::setFeature(FEATUREM feature)
{
    memcpy(this->feature.data(), feature.data(), sizeof(float) * FEATURE_SIZE);
}

FEATUREM AlgorithmResultSet::getFeature() const
{
    return feature;
}

void AlgorithmResultSet::setConfidence(float confidence)
{
    this->confidence = confidence;
}

float AlgorithmResultSet::getConfidence() const
{
    return confidence;
}

void AlgorithmResultSet::setLeft(int left)
{
    this->left = left;
}

int AlgorithmResultSet::getLeft() const
{
    return left;
}

void AlgorithmResultSet::setTop(int top)
{
    this->top = top;
}

int AlgorithmResultSet::getTop() const
{
    return top;
}

void AlgorithmResultSet::setWidth(int width)
{
    this->width = width;
}

int AlgorithmResultSet::getWidth() const
{
    return width;
}

void AlgorithmResultSet::setHeight(int height)
{
    this->height = height;
}

int AlgorithmResultSet::getHeight() const
{
    return height;
}

void AlgorithmResultSet::setVehiclePlate(string &vehiclePlate)
{
    this->vehiclePlate = vehiclePlate;
}

string AlgorithmResultSet::getVehiclePlate() const
{
    return vehiclePlate;
}

FaceAdditional::FaceAdditional()
{

}

FaceAdditional::~FaceAdditional()
{

}

void FaceAdditional::setFaceID(string &faceID)
{
    this->faceID = faceID;
}

string FaceAdditional::getFaceID()
{
    return faceID;
}

void FaceAdditional::setName(string &name)
{
    this->name = name;
}

string FaceAdditional::getName()
{
    return name;
}

void FaceAdditional::setDepartment(string &department)
{
    this->department = department;
}

string FaceAdditional::getDepartment()
{
    return department;
}

HttpPlatFormData::HttpPlatFormData()
{
    width = 0;
    height = 0;
    TrustFlag = 0;
    personNum = 0;
    similarity = 0.0;
}

HttpPlatFormData::~HttpPlatFormData()
{

}

bool HttpPlatFormData::parseMessage(const string &msg)
{
    JSONCPP_STRING errs;
    Json::Value root;
    Json::CharReaderBuilder readerBuilder;
    std::unique_ptr<Json::CharReader> const jsonReader(readerBuilder.newCharReader());
    if (jsonReader->parse(msg.c_str(), msg.c_str()+msg.length(), &root, &errs))
    {
        if(root.isMember("width") && root["width"].isInt())
        {
            width = root["width"].asInt();
        }
        
        if(root.isMember("height") && root["height"].isInt())
        {
            height = root["height"].asInt();
        }
        
        if(root.isMember("TrustFlag") && root["TrustFlag"].isUInt());
        {
            TrustFlag = root["TrustFlag"].asUInt();
        }
        
        if(root.isMember("imageData") && root["imageData"].isString())
        {
            imageData = root["imageData"].asString();
        }
        
        if(root.isMember("AlgorithmResultSet") && root["AlgorithmResultSet"].isArray())
        {
            int count = root["AlgorithmResultSet"].size();
            for(int i = 0; i < count; i++)
            {
                float featureTemp[FEATURE_SIZE];
                int featureSize = root["AlgorithmResultSet"][i]["feature"].size();
                if(featureSize != FEATURE_SIZE)
                {
                    continue;
                }

                for(int j = 0; j < featureSize; j++)
                {
                    featureTemp[j] = root["AlgorithmResultSet"][i]["feature"][j].asFloat();
                }

                AlgorithmResultSet algorithmResultSet;
                FEATUREM feature;
                memcpy(feature.data(), featureTemp, sizeof(float) * FEATURE_SIZE);
                algorithmResultSet.setFeature(feature);
                if(root["AlgorithmResultSet"][i].isMember("confidence"))
                {
                    float confidence = root["AlgorithmResultSet"][i]["confidence"].asFloat();
                    algorithmResultSet.setConfidence(confidence);
                }

                if(root["AlgorithmResultSet"][i].isMember("vehiclePlate") && root["AlgorithmResultSet"][i]["vehiclePlate"].isString())
                {
                    string vehiclePlate = root["AlgorithmResultSet"][i]["vehiclePlate"].asString();
                    algorithmResultSet.setVehiclePlate(vehiclePlate);
                }
                if(root["AlgorithmResultSet"][i].isMember("left") && root["AlgorithmResultSet"][i]["left"].isInt())
                {
                    algorithmResultSet.setLeft(root["AlgorithmResultSet"][i]["left"].asInt());
                }
                if(root["AlgorithmResultSet"][i].isMember("top") && root["AlgorithmResultSet"][i]["top"].isInt())
                {
                    algorithmResultSet.setTop(root["AlgorithmResultSet"][i]["top"].isInt());
                }

                if(root["AlgorithmResultSet"][i].isMember("width") && root["AlgorithmResultSet"][i]["width"].isInt())
                {
                    algorithmResultSet.setWidth(root["AlgorithmResultSet"][i]["width"].asInt());
                }
                if(root["AlgorithmResultSet"][i].isMember("height") && root["AlgorithmResultSet"][i]["height"].isInt())
                {
                    algorithmResultSet.setHeight(root["AlgorithmResultSet"][i]["height"].asInt());
                }

                resultSetVec.push_back(algorithmResultSet);
            }
        }
        if(root.isMember("personNum") && root["personNum"].isInt())
        {
            personNum = root["personNum"].asInt();
        }
        
        if(root.isMember("cameraID") && root["cameraID"].isString())
        {
            cameraID = root["cameraID"].asString();
        }
        if(root.isMember("cameraName") && root["cameraName"].isString())
        {
            cameraName = root["cameraName"].asString();
        }
        
        if(root.isMember("currentID") && root["currentID"].isString())
        {
            currentID = root["currentID"].asString();
        }
        
        if(root.isMember("similarity"))
        {
            similarity = root["similarity"].asFloat();
        }
        if(root.isMember("FaceAdditionalInfo") && root["FaceAdditionalInfo"].isArray())
        {
            Value faceAddtionalInfoValue = root["FaceAdditionalInfo"];
            for (int i = 0; i < faceAddtionalInfoValue.size(); i++)
            {
                FaceAdditional faceAdditionalInfo;
                string faceID = faceAddtionalInfoValue[i]["faceID"].asString();
                string name = faceAddtionalInfoValue[i]["name"].asString();
                string department = faceAddtionalInfoValue[i]["department"].asString();

                faceAdditionalInfo.setFaceID(faceID);
                faceAdditionalInfo.setName(name);
                faceAdditionalInfo.setDepartment(department);

                faceAdditionalInfoVec.push_back(faceAdditionalInfo);
            }    
        }

        return true;
    }
    else
    {
        LOG4CPLUS_ERROR("root", errs);
        return false;
    }
}

string HttpPlatFormData::buildUpMessage() const
{
    Value root;
    StreamWriterBuilder writerBuilder;
//    root["width"] = width;
//    root["height"] = height;
    root["TrustFlag"] = TrustFlag;
//    root["imageData"] = imageData;

    for (size_t i = 0; i < resultSetVec.size(); i++)
    {
        Value node;
        FEATUREM featureTemp = resultSetVec[i].getFeature();
        float feature[FEATURE_SIZE] = {0};
        memcpy(feature, featureTemp.data(), FEATURE_SIZE * sizeof(float));
        for(int j = 0; j < FEATURE_SIZE; j++)
        {
            node["feature"].append(feature[j]);
        }
        node["left"] = resultSetVec[i].getLeft();
        node["top"] = resultSetVec[i].getTop();
        node["width"] = resultSetVec[i].getWidth();
        node["height"] = resultSetVec[i].getHeight();
        node["confidence"] = resultSetVec[i].getConfidence();
        node["vehiclePlate"] = resultSetVec[i].getVehiclePlate();
        root["AlgorithmResultSet"].append(node);
    }

    if(resultSetVec.empty())
    {
        root["AlgorithmResultSet"].resize(0);
    }

    root["personNum"] = personNum;
//    root["cameraID"] = cameraID;
//    root["cameraName"] = cameraName;
//    root["currentID"] = currentID;
    root["similarity"] = similarity;

    for (size_t i = 0; i < faceAdditionalInfoVec.size(); i++)
    {
        FaceAdditional faceAdditional = faceAdditionalInfoVec[i];

        Value node;
        node["faceID"] = faceAdditional.getFaceID();
        node["name"] = faceAdditional.getName();
        node["department"] = faceAdditional.getDepartment();
        root["FaceAdditionalInfo"].append(node);
    }

    if(faceAdditionalInfoVec.empty())
    {
        root["FaceAdditionalInfo"].resize(0);
    }
    
    ostringstream os;
    unique_ptr<StreamWriter> jsonWriter(writerBuilder.newStreamWriter());
    jsonWriter->write(root, &os);

    return os.str();
}

void HttpPlatFormData::setImageWidth(int width)
{
    this->width = width;
}

int HttpPlatFormData::getImageWidth()
{
    return width;
}

void HttpPlatFormData::setIamgeHeight(int height)
{
    this->height = height;
}

int HttpPlatFormData::getImageHeight()
{
    return height;
}

void HttpPlatFormData::setTrustFlag(int flag)
{
    this->TrustFlag = flag;
}

int HttpPlatFormData::getTrustFlag()
{
    return TrustFlag;
}

void HttpPlatFormData::setImageData(string &imageData)
{
    this->imageData = imageData;
}

string HttpPlatFormData::getImageData()
{
    return imageData;
}

void HttpPlatFormData::setResultSet(vector<AlgorithmResultSet> &resultSetVec)
{
    this->resultSetVec.clear();
    this->resultSetVec = resultSetVec;
}

vector<AlgorithmResultSet> HttpPlatFormData::getResultSet()
{
    return resultSetVec;
}

void HttpPlatFormData::setCameraID(string &cameraID)
{
    this->cameraID = cameraID;
}
    
string HttpPlatFormData::getCameraID()
{
    return cameraID;
}

void HttpPlatFormData::setCameraName(string &cameraName)
{
    this->cameraName = cameraName;
}

string HttpPlatFormData::getCameraName()
{
    return cameraName;
}

void HttpPlatFormData::setFaceAdditionalInfo(vector<FaceAdditional> &faceAdditionalInfoVec)
{
    this->faceAdditionalInfoVec = faceAdditionalInfoVec;
}

vector<FaceAdditional> HttpPlatFormData::getFaceAdditionalInfo()
{
    return faceAdditionalInfoVec;
}

void HttpPlatFormData::setPersonNum(int personNum)
{
    this->personNum = personNum;
}

int HttpPlatFormData::getPersonNum()
{
    return personNum;
}

void HttpPlatFormData::setCurrentID(string &currentID)
{
    this->currentID = currentID;
}

string HttpPlatFormData::getCurrentID()
{
    return currentID;
}

void HttpPlatFormData::setSimilarity(float similarity)
{
    this->similarity = similarity;
}

float HttpPlatFormData::getSimilarity()
{
    return similarity;
}