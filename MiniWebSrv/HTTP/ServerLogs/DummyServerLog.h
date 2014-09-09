#pragma once

#include "../IServerLog.h"

namespace HTTP
{

namespace ServerLog
{

class Dummy : public IServerLog
{
public:

	virtual void OnConnection(void *Connection, unsigned int SourceAddr, bool IsAllowed) { }
	virtual void OnConnectionFinished(void *Connection) { }

	virtual void OnRequest(void *Connection, HTTP::METHOD Method, const std::string &Resource, const HTTP::QueryParams &Query, const std::vector<HTTP::Header> &HeaderA,
		unsigned long long ContentLength, const unsigned char *ContentBuff, const unsigned char *ContentBuffEnd,
		unsigned int ResponseCode, unsigned long long ResponseLength,double ReqTime, double RespTime,
		void *UpgradeConn=nullptr) { }

	virtual void OnWebSocket(void *Connection, const std::string &Resource, bool IsSuccess, const char *Origin=nullptr, const char *SubProtocol=nullptr) { }
};

}; //ServerLog

}; //HTTP
