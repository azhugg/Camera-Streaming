
#include "stdafx.h"
#include "tcpclient.h"

using namespace std;
using namespace network;

unsigned WINAPI TCPClientThreadFunction(void* arg);


cTCPClient::cTCPClient()
: m_isConnect(false)
, m_threadLoop(true)
, m_sleepMillis(30)
, m_maxBuffLen(BUFFER_LENGTH)
, m_recvBytes(0)
{
}

cTCPClient::~cTCPClient()
{
	Close();
}


bool cTCPClient::Init(const string &ip, const int port, 
	const int packetSize, const int maxPacketCount, const int sleepMillis,
	const bool isIgnoreHeader)
	// packetSize = 512, maxPacketCount = 10, int sleepMillis = 30, isIgnoreHeader=false
{
	Close();

	m_ip = ip;
	m_port = port;
	m_sleepMillis = sleepMillis;
	m_maxBuffLen = packetSize;

	if (network::LaunchTCPClient(ip, port, m_socket))
	{
		common::dbg::Log("Connect TCP/IP Client ip= %s, port=%d \n", ip.c_str(), port);

		if (!m_recvQueue.Init(packetSize, maxPacketCount, isIgnoreHeader))
		{
			Close();
			return false;
		}

		if (!m_sendQueue.Init(packetSize, maxPacketCount, isIgnoreHeader))
		{
			Close();
			return false;
		}

		m_threadLoop = false;
		if (m_thread.joinable())
			m_thread.join();

 		m_isConnect = true;
 		m_threadLoop = true;
		m_thread = std::thread(TCPClientThreadFunction, this);
	}
	else
	{
		common::dbg::ErrLog("Error!! Connect TCP/IP Client ip= %s, port=%d \n", ip.c_str(), port);
		return false;
	}

	return true;
}


void cTCPClient::Send(BYTE *buff, const int len)
{
	RET(!m_isConnect);
	m_sendQueue.Push(m_socket, buff, len);
}


void cTCPClient::Close()
{
	m_isConnect = false;
	m_threadLoop = false;
	if (m_thread.joinable())
		m_thread.join();

	closesocket(m_socket);
	m_socket = INVALID_SOCKET;
}


unsigned WINAPI TCPClientThreadFunction(void* arg)
{
	cTCPClient *client = (cTCPClient*)arg;
	char *buff = new char[client->m_maxBuffLen];
	const int maxBuffLen = client->m_maxBuffLen;

	while (client->m_threadLoop)
	{
		const timeval t = { 0, client->m_sleepMillis };
		
		//-----------------------------------------------------------------------------------
		// Receive Packet
		fd_set readSockets;
		FD_ZERO(&readSockets);
		FD_SET(client->m_socket, &readSockets);
		const int ret = select(readSockets.fd_count, &readSockets, NULL, NULL, &t);
		if (ret != 0 && ret != SOCKET_ERROR)
		{
			const int result = recv(readSockets.fd_array[0], buff, maxBuffLen, 0);
			if (result == SOCKET_ERROR || result == 0) // 받은 패킷사이즈가 0이면 서버와 접속이 끊겼다는 의미다.
			{
				// error occur
				client->m_isConnect = false;
				client->m_threadLoop = false;
			}
			else
			{
				client->m_recvBytes += result;
				client->m_recvQueue.Push(readSockets.fd_array[0], (BYTE*)buff, result, true);
			}
		}
		//-----------------------------------------------------------------------------------


		//-----------------------------------------------------------------------------------
		// Send Packet
		client->m_sendQueue.SendAll();
		//-----------------------------------------------------------------------------------
	}

	delete[] buff;
	return 0;
}
