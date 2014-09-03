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
	enum FLAGS
	{
		FLAG_FIN     = 1 << 7, //In the first byte.
		FLAG_RESERVED= 7 << 4, //In the first byte.
		FLAG_MASK    = 1 << 7, //In the second byte.
	};

	enum OPCODENAME
	{
		OCN_CONTINUATION  =   0,
		OCN_TEXT          =   1,
		OCN_BINARY        =   2,
		OCN_CLOSE         =   8,
		OCN_PING          =   9,
		OCN_PONG          = 0xA,

		OCN_MASK          = 0xF,
	};

	enum SAFESTATE
	{
		SAFE_READ = 1 << 0,
		SAFE_WRITE = 1 << 1,

		SAFE_ALL = SAFE_READ | SAFE_WRITE,
	};

	boost::mutex SocketMtx;
	unsigned int SafeStates;

	unsigned int SilentTime;

	unsigned long long CurrFrameLength; //Currently read total frame length (including header).

	UD::Comm::StreamReadBuff<Config::ReadBuffSize> ReadBuff;
	UD::Comm::WriteBuffQueue<Config::WriteBuffSize,Config::WriteQueueInitSize> WriteBuff;

	OPCODENAME FragOpCode; //Fragmented message opcode, or OCN_CONTINUATION .
	std::vector<unsigned char> FragMsgA; //Fragmented message data.

	static const unsigned long long UnknownFrameLength = ~(unsigned long long)0;

	void OnRead(const boost::system::error_code &error, std::size_t bytes_transferred);
	void OnWrite(const boost::system::error_code &error, std::size_t bytes_transferred);

	void ProcessIncoming();
	void ProcessFrame(const unsigned char *Buff, unsigned int Length);

	void StartAsyncRead();
	void StartAsyncWrite();

	template<SAFESTATE State> inline void SetSafeState() { SafeStates|=State; }
	template<SAFESTATE State> inline void ClearSafeState() { SafeStates&=~State; }
};

}; //WebSocket

}; //namespace HTTP
