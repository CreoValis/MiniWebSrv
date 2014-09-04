#pragma once

#include <string>

#include <boost/thread.hpp>

#include "../BuildConfig.h"
#include "../Common/StreamReadBuff.h"
#include "../Common/WriteBuffQueue.h"

#include "../ConnectionBase.h"

#include "Common.h"

namespace HTTP
{

namespace WebSocket
{

class IMsgHandler;

class Connection : public HTTP::ConnectionBase
{
public:
	Connection(boost::asio::ip::tcp::socket &&SrcSocket);
	virtual ~Connection();

	virtual void Start(IRespSource *NewRespSource) { }
	virtual void Stop();
	virtual bool OnStep(unsigned int StepInterval, ConnectionBase **OutNextConn);

	virtual boost::mutex &GetSendMutex() { return SendBuffMtx; }
	virtual unsigned char *Allocate(MESSAGETYPE Type, unsigned long long Length);
	virtual bool Deallocate();
	virtual bool Send();
	virtual bool SendPing();

	virtual void Close(unsigned short Reason);

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

		OCN_CONTROL_BEGIN =   8,
		OCN_CLOSE         =   OCN_CONTROL_BEGIN,
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

	boost::mutex SendBuffMtx;
	unsigned int SafeStates;

	unsigned int SilentTime;

	unsigned long long CurrFrameLength; //Currently read total frame length (including header).

	UD::Comm::StreamReadBuff<Config::ReadBuffSize> ReadBuff;
	UD::Comm::WriteBuffQueue<Config::WriteBuffSize,Config::WriteQueueInitSize> WriteBuff;

	OPCODENAME FragOpCode; //Fragmented message opcode, or OCN_CONTINUATION .
	std::string FragMsgData; //Fragmented message data.

	IMsgHandler *MyHandler;

	static const unsigned long long UnknownFrameLength = ~(unsigned long long)0;
	static const bool AllowMaskedOnly = true;

	void OnRead(const boost::system::error_code &error, std::size_t bytes_transferred);
	void OnWrite(const boost::system::error_code &error, std::size_t bytes_transferred);

	CLOSEREASON ProcessIncoming();
	CLOSEREASON ProcessFrame(const unsigned char *Buff, unsigned int Length);
	CLOSEREASON ProcessControlFrame(OPCODENAME OpCode, const unsigned char *PayloadBuff, unsigned int Length);

	void OnProtocolError(CLOSEREASON Reason);
	bool SendControlFrame(OPCODENAME OpCode);
	bool SendCloseInternal(unsigned short Reason);

	void StartAsyncWriteExternal();
	void StartAsyncWriteCloseExternal();

	void StartAsyncRead();
	void StartAsyncWrite();

	void PostWriteReq();
	void PostCloseReq();

	template<SAFESTATE State> inline void SetSafeState() { SafeStates|=State; }
	template<SAFESTATE State> inline void ClearSafeState() { SafeStates&=~State; }
	template<SAFESTATE State> inline bool IsSafeState() { return (SafeStates & State)!=0; }

	static inline MESSAGETYPE GetMessageType(OPCODENAME FrameOpCode) { return FrameOpCode==OCN_TEXT ? MSGTYPE_TEXT : MSGTYPE_BINARY; }
	static inline OPCODENAME GetOpcode(MESSAGETYPE MsgType) { return MsgType==MSGTYPE_TEXT ? OCN_TEXT : OCN_BINARY; }
};

}; //WebSocket

}; //namespace HTTP
