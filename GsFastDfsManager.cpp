
#include "GsFastDfsManager.h"

void GsFastDfsManager::initSource(const std::string& group, const std::string& host)
{
	m_StorageGroup = group;
	std::string oper_host = host;
	std::string chost = strtok(const_cast<char*>(oper_host.c_str()), ":");
	std::string sport = strtok(NULL, ":");
	std::string vport = strtok(NULL, ":");

	m_StorageHost = "http://" + chost + ":" + vport + "/";
	m_StorageConf = "tracker_server=" + chost + ":" + sport;
}

int GsFastDfsManager::writeToFileCallback(void *arg, const int64_t file_size, 
        const char *data, const int current_size)
{
	if (arg == NULL)
	{
		return EINVAL;
	}

	if (fwrite(data, current_size, 1, (FILE *)arg) != 1)
	{
		return errno != 0 ? errno : EIO;
	}

	return 0;
}

int GsFastDfsManager::uploadFileCallback(void *arg, const int64_t file_size, int sock)
{
	int64_t total_send_bytes;
	char *filename;

	if (arg == NULL)
	{
		return EINVAL;
	}

	filename = (char *)arg;
	return tcpsendfile(sock, filename, file_size, \
		g_fdfs_network_timeout, &total_send_bytes);
}

bool GsFastDfsManager::uploadFile(char* file_content, int64_t file_size, std::string& path)
{
	ConnectionInfo *pTrackerServer;
	ConnectionInfo *pStorageServer;
	int result;
	ConnectionInfo storageServer;
	char group_name[FDFS_GROUP_NAME_MAX_LEN + 1] = {0};
	char remote_filename[256] = {0};
	char appender_filename[256] = {0};
	FDFSMetaData meta_list[32] = {0};
	int meta_count;
	char token[32 + 1] = {0};
	char file_id[128] = {0};
	char file_url[256] = {0};
	char szDatetime[20] = {0};
	char szPortPart[16] = {0};
	int url_len;
	time_t ts;
	// int64_t file_size = 0;
	int store_path_index;
	FDFSFileInfo file_info;
	int upload_type;
	const char *file_ext_name = "jpeg";
	struct stat stat_buf;

	log_init();
	g_log_context.log_level = LOG_DEBUG;
	TrackerServerGroup tracker_group = {0, 0, -1, NULL};
	if ((fdfs_client_init_from_buffer_ex((&tracker_group), m_StorageConf.c_str())) != 0)
	{
		return result;
	}
	
	//pTrackerServer = tracker_get_connection();
	pTrackerServer = tracker_get_connection_ex((&tracker_group));
	if (pTrackerServer == NULL)
	{
		fdfs_client_destroy_ex(&tracker_group);
		return errno != 0 ? errno : ECONNREFUSED;
	}

    
	upload_type = FDFS_UPLOAD_BY_BUFF;

	

	*group_name = '\0';
	store_path_index = 0;
	if ((result=tracker_query_storage_store(pTrackerServer, \
			&storageServer, group_name, &store_path_index)) != 0)
	{

		fdfs_client_destroy_ex(&tracker_group);

	}


	if ((pStorageServer=tracker_make_connection(&storageServer, \
		&result)) == NULL)
	{
		fdfs_client_destroy_ex(&tracker_group);
		return result;
	}

	meta_count = 0;

	

		
	result = storage_upload_appender_by_filebuff( \
			pTrackerServer, pStorageServer, \
			store_path_index, file_content, \
			file_size, file_ext_name, \
			meta_list, meta_count, \
			group_name, remote_filename);
	

	fdfs_get_file_info_ex(group_name, remote_filename, false, &file_info);
	//fdfs_get_file_info(group_name, remote_filename, &file_info);

	strcpy(appender_filename, remote_filename);

	fdfs_get_file_info_ex(group_name, appender_filename, false, &file_info);
	//fdfs_get_file_info(group_name, appender_filename, &file_info);

	tracker_close_connection_ex(pStorageServer, true);
	tracker_close_connection_ex(pTrackerServer, true);

	fdfs_client_destroy_ex(&tracker_group);

	//后期前缀使用数据库StorageServer字段数据
    path = m_StorageHost + m_StorageGroup + "/";
    path += std::string(appender_filename);
	//path += appender_filename;

	return result;
}


