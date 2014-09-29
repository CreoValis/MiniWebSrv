#pragma once

#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/coroutine/all.hpp>

#include "Common/StreamReadBuff.h"
#include "Common/WriteBuffQueue.h"

#include "BuildConfig.h"
#include "Common.h"
#include "QueryParams.h"
#include "Connection.h"

namespace HTTP
{

class IRespSource;

namespace RespSource
{
class CommonError;
};

class Connection
{
public:
	Connection(boost::asio::io_service &MyIOS, RespSource::CommonError *NewErrorRS);
	~Connection();

	void Start(IRespSource *NewRespSource);
	/**@return False, if the connection is closed, and it should be deleted.*/
	bool OnStep(unsigned int StepInterval);

	inline boost::asio::ip::tcp::socket &GetSocket() { return MySock; }

protected:
	boost::asio::strand MyStrand;
	boost::asio::ip::tcp::socket MySock;

	unsigned int SilentTime;
	bool IsDeletable;

	METHOD CurrMethod;
	std::string CurrResource;
	QueryParams CurrQuery;
	const unsigned char *ContentBuff, *ContentEndBuff; //Only valid if the current content type is unknown.
	std::vector<Header> HeaderA;

	const char *ServerName;
	IRespSource *MyRespSource;

	RespSource::CommonError *ErrorRS;

	char *PostHeaderBuff, *PostHeaderBuffEnd, *PostHeaderBuffPos;

	UD::Comm::StreamReadBuff<Config::ReadBuffSize> ReadBuff;
	UD::Comm::WriteBuffQueue<Config::WriteBuffSize,Config::WriteQueueInitSize> WriteBuff;

	void ContinueRead(boost::asio::yield_context &Yield);
	void WriteNext(boost::asio::yield_context &Yield);

	void ProtocolHandler(boost::asio::yield_context Yield);
	bool ResponseHandler(boost::asio::yield_context &Yield);

	void CreatePostHeaderBuff();
	char *AddPostHeader(const unsigned char *Begin, const unsigned char *End);

	static METHOD ParseMethod(const unsigned char *Begin, const unsigned char *End);
};

};
