/*
 * FaceDetectAndFeature.cpp
 *
 *  Created on: May 20, 2019
 *      Author: root
 */

#include "FaceDetectAndFeature.h"
#include "opencv2/opencv.hpp"

using namespace cv;
#include <malloc.h>
int faceInitNum = 0;
#define FEATURESIZE 512
#define FACENUMBER 50
CFaceDetectAndFeature::CFaceDetectAndFeature()
{
	faceDetect = NULL;
}

CFaceDetectAndFeature::~CFaceDetectAndFeature()
{
	delete faceDetect;
	faceDetect = NULL;
}

bool CFaceDetectAndFeature::init(int mode, int gpuID, int ProcessMaxL, int ProcessMinL)
{
	gsFaceParam faceSdkParam;
	faceSdkParam.faceDet_nms_threshold = 0.4;
	faceSdkParam.faceDet_threshold = 0.6;
	faceSdkParam.faceDet_devid = gpuID;
	faceSdkParam.faceDet_batchsize = 1;
	faceSdkParam.faceDet_model = "detect_19201080";
	faceSdkParam.feaExt_devid = gpuID;
	faceSdkParam.feaExt_batchsize = 1;
	faceSdkParam.feaExt_model = "feature_101";
	faceSdkParam.LongSideSize = 1920;
	faceSdkParam.scalRatio = FACTOR_16_9;
	faceSdkParam.type = FACEALL;
	faceSdkParam.inputType = 1;

	try
	{
		faceDetect = new gsFaceSDK("/usr/local/cuda/tensorRT/models/face/", faceSdkParam);
	}
	catch(...)
	{
		return false;
	}


	return true;
}

void CFaceDetectAndFeature::processImage(Mat &frame, int width, int height, int channel, GetFaceType faceType, vector<CFaceInformation> &faceInformations)
{
	freeCount++;
	Mat src;
	faceDetect->BGR2RGB(src, frame);

	cvtColor(frame, src, CV_RGB2BGR);

	vector<FaceRect>faceBoxs;
	FACE_NUMBERS faceNum;
	try
	{
		faceNum = faceDetect->getFacesAllResult(src.data, src.cols, src.rows, 3, faceBoxs, faceType);
	}
	catch(...)
	{
		if(!src.empty())
		{
			src.release();
			src.deallocate();
		}
		return;
	}

	if(!src.empty())
	{
		src.release();
		src.deallocate();
	}
	
	for(unsigned int i = 0; i < faceNum; i++)
	{
		CFaceInformation faceInformation;
		faceInformation.feature = new float[FACEFEATURESIZE];
		faceInformation.left = faceBoxs[i].facebox[0];
		faceInformation.top = faceBoxs[i].facebox[1];
		faceInformation.width = faceBoxs[i].facebox[2] - faceBoxs[i].facebox[0];
		faceInformation.height = faceBoxs[i].facebox[3] - faceBoxs[i].facebox[1];
		faceInformation.facePosScore = faceBoxs[i].score;
		faceInformation.age = faceBoxs[i].age;
		faceInformation.gender = faceBoxs[i].gender;
	
		for (size_t j = 0; j < FACEFEATURESIZE; j++)
		{
			faceInformation.feature[j] = faceBoxs[i].Rfeature[j];
		}
		
//		memcpy(faceInformation.feature, faceBoxs[i].feature, FACEFEATURESIZE * sizeof(float));
		faceInformations.push_back(faceInformation);
	}
}

float CFaceDetectAndFeature::similarity(R_FEATURE &featureA, vector<R_FEATURE> features)
{
	int pos = 0;
	float similary = faceDetect->compares(featureA, features, pos);
	return similary;
}

void CFaceDetectAndFeature::release()
{
	delete faceDetect;
	faceDetect = NULL;
}
