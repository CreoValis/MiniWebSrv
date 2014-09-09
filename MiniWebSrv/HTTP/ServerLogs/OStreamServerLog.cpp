#include "OStreamServerLog.h"

#include <iostream>
#include <iomanip>

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

void PrintBandwidth(std::ostream &Target, double Value)
{
	std::streamsize PrevPrec=Target.precision(1);
	if (Value>1024*1024*2)
		Target << std::fixed << Value/(1024*1024) << " MB/s";
	else if (Value>1024*2)
		Target << std::fixed << Value/1024 << " kB/s";
	else
		Target << std::fixed << Value << " B/s";
	Target.precision(PrevPrec);
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
	unsigned long long ContentLength, const unsigned char *ContentBuff, const unsigned char *ContentBuffEnd,
	unsigned int ResponseCode, unsigned long long ResponseLength,
	double ReqTime, double RespTime,
	void *UpgradeConn)
{
	boost::unordered_map<void *,ConnDataHolder>::iterator FindI=ConnMap.find(Connection);

	if (FindI!=ConnMap.end())
	{
		TargetS <<
			FindI->second.SourceAddr << " " <<
			GetMethodName(Method) << " " <<
			Resource << " (" << ContentLength << "; ";
		PrintBandwidth(TargetS,ContentLength/ReqTime);
		TargetS << "): "
			<< ResponseCode << " (" << ResponseLength << "; ";
		PrintBandwidth(TargetS,ResponseLength/RespTime);
		TargetS << ")" <<
			std::endl;

		if ((UpgradeConn) && (FindI->second.IsWebSocket))
		{
			ConnMap[UpgradeConn]=FindI->second;
			ConnMap.erase(FindI);
		}
	}
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
