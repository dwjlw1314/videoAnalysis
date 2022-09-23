
#ifndef _GsFastDfsManager_H
#define _GsFastDfsManager_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "fdfs_client.h"
#include "fdfs_global.h"
#include "fastcommon/logger.h"
#include "fastcommon/base64.h"
#include "fastcommon/sockopt.h"
#include "fastcommon/logger.h"
#include "fdfs_http_shared.h"

#ifdef __cplusplus
}
#endif

#include <iostream>
#include "Singleton.h"

class GsFastDfsManager : public Singleton<GsFastDfsManager>
{
public:

    /**
     * @brief 初始化资源
     * @param host 存储服务器主机或集群地址
     */
    void initSource(const std::string& group, const std::string& host);

    /**
     * @brief 上传资源
     * @param file_content 资源地址
     * @param file_size 资源大小
     */
    bool uploadFile(char* file_content, int64_t file_size, std::string& path);

    /**
     * @brief 上传视频资源，视频按照mp4结尾
     * 
     * @param file_content  视频文件路径
     * 
     */

    bool uploadVideo(char * file_content,std::string & path);
    

private:

    int writeToFileCallback(void *arg, const int64_t file_size, const char *data, const int current_size);

    int uploadFileCallback(void *arg, const int64_t file_size, int sock);

private:

    std::string m_StorageGroup;
    std::string m_StorageHost;
    std::string m_StorageConf;
    
};

#endif