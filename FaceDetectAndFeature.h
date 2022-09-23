/*
 * FaceDetectAndFeature.h
 *
 *  Created on: May 20, 2019
 *      Author: root
 */

#ifndef SRC_FACEDETECTANDFEATURE_H_
#define SRC_FACEDETECTANDFEATURE_H_

#include <vector>
#include <string>
#include <iostream>
#include <cstdio>
using namespace std;

#include "gsFaceSDK.h"
#include "public_data.h"
#include "FaceInformation.h"
#include <Eigen/Dense>
using namespace IODATA;

class CFaceDetectAndFeature
{
public:
	CFaceDetectAndFeature();
	~CFaceDetectAndFeature();

public:
	bool init(int mode = 0, int gpuID = 0, int ProcessMaxL = 1920, int ProcessMinL = 1080);
	void processImage(Mat &frame, int width, int height, int channel, GetFaceType faceType, vector<CFaceInformation> &faceInformations);
	float similarity(R_FEATURE &featureA, vector<R_FEATURE> features);
	void release();

private:

	int tasks=0;
	gsFaceSDK *faceDetect;

	int freeCount=0;

};

#endif /* SRC_FACEDETECTANDFEATURE_H_ */
