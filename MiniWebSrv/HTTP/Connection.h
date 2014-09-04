#pragma once

#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>

#include "Common/StreamReadBuff.h"
#include "Common/WriteBuffQueue.h"

#include "BuildConfig.h"
#include "Common.h"
#include "Header.h"
#include "QueryParams.h"
#include "ConnectionBase.h"

namespace HTTP
{

class IRespSource;

namespace RespSource
{
class CommonError;
};

class Connection : public ConnectionBase
{
public:
	Connection(boost::asio::io_service &MyIOS, RespSource::CommonError *NewErrorRS);
	virtual ~Connection();

	virtual void Start(IRespSource *NewRespSource);
	/**Closes the connection socket.*/
	virtual void Stop();
	/**@return False, if the connection is closed, and it should be deleted.*/
	virtual bool OnStep(unsigned int StepInterval, ConnectionBase **OutNextConn);

protected:
	boost::asio::strand MyStrand;

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

	ConnectionBase *NextConn;

	static const std::string WebSocketGUID, UpgradeWebSocketVal;

	void ContinueRead(boost::asio::yield_context &Yield);
	void WriteNext(boost::asio::yield_context &Yield);
	void WriteAll(boost::asio::yield_context &Yield);

	void ProtocolHandler(boost::asio::yield_context Yield);
	bool ResponseHandler(boost::asio::yield_context &Yield, const Header *Upgrade=nullptr,
		const Header *WebSocketKey=nullptr, const Header *WebSocketProtocol=nullptr, const Header *WebSocketVersion=nullptr);
	bool WebSocketResponseHandler(boost::asio::yield_context &Yield,
		unsigned int Version, const char *Key, const char *SubProtocols);

	void CreatePostHeaderBuff();
	char *AddPostHeader(const unsigned char *Begin, const unsigned char *End);

	static METHOD ParseMethod(const unsigned char *Begin, const unsigned char *End);
};

};
