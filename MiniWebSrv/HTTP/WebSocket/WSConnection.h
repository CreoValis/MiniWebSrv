#pragma once

#include <boost/thread.hpp>

#include "../BuildConfig.h"
#include "../Common/StreamReadBuff.h"
#include "../Common/WriteBuffQueue.h"

#include "../ConnectionBase.h"

namespace HTTP
{

namespace WebSocket
{

class Connection : public HTTP::ConnectionBase
{
public:
	Connection(boost::asio::ip::tcp::socket &&SrcSocket);
	virtual ~Connection();

	virtual void Start(IRespSource *NewRespSource) { }
	virtual void Stop();
	virtual bool OnStep(unsigned int StepInterval, ConnectionBase **OutNextConn);

private:
	enum SAFESTATE
	{
		SAFE_READ = 1 << 0,
		SAFE_WRITE = 1 << 1,

		SAFE_ALL = SAFE_READ | SAFE_WRITE,
	};

	boost::mutex SocketMtx;
	unsigned int SafeStates;

	unsigned int SilentTime;

	UD::Comm::StreamReadBuff<Config::ReadBuffSize> ReadBuff;
	UD::Comm::WriteBuffQueue<Config::WriteBuffSize,Config::WriteQueueInitSize> WriteBuff;

	void OnRead(const boost::system::error_code &error, std::size_t bytes_transferred);
	void OnWrite(const boost::system::error_code &error, std::size_t bytes_transferred);

	void StartAsyncRead();
	void StartAsyncWrite();

	template<SAFESTATE State> inline void SetSafeState() { SafeStates|=State; }
	template<SAFESTATE State> inline void ClearSafeState() { SafeStates&=~State; }
};

}; //WebSocket

}; //namespace HTTP
