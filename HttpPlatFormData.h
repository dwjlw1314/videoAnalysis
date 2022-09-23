
#ifndef _HTTP_PLATFORM_DATA_H_
#define _HTTP_PLATFORM_DATA_H_

#include "InternalDataSet.h"

#include <string>
using namespace std;

#include <Eigen/Dense>
#include "public_data.h"
using namespace IODATA;

#include "json/json.h"
using namespace Json;

class AlgorithmResultSet
{
public:
    AlgorithmResultSet();
    AlgorithmResultSet(const AlgorithmResultSet &other);
    AlgorithmResultSet& operator=(const AlgorithmResultSet &other);
    ~AlgorithmResultSet();

public:
    FEATUREM getFeature() const;
    void setFeature(FEATUREM feature);

    void setConfidence(float confidence);
    float getConfidence() const;

    void setLeft(int left);
    int getLeft() const;

    void setTop(int top);
    int getTop() const;

    void setWidth(int width);
    int getWidth() const;

    void setHeight(int height);
    int getHeight() const;

    void setVehiclePlate(string &vehiclePlate);
    string getVehiclePlate() const;

private:
    int left;
    int top;
    int width;
    int height;
    float confidence;
    FEATUREM feature;
    string vehiclePlate;
};

class FaceAdditional
{
public:
    FaceAdditional();
    ~FaceAdditional();

public:
    void setFaceID(string &faceID);
    string getFaceID();

    void setName(string &name);
    string getName();

    void setDepartment(string &department);
    string getDepartment();

private:
    string faceID;
    string name;
    string department;
};



class HttpPlatFormData
{
public:
    HttpPlatFormData();
    ~HttpPlatFormData();

public:
    bool parseMessage(const string &msg);
    string buildUpMessage() const;

    void setImageWidth(int width);
    int getImageWidth();

    void setIamgeHeight(int height);
    int getImageHeight();

    void setTrustFlag(int flag);
    int getTrustFlag();

    void setImageData(string &imageData);
    string getImageData();

    void setResultSet(vector<AlgorithmResultSet> &resultSetVec);
    vector<AlgorithmResultSet> getResultSet();

    void setCameraID(string &cameraID);
    string getCameraID();

    void setCameraName(string &cameraName);
    string getCameraName();

    void setFaceAdditionalInfo(vector<FaceAdditional> &faceAdditionalInfoVec);
    vector<FaceAdditional> getFaceAdditionalInfo();

    void setPersonNum(int personNum);
    int getPersonNum();

    void setCurrentID(string &currentID);
    string getCurrentID();

    void setSimilarity(float similarity);
    float getSimilarity();

private:
    int width;
    int height;
    int personNum;
    uint32_t TrustFlag;
    float similarity;
    string cameraID;//摄像头ID
    string cameraName;//摄像头名称
    string currentID;//任务ID
    string imageData;
    vector<FaceAdditional> faceAdditionalInfoVec;
    vector<AlgorithmResultSet> resultSetVec;
};


#endif