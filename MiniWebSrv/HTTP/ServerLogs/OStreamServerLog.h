#pragma once

#include <ostream>

#include <unordered_map>

#include "../IServerLog.h"

namespace HTTP
{

namespace ServerLog
{

class OStream : public IServerLog
{
public:
	inline OStream(std::ostream &Target) : TargetS(Target) { }
	virtual ~OStream() { }

	virtual void OnConnection(void *Connection, unsigned int SourceAddr, bool IsAllowed);
	virtual void OnConnectionFinished(void *Connection);

	virtual void OnRequest(void *Connection, HTTP::METHOD Method, const std::string &Resource, const HTTP::QueryParams &Query, const std::vector<HTTP::Header> &HeaderA,
		unsigned long long ContentLength, const unsigned char *ContentBuff, const unsigned char *ContentBuffEnd,
		unsigned int ResponseCode, unsigned long long ResponseLength,
		double ReqTime, double RespTime,
		void *UpgradeConn=nullptr);
	virtual void OnWebSocket(void *Connection, const std::string &Resource, bool IsSuccess, const char *Origin=nullptr, const char *SubProtocol=nullptr);

private:
	struct ConnDataHolder
	{
		inline ConnDataHolder() : IsWebSocket(false) { }

		std::string SourceAddr, WSTarget;
		bool IsWebSocket;
	};

	std::ostream &TargetS;

	std::unordered_map<void *,ConnDataHolder> ConnMap;

	static void PrintNow(std::ostream &Target);
};

}; //ServerLog

}; //HTTP
