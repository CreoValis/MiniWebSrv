#include "OStreamServerLog.h"

#include <iostream>

using namespace HTTP::ServerLog;

const char *GetMethodName(HTTP::METHOD Method)
{
	switch (Method)
	{
	case HTTP::METHOD_GET: return "GET ";
	case HTTP::METHOD_POST: return "POST";
	case HTTP::METHOD_HEAD: return "HEAD";
	default: return "-";
	}
}

void OStream::OnConnection(void *Connection, unsigned int SourceAddr, bool IsAllowed)
{
	if (IsAllowed)
	{
		ConnDataHolder &CurrHolder=ConnMap[Connection];

		char AddrBuff[20];
		sprintf(AddrBuff,"%u.%u.%u.%u",
			(SourceAddr >> 24),
			(SourceAddr >> 16) & 0xFF,
			(SourceAddr >>  8) & 0xFF,
			(SourceAddr >>  0) & 0xFF);

		CurrHolder.SourceAddr=AddrBuff;
		CurrHolder.SourceAddr.append((4*3+3)-CurrHolder.SourceAddr.length(),' ');
	}
}

void OStream::OnConnectionFinished(void *Connection)
{
	boost::unordered_map<void *,ConnDataHolder>::iterator FindI=ConnMap.find(Connection);
	if (FindI!=ConnMap.end())
	{
		if (FindI->second.IsWebSocket)
		{
			TargetS <<
				FindI->second.SourceAddr << " WebSocketClose" <<
				std::endl;
		}

		ConnMap.erase(FindI);
	}
}

void OStream::OnRequest(void *Connection, HTTP::METHOD Method, const std::string &Resource, const HTTP::QueryParams &Query, const std::vector<HTTP::Header> &HeaderA,
	const unsigned char *ContentBuff, const unsigned char *ContentBuffEnd,
	unsigned int ResponseCode, unsigned long long ResponseLength, void *UpgradeConn)
{
	ConnDataHolder &CurrHolder=ConnMap[Connection];

	TargetS <<
		CurrHolder.SourceAddr << " " <<
		GetMethodName(Method) << " " <<
		Resource << " (" << (ContentBuffEnd-ContentBuff) << "): "
		<< ResponseCode << " (" << ResponseLength << ")" <<
		std::endl;

	if ((UpgradeConn) && (CurrHolder.IsWebSocket))
		ConnMap[UpgradeConn]=CurrHolder;
}

void OStream::OnWebSocket(void *Connection, const std::string &Resource, bool IsSuccess, const char *Origin, const char *SubProtocol)
{
	if (IsSuccess)
	{
		ConnDataHolder &CurrHolder=ConnMap[Connection];

		TargetS <<
			CurrHolder.SourceAddr << " WebSocket " <<
			(Origin ? Origin : "") << ": " <<
			(SubProtocol ? SubProtocol : "-") <<
			std::endl;

		CurrHolder.IsWebSocket=true;
	}
}
