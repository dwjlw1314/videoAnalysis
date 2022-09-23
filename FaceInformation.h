/*
 * FaceInformation.h
 *
 *  Created on: May 20, 2019
 *      Author: root
 */

#ifndef SRC_FACEINFORMATION_H_
#define SRC_FACEINFORMATION_H_

#include "public_data.h"
using namespace IODATA;

#include <string>
using namespace std;
#define FACEFEATURESIZE 512
class CFaceInformation
{
public:
	CFaceInformation();
	~CFaceInformation();
	CFaceInformation(const CFaceInformation &other);
	CFaceInformation& operator=(const CFaceInformation &other);

public:
	// face position
	int left;
	int top;
	int width;
	int height;
	int gender;
	float age;

	float *feature;
	float facePosScore;
};

#endif /* SRC_FACEINFORMATION_H_ */