bool GsFastDfsManager::uploadVideo(char* file_name,std::string& path)
{
	char *conf_filename;
	ConnectionInfo *pTrackerServer;
	ConnectionInfo *pStorageServer;
	int result;
	ConnectionInfo storageServer;
	char group_name[FDFS_GROUP_NAME_MAX_LEN + 1] = {'\0'};
	FDFSMetaData meta_list[32];
	int meta_count;
	int i;
	FDFSMetaData *pMetaList;
	char token[32 + 1] = {0};
	char file_id[128] = {0};
	char master_file_id[128] = {0};
	char file_url[256] = {0};
	char szDatetime[20] = {0};
	char szPortPart[16] = {0};
	int url_len = 0;
	time_t ts;
    char *file_buff = nullptr;
	int64_t file_size = 0;
	
	char *meta_buff = nullptr;
	int store_path_index = 0;
	FDFSFileInfo file_info;

	log_init();
	g_log_context.log_level = LOG_DEBUG;

	const char *operation = "upload";

	TrackerServerGroup tracker_group = {0, 0, -1, NULL};
	if ((fdfs_client_init_from_buffer_ex((&tracker_group), m_StorageConf.c_str())) != 0)
	{
		return result;
	}
	// if ((result=fdfs_client_init_from_buffer(m_StorageConf.c_str())) != 0)
	// {
	// 	return result;
	// }

	pTrackerServer = tracker_get_connection_ex((&tracker_group));
	if (pTrackerServer == NULL)
	{
		fdfs_client_destroy_ex(&tracker_group);
		return errno != 0 ? errno : ECONNREFUSED;
	}

	// local_filename = NULL;
	if (strcmp(operation, "upload") == 0)
	{
		int upload_type;
		char *prefix_name = nullptr;
		const char *file_ext_name = nullptr;
		char slave_file_id[256] = {0};
		int slave_file_id_len;
		upload_type = FDFS_UPLOAD_BY_BUFF;


		{
		ConnectionInfo storageServers[FDFS_MAX_SERVERS_EACH_GROUP];
		ConnectionInfo *pServer = nullptr;
		ConnectionInfo *pServerEnd = nullptr;
		int storage_count;

		//strcpy(group_name, "group1");
		if ((result=tracker_query_storage_store_list_with_group( \
			pTrackerServer, group_name, storageServers, \
			FDFS_MAX_SERVERS_EACH_GROUP, &storage_count, \
			&store_path_index)) == 0)
		{
			printf("tracker_query_storage_store_list_with_group: \n");
			pServerEnd = storageServers + storage_count;
			for (pServer=storageServers; pServer<pServerEnd; pServer++)
			{
				printf("\tserver %d. group_name=%s, " \
				       "ip_addr=%s, port=%d\n", \
					(int)(pServer - storageServers) + 1, \
					group_name, pServer->ip_addr, \
					pServer->port);
			}
			printf("\n");
		}
		}

		if ((result=tracker_query_storage_store(pTrackerServer, \
		                &storageServer, group_name, &store_path_index)) != 0)
		{
			fdfs_client_destroy_ex(&tracker_group);
			printf("tracker_query_storage fail, " \
				"error no: %d, error info: %s\n", \
				result, STRERROR(result));
			return result;
		}

		printf("group_name=%s, ip_addr=%s, port=%d\n", \
			group_name, storageServer.ip_addr, \
			storageServer.port);

		if ((pStorageServer=tracker_make_connection(&storageServer, \
			&result)) == NULL)
		{
			fdfs_client_destroy_ex(&tracker_group);
			return result;
		}

		memset(&meta_list, 0, sizeof(meta_list));
		meta_count = 0;
		strcpy(meta_list[meta_count].name, "ext_name");
		strcpy(meta_list[meta_count].value, "jpg");
		meta_count++;
		strcpy(meta_list[meta_count].name, "width");
		strcpy(meta_list[meta_count].value, "160");
		meta_count++;
		strcpy(meta_list[meta_count].name, "height");
		strcpy(meta_list[meta_count].value, "80");
		meta_count++;
		strcpy(meta_list[meta_count].name, "file_size");
		strcpy(meta_list[meta_count].value, "115120");
		meta_count++;

		file_ext_name = fdfs_get_file_ext_name(file_name);

		if (upload_type == FDFS_UPLOAD_BY_BUFF)
		{
			printf("storage_upload_by_filebuff\n");
			if((result = getFileSize(file_name, &file_size)) != 0)
			{
				tracker_close_connection_ex(pStorageServer, true);
				fdfs_client_destroy_ex(&tracker_group);
				return result;
			}

			char *file_content = (char *)malloc(file_size);
			if ((result=getFileContentEx(file_name, \
					file_content, 0, &file_size)) == 0)
			{
				result = storage_upload_by_filebuff1(pTrackerServer, \
					pStorageServer, store_path_index, \
					file_content, file_size, file_ext_name, \
					meta_list, meta_count, \
					group_name, file_id);
				printf("file_id is %s\n",file_id);

				path = m_StorageHost;
    			path += std::string(file_id);
			}

			free(file_content);
		

		if (result != 0)
		{
			printf("upload file fail, " \
				"error no: %d, error info: %s\n", \
				result, STRERROR(result));
			tracker_close_connection_ex(pStorageServer, true);
			fdfs_client_destroy_ex(&tracker_group);
			return result;
		}

		if (g_tracker_server_http_port == 80)
		{
			*szPortPart = '\0';
		}
		else
		{
			sprintf(szPortPart, ":%d", g_tracker_server_http_port);
		}

		url_len = sprintf(file_url, "http://%s%s/%s", \
				pStorageServer->ip_addr, szPortPart, file_id);
		if (g_anti_steal_token)
		{
			ts = time(NULL);
			fdfs_http_gen_token(&g_anti_steal_secret_key, \
				file_id, ts, token);
			sprintf(file_url + url_len, "?token=%s&ts=%d", \
				token, (int)ts);
		}

		//fdfs_get_file_info1(file_id, &file_info);
		fdfs_get_file_info_ex1(file_id, false, &file_info);
	

		strcpy(master_file_id, file_id);
		*file_id = '\0';

		if (result != 0)
		{
			printf("upload slave file fail, " \
				"error no: %d, error info: %s\n", \
				result, STRERROR(result));
			tracker_close_connection_ex(pStorageServer, true);
			fdfs_client_destroy_ex(&tracker_group);
			return result;
		}

		if (g_tracker_server_http_port == 80)
		{
			*szPortPart = '\0';
		}
		else
		{
			sprintf(szPortPart, ":%d", g_tracker_server_http_port);
		}
		url_len = sprintf(file_url, "http://%s%s/%s", \
				pStorageServer->ip_addr, szPortPart, file_id);
		if (g_anti_steal_token)
		{
			ts = time(NULL);
			fdfs_http_gen_token(&g_anti_steal_secret_key, \
				file_id, ts, token);
			sprintf(file_url + url_len, "?token=%s&ts=%d", \
				token, (int)ts);
		}

		//fdfs_get_file_info1(file_id, &file_info);
		fdfs_get_file_info_ex1(file_id, false, &file_info);
	

		// if (fdfs_gen_slave_filename(master_file_id, \
        //        		prefix_name, file_ext_name, \
        //         	slave_file_id, &slave_file_id_len) == 0)
		// {
		// 	if (strcmp(file_id, slave_file_id) != 0)
		// 	{
		// 		printf("slave_file_id=%s\n" \
		// 			"file_id=%s\n" \
		// 			"not equal!\n", \
		// 			slave_file_id, file_id);
		// 	}
		// }
	}

	
	{
		fdfs_client_destroy_ex(&tracker_group);
		printf("invalid operation: %s\n", operation);
		return EINVAL;
	}

   
    

	tracker_close_connection_ex(pStorageServer, true);
	tracker_close_connection_ex(pTrackerServer, true);

	fdfs_client_destroy_ex(&tracker_group);
	//fdfs_client_destroy();

	return result;
}


}