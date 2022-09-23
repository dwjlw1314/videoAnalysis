#include "VideoAlgorithm.hpp"
#ifdef ENABLERZ
void VideoAlgorithm::ReadFrameFFmpeg(CameraData cd, std::shared_ptr<ZmqManager> zmq_mgmt,
    std::map<std::string, IndexData>& zermq_index_data)
{
    cudaSetDevice(0);

    uint64_t rfd = 0;
    int32_t ca_index, tm_index, pa_index;

    if (zermq_index_data.find(cd.CameraId) != zermq_index_data.end())
    {
        ca_index = zermq_index_data[cd.CameraId].index;
        rfd |= (uint64_t)0x0000000000000001 << ca_index;
    }

    tm_index = zermq_index_data["dad4668bc0b8e79cd460bf9f0e0a9700"].index;
    pa_index = zermq_index_data["2759a2762b901db4998d74f394513a86"].index;

    std::shared_ptr<VideoSource> videoDecoder = std::make_shared<VideoSource>();

    if(!videoDecoder->init(cd.CameraUrl))
    {
        std::cout << cd.CameraUrl << std::endl;
    }

    std::mutex mutex;
   
    //模拟发送数据到解码
    zmq_mgmt->SendMessage(ca_index, "1", 1, false);

    std::shared_ptr<MatType>& gpu = zermq_index_data[cd.CameraId].DecodeGpu;
    std::shared_ptr<std::atomic_bool>& use = zermq_index_data[cd.CameraId].used;
    thread_local int count = 0;
    
    while(1)
    {
        //使用发送端接收任务管理递送的数据
        uint64_t bit = zmq_mgmt->MessagePool(rfd,false,5000);
        if (bit & 0xffffffffffffffff)
        {
            std::size_t msg_len;
            std::string zmqMsg;

            zmq_mgmt->ReceiveMessage(ca_index, zmqMsg, msg_len, false);

            //std::cout << "video recv msg: "  << ca_index << std::endl;
            //std::string msg = std::to_string(ca_index);
            //zmq_mgmt->SendMessage(ca_index, msg, msg.length());
        }
        else continue;
        
        while(1) {
        //printf("DecodeGpu addr == %p\n",zermq_index_data[cd.CameraId].DecodeGpu.get());
            count++;
            bool ret = videoDecoder->run(use, gpu, 0, &mutex);

            if (cd.CameraId == "888853fb18fa4692b40fa5c03a0d2eda")
                break;

            if (count > 5)
            {
                count = 0;
                break;
            }
        }
        //std::cout << "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx: " << std::endl;
        // if (!ret) {
        //     std::cout << "videoDecoder finish! cameraID: "  << cd.CameraId << std::endl;
        //     break;
        // }

        //printf("end DecodeGpu addr == %p\n",zermq_index_data[cd.CameraId].DecodeGpu.get());
        //std::cout << "video: "<< count++ << std::endl;
        std::string msg = "1";
        zmq_mgmt->SendMessage(ca_index, msg, msg.length());
    }
}

void VideoAlgorithm::WriteFrameFFmpeg(CameraData cd, std::shared_ptr<ZmqManager> zmq_mgmt,
    std::map<std::string, IndexData>& zermq_index_data)
{
    cudaSetDevice(0);
    std::string ee = cd.CameraId + "E";
    int32_t ca_index = zermq_index_data[ee].index;
    uint64_t rfd = 0;
    rfd |= (uint64_t)0x0000000000000001 << ca_index;

    //std::shared_ptr<CudaEncoder> videoEncoder = std::make_shared<CudaEncoder>();
    CudaEncoder* videoEncoder = new CudaEncoder();
    videoEncoder->SetEncodeInfo(cd.DestUrl);
    videoEncoder->InitVideoEncoder(0, 720, 1280, cd.CameraId);
    std::shared_ptr<MatType> gpu = zermq_index_data[ee].EncodeGpu;
    while(1)
    {
        uint64_t bit = zmq_mgmt->MessagePool(rfd,true,5000);
        if (bit & 0xffffffffffffffff)
        {
            std::size_t msg_len;
            std::string zmqMsg;
            zmq_mgmt->ReceiveMessage(ca_index, zmqMsg, msg_len);

            //std::cout << "WriteFrameFFmpeg index： " << ca_index << std::endl;
            // std::string msg = std::to_string(ca_index);
            // zmq_mgmt->SendMessage(ca_index, msg, msg.length(),false);
            // cv::Mat frame(gpu->cols, gpu->rows, CV_8UC4);
            // gpu->download(frame);
            // cv::imwrite("/opt/123.jpg", frame);

			videoEncoder->StartEncoder(gpu, ca_index);
        }
    }
}
#endif