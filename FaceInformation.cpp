/*
 * FaceInformation.cpp
 *
 *  Created on: May 20, 2019
 *      Author: root
 */

#include "FaceInformation.h"
#include <cstring>



CFaceInformation::CFaceInformation()
{
	// TODO Auto-generated constructor stub
	left = -1;
	top = -1;
	width = -1;
	height = -1;
	feature = NULL;
	facePosScore = 0.0;
	gender = -1;
	age = 0.0;
}

CFaceInformation::~CFaceInformation()
{
	// TODO Auto-generated destructor stub
	delete []feature;
	feature = NULL;
}

CFaceInformation::CFaceInformation(const CFaceInformation &other)
{
	feature = new float[FACEFEATURESIZE];
	memset(feature, 0, FACEFEATURESIZE);
	memcpy(feature, other.feature, FACEFEATURESIZE * sizeof(float));
	left = other.left;
	top = other.top;
	width = other.width;
	height = other.height;
	facePosScore = other.facePosScore;
	gender = other.gender;
	age = other.age;
}

CFaceInformation& CFaceInformation::operator=(const CFaceInformation &other)
{
	feature = new float[FACEFEATURESIZE];
	memset(feature, 0, FACEFEATURESIZE);
	memcpy(feature, other.feature, FACEFEATURESIZE * sizeof(float));
	left = other.left;
	top = other.top;
	width = other.width;
	height = other.height;
	facePosScore = other.facePosScore;
	gender = other.gender;
	age = other.age;
	return *this;
}

