/**
Copyright (C) 2021 - 2022, Beijing Chen An Co.Ltd.All rights reserved.
File Name: ZmqManager.hpp
Description: zeromq manager class
Ref：http://api.zeromq.org/
quote: zeromq-4.3.2
Change History:
Date         Version        Changed By      Changes
2022/01/05   1.0.0.0           dwj          Create
*/

#ifndef ZMQMANAGER_H
#define ZMQMANAGER_H

#include "PublicMethod.hpp"
#include <zmq.h>

#define MAX_SOCKET_SET 100

class ZmqManager final
{
    typedef struct SocketSet {
        bool enable = false;
        std::string prefix = "";
        void *recv = nullptr;
        void *send = nullptr;
    }SocketSet;
    
public:
    /** @brief SocketPair Dependent on the same context */
    ZmqManager();
    ~ZmqManager();

    /** @brief Delete copy and assignment */
    //Move Assignment operator
    ZmqManager& operator=(ZmqManager&& obj) = delete;
    //Move Constructor
    ZmqManager(ZmqManager&& obj) = delete;
    //Copy Constructor
    ZmqManager(ZmqManager& obj) = delete;
    //Copy Assignment operator
    ZmqManager& operator=(ZmqManager& obj) = delete;

    /** @brief 添加数据发布套接字
     * @param host 代理服务端主机地址
     * @return 添加成功: true 添加失败: false
     */
    bool AddPublisherSocket(const std::string host);

    /** @brief 发送消息到代理服务器
     *  @param data 发送数据的承载实体
     *  @param len 发送数据的字节长度
     *  @return 发送成功标志 success->true
     */
    bool SendtoBroker(const std::string& data, std::size_t len);

    /** @brief 添加PAIR对象
     *  @param bind_name The bind_name is a string consisting of a transport
     *    inproc://bind_name followed by an address. 
     *    The name cannot be duplicate. No repeated processing in this api
     *  @return 添加成功: 0~MAX_SOCKET_SET; 添加失败: -1
     */
    int Add(const std::string& bind_name);

    /** @brief 删除PAIR指定对象
     *  @param bind_name The bind_name is a string consisting of a transport
     *    inproc://bind_name followed by an address.
     *  @param prw 指定删除PAIR对象使用的类型，0x01是recv，0x02是send 0x03是all
     *  @return 删除成功: 0~MAX_SOCKET_SET; 删除失败: -1
     */
    int Del(const std::string& bind_name, uint16_t prw = 0x03);

    /** @brief 添加消息轮询的PAIR索引
     *  @param indexes Add函数返回的索引值转换成对应二进制位 eg: Add返回1，3则对应0x1010
     *  @param prw 指定该索引使用的PAIR对端类型，true是recv，false是send
     *  @param timeout 判断套接字是否可读写超时时间 milliseconds
     *  @param rw 指定该索引监听的类型，true是读模式，false是写模式
     *  @return 对应可读数据的索引二进制位
     */
    uint64_t MessagePool(uint64_t indexes, bool prw = true, uint32_t timeout = 0, bool rw = true);

    /** @brief 获取消息
     *  @param index Add函数返回的索引类型
     *  @param data 保存接收到的数据, 参数是inout类型
     *  @param len 保存接收的数据长度, 参数是inout类型
     *  @param prw 指定该索引使用的PAIR对端类型，true是recv，false是send
     *  @return 读取成功标志 success->true
     */
    bool ReceiveMessage(int index, std::string& data, std::size_t& len, bool prw = true);

    /** @brief 发送消息
     *  @param index Add函数返回的索引类型
     *  @param data 发送数据的承载实体
     *  @param len 发送数据的字节长度
     *  @param prw 指定该索引使用的PAIR对端类型，true是send，false是recv
     *  @return 发送成功标志 success->true
     */
    bool SendMessage(int index, const std::string& data, std::size_t len, bool prw = true);

    /** @brief 判断PAIR对象是否存在
     *  @param bind_name The bind_name is a string consisting of a transport
     *    inproc://bind_name followed by an address.
     *  @return 存在返回索引: 0~MAX_SOCKET_SET; 不存在: -1
     */
    int Exist(const std::string& bind_name);

    /** @brief 获取PAIR对象prefix
     *  @param index Add函数返回的索引类型
     *  @return 存在返回PAIR对象prefix 不存在: ""
     */
    std::string GetName(int index);

    /** @brief 获取PAIR对个数
     *  @return 已创建成功的索引数量
     */
    inline uint32_t GetIndexNums(void) { return m_SocketSetNums; }

private:
    void *m_pContext;
    void *m_pBrokerSocket;

    /* array Replace map. Performance priority*/
    SocketSet m_SocketSet[MAX_SOCKET_SET];
    //std::map<std::string, SocketSet> m_obj;

    int m_SocketSetNums;
}; /* ZmqManager end */


#endif  /* ZMQMANAGER_H */