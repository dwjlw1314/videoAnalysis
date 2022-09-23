/*
Copyright (C) 2020 - 2021, Beijing Chen An Co.Ltd.All rights reserved.
File Name: VideoAlgorithm.hpp
Description: 视频编解码、推流、算法AI资源管理
Change History:
Date         Version      Changes
2022/02/25   1.0.0.0      Create
*/
#ifndef _VIDEOALGORITHM_H_
#define _VIDEOALGORITHM_H_

#ifdef ENABLERZ

#include <cuda_runtime.h>
#include "VideoSource.h"
#include "VideoEncoder.h"
#include "InternalDataSet.h"
#include "ZmqManager.hpp"

#include <vector>

using namespace CAAIImagePro;

class VideoAlgorithm final
{
public:
    VideoAlgorithm(void) {};

    /** @brief 初始化资源，接口未实现 */
    void InitResource(void) = delete;

    void ReadFrameFFmpeg(CameraData cd, std::shared_ptr<ZmqManager> zmq_mgmt,
        std::map<std::string, IndexData>& zermq_index_data);

    void WriteFrameFFmpeg(CameraData cd, std::shared_ptr<ZmqManager> zmq_mgmt,
        std::map<std::string, IndexData>& zermq_index_data);

private:

};

#endif

#endif /* _VIDEOALGORITHM_H_ */