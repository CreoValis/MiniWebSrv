#pragma once

#include <chrono>
#include <boost/bind/bind.hpp>
#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>

#include "Common/StreamReadBuff.h"
#include "Common/WriteBuffQueue.h"

#include "BuildConfig.h"
#include "Common.h"
#include "Header.h"
#include "QueryParams.h"
#include "ConnectionBase.h"
#include "ConnectionConfig.h"
#include "FileUploadConfig.h"

namespace HTTP
{

class IRespSource;
class IServerLog;

namespace RespSource
{
class CommonError;
class CORSPreflight;
};

class Connection : public ConnectionBase
{
public:
	Connection(boost::asio::io_service &MyIOS, RespSource::CommonError *NewErrorRS, RespSource::CORSPreflight *NewCorsPFRS, const char *NewServerName,
		Config::Connection Conf=Config::Connection(), Config::FileUpload FUConf=Config::FileUpload());
	virtual ~Connection();

	virtual void Start(IRespSource *NewRespSource, IServerLog *NewLog);
	/**Closes the connection socket.*/
	virtual void Stop();
	/**@return False, if the connection is closed, and it should be deleted.*/
	virtual bool OnStep(unsigned int StepInterval, ConnectionBase **OutNextConn);

protected:
	boost::asio::io_service::strand MyStrand;

	unsigned int SilentTime;
	bool IsDeletable;

	VERSION CurrVersion;
	METHOD CurrMethod;
	std::string CurrResource;
	QueryParams CurrQuery;
	unsigned long long ContentLength; //Only valid when the client sent some data.
	const unsigned char *ContentBuff, *ContentEndBuff; //Only valid if the current content type is unknown.
	std::vector<Header> HeaderA;
	std::chrono::steady_clock::time_point ReqStartTime;

	const char *ServerName;
	IRespSource *MyRespSource;
	IServerLog *MyLog;

	RespSource::CommonError *ErrorRS;
	RespSource::CORSPreflight *CorsPFRS;

	char *PostHeaderBuff, *PostHeaderBuffEnd;

	UD::Comm::StreamReadBuff<BuildConfig::ReadBuffSize> ReadBuff;
	UD::Comm::WriteBuffQueue<BuildConfig::WriteBuffSize, BuildConfig::WriteQueueInitSize> WriteBuff;

	ConnectionBase *NextConn;

	const Config::Connection Conf;

	void ContinueRead(boost::asio::yield_context &Yield);
	void WriteNext(boost::asio::yield_context &Yield);
	void WriteAll(boost::asio::yield_context &Yield);

	void ProtocolHandler(boost::asio::yield_context Yield);
	bool HeaderHandler(boost::asio::yield_context Yield);
	bool ContentHandler(boost::asio::yield_context Yield);
	bool ParseRequestLine(const unsigned char *Begin, const unsigned char *End);

	/**@return True, if this connection should continue.*/
	bool ResponseHandler(boost::asio::yield_context &Yield);

	void ResetRequestData();

	inline bool HandleCORS() const { return CorsPFRS!=nullptr; }

	static METHOD ParseMethod(const unsigned char *Begin, const unsigned char *End);
};

};
