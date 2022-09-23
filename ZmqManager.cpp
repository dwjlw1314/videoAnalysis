#include <string.h>
#include "ZmqManager.hpp"

ZmqManager::ZmqManager()
	: m_SocketSetNums(0)
{
	// 1.创建新的上下文对象
	m_pContext = zmq_ctx_new();
	if (nullptr == m_pContext)
		throw "zmq_ctx_new fatal";
}

ZmqManager::~ZmqManager()
{
	/** @todo release socket and context; */
	for(auto node: m_SocketSet)
	{
		if (node.enable)
		{
			zmq_close(node.send);
			zmq_close(node.recv);
		}
	}

	zmq_ctx_destroy(m_pContext);
}

int ZmqManager::Add(const std::string& bind_name)
{
	//int64_t affinity = 0;
	int i = 0, index = -1;
	std::string endpoint = "inproc://" + bind_name;
	for (; i < MAX_SOCKET_SET; i++)
	{
		if (!m_SocketSet[i].enable)
		{
			// 2.创建、绑定PAIR套接字
			void *receiver = zmq_socket(m_pContext, ZMQ_PAIR);
			if (nullptr == receiver) break;

			//zmq_setsockopt (receiver, ZMQ_AFFINITY, &affinity, sizeof (affinity));
			//zmq_setsockopt (receiver, ZMQ_LINGER, &affinity, sizeof (affinity));
			m_SocketSet[i].recv = receiver;
		
			int rc = zmq_bind(receiver, endpoint.c_str());
			if (-1 == rc) break;

			void *sender = zmq_socket(m_pContext, ZMQ_PAIR);
			if (nullptr == sender) break;

			//zmq_setsockopt (sender, ZMQ_AFFINITY, &affinity, sizeof (affinity));
			//zmq_setsockopt (sender, ZMQ_LINGER, &affinity, sizeof (affinity));
			m_SocketSet[i].send = sender;

			rc = zmq_connect(sender, endpoint.c_str());
			if (-1 == rc) break;

			m_SocketSet[i].prefix = endpoint;
			m_SocketSet[i].enable = true;
			index = i;
			m_SocketSetNums++;
			break;
		}
	}

	/* 逻辑异常对资源释放 */
	if (index != i && MAX_SOCKET_SET != i)
	{
		m_SocketSet[i].prefix = "";
		m_SocketSet[i].enable = false;
		if (m_SocketSet[i].send) zmq_close(m_SocketSet[i].send);
		if (m_SocketSet[i].recv) zmq_close(m_SocketSet[i].recv);
	}

	return index;
}

uint64_t ZmqManager::MessagePool(uint64_t indexes, bool prw, uint32_t timeout, bool rw)
{
	thread_local uint32_t count = 0;
	thread_local int32_t map[MAX_SOCKET_SET] = {-1};
	thread_local uint64_t cur_idx = 0;
	thread_local zmq_pollitem_t m_PoolItems[MAX_SOCKET_SET] = {0};
	uint32_t event = rw ? ZMQ_POLLIN : ZMQ_POLLOUT;
	uint64_t bits = 0;

	if (cur_idx != indexes)
	{
		count = 0;
		cur_idx = indexes;
		for (int i = 0; i < MAX_SOCKET_SET; i++)
		{
			map[i] = -1;
			m_PoolItems[i] = {0};
			if ((indexes & 0x01) && m_SocketSet[i].enable)
			{
				if (prw)
					m_PoolItems[count].socket = m_SocketSet[i].recv;
				else
					m_PoolItems[count].socket = m_SocketSet[i].send;
				m_PoolItems[count].events = event;
				map[count++] = i;
			}
			indexes >>= 1;
		}
	}

	if (-1 == zmq_poll (m_PoolItems, count, timeout))
		return bits;
	
	for(int i = 0; i < count; i++)
	{
		if (m_PoolItems[i].revents == event)
			bits |= (uint64_t)0x0000000000000001 << map[i];
	}

	return bits;
}

bool ZmqManager::ReceiveMessage(int index, std::string& data, std::size_t& len, bool rw)
{
	int rc = 0;

	if (index < 0 || index > m_SocketSetNums || !m_SocketSet[index].enable)
		return false;
    
    zmq_msg_t msg;
    if (0 != zmq_msg_init(&msg))
		return false;
    
	if (rw)
    	rc = zmq_msg_recv(&msg, m_SocketSet[index].recv, 0);
	else
		rc = zmq_msg_recv(&msg, m_SocketSet[index].send, 0);

    if(rc == -1) return false;

	len = rc;

	data = std::string((char*)zmq_msg_data(&msg), rc);
 
    zmq_msg_close(&msg);
 
    return true;
}

bool ZmqManager::SendMessage(int index, const std::string& data, std::size_t len, bool rw)
{
	if (index < 0 || index > m_SocketSetNums || !m_SocketSet[index].enable)
		return false;
	
	int sc;
    
    zmq_msg_t msg;
    zmq_msg_init_size(&msg, len);
 
    memcpy(zmq_msg_data(&msg), data.c_str(), len);
    
	if (rw)
    	sc = zmq_msg_send(&msg, m_SocketSet[index].send, ZMQ_DONTWAIT);
	else
		sc = zmq_msg_send(&msg, m_SocketSet[index].recv, ZMQ_DONTWAIT);
    
	if(sc == -1) return false;
	
    zmq_msg_close(&msg);
 
    return true;
}

int ZmqManager::Exist(const std::string& bind_name)
{
	std::string endpoint = "inproc://" + bind_name;
	for(int index = 0; index < MAX_SOCKET_SET; index++)
	{
		if(m_SocketSet[index].enable && !m_SocketSet[index].prefix.compare(endpoint))
			return index;
	}
	return -1;
}

std::string ZmqManager::GetName(int index)
{
	std::string prefix = "";
	if (index < MAX_SOCKET_SET)
		prefix = m_SocketSet[index].prefix;
	//not_prefix = m_SocketSet[index].prefix.erase(0,9);
	return prefix;
}

int ZmqManager::Del(const std::string& bind_name, uint16_t prw)
{
	std::string endpoint = "inproc://" + bind_name;
	for(int index = 0; index < MAX_SOCKET_SET; index++)
	{
		if(m_SocketSet[index].enable && !m_SocketSet[index].prefix.compare(endpoint))
		{
			if (prw & 0x01)
			{
				zmq_close(m_SocketSet[index].recv);
				m_SocketSet[index].recv = nullptr;
			}
			if (prw & 0x02)
			{
				zmq_close(m_SocketSet[index].send);
				m_SocketSet[index].send = nullptr;
			}

			if (!m_SocketSet[index].send && !m_SocketSet[index].recv)
			{
				m_SocketSet[index].enable = false;
				m_SocketSet[index].prefix = "";
				m_SocketSetNums--;
			}

			return index;
		}
	}
	return -1;
}

bool ZmqManager::AddPublisherSocket(const std::string host)
{
	m_pBrokerSocket = zmq_socket (m_pContext, ZMQ_PUB);
    int rc = zmq_connect (m_pBrokerSocket, host.c_str());
    return 0 == rc ? false : true;
}

bool ZmqManager::SendtoBroker(const std::string& data, std::size_t len)
{
	int rc = zmq_send (m_pBrokerSocket, data.c_str(), len, 0);
	return 0 == rc ? false : true;
}