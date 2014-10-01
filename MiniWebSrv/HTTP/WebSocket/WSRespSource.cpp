#include "WSRespSource.h"

#include <stdlib.h>

#include <boost/uuid/sha1.hpp>

#include "../IServerLog.h"
#include "../ConnectionBase.h"
#include "../Common/BinUtils.h"
#include "../Common/StringUtils.h"
#include "../RespSources/CommonErrorRespSource.h"

#include "WSConnection.h"
#include "IMsgHandler.h"

using namespace HTTP::WebSocket;

const std::string WSRespSource::ConnUpgradeVal="upgrade";
const std::string WSRespSource::WebSocketGUID="258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
const std::string WSRespSource::UpgradeWebSocketVal="websocket";

namespace
{

struct StringArrayFiller
{
	StringArrayFiller(std::vector<std::string> &NewTargetA) : TargetA(NewTargetA)
	{ }

	void operator()(const char *Begin, const char *End)
	{
		TargetA.resize(TargetA.size()+1);
		TargetA.back().assign(Begin,End);
	}

private:
	std::vector<std::string> &TargetA;
};

unsigned char Bin6ToBase64(unsigned char Src)
{
	if (Src<26)
		return 'A' + Src;
	else if (Src<52)
		return 'a' - 26 + Src;
	else if (Src<62)
		return '0' - 52 + Src;
	else if (Src==62)
		return '+';
	else //if (Src==63)
		return '/';
	//else
		//return '?';
}

unsigned int Base64Encode(const unsigned char *SrcBuff, const unsigned char *SrcBuffEnd,
		unsigned char *TargetBuff)
{
	unsigned char *OrigTarget=TargetBuff;

	unsigned char Acc=0;
	unsigned int Mod3=-1;
	for (unsigned int x=0, SrcLength=SrcBuffEnd-SrcBuff; x!=SrcLength; ++x)
	{
		++Mod3;
		unsigned char CurrVal=*SrcBuff;
		switch (Mod3)
		{
		case 3:
			Mod3=0;
		case 0:
			*TargetBuff++=Bin6ToBase64(CurrVal >> 2);
			Acc=(CurrVal & 0x3) << 4;
			break;
		case 1:
			Acc|=(CurrVal & 0xF0) >> 4;
			*TargetBuff++=Bin6ToBase64(Acc);
			Acc=(CurrVal & 0xF) << 2;
			break;
		case 2:
			Acc|=(CurrVal & 0xC0) >> 6;
			*TargetBuff++=Bin6ToBase64(Acc);
			*TargetBuff++=Bin6ToBase64(CurrVal & 0x3F);
			break;
		}

		SrcBuff++;
	}

	if (Mod3!=2)
	{
		*TargetBuff++=Bin6ToBase64(Acc);
		if (!Mod3)
		{
			*TargetBuff++='=';
			*TargetBuff++='=';
		}
		else
			*TargetBuff++='=';
	}

	return TargetBuff-OrigTarget;
}


};

WSRespSource::WSResponse::WSResponse(IMsgHandler *NewHandler,
	const char *SecWebSocketKey,
	const char *SubProtocol) : MyHandler(NewHandler), SubProtocol(SubProtocol)
{
	//Create the accept key.
	{
		std::string AuthRespBase=SecWebSocketKey;
		AuthRespBase+=WebSocketGUID;

		unsigned int AuthHash[5];
		boost::uuids::detail::sha1 Hash;
		Hash.process_bytes(AuthRespBase.data(),AuthRespBase.length());
		Hash.get_digest(AuthHash);

		AuthHash[0]=FROM_BIGENDIAN4(AuthHash[0]);
		AuthHash[1]=FROM_BIGENDIAN4(AuthHash[1]);
		AuthHash[2]=FROM_BIGENDIAN4(AuthHash[2]);
		AuthHash[3]=FROM_BIGENDIAN4(AuthHash[3]);
		AuthHash[4]=FROM_BIGENDIAN4(AuthHash[4]);

		unsigned char EncStr[32];
		EncStr[Base64Encode((const unsigned char *)AuthHash,(const unsigned char *)AuthHash+20,EncStr)]='\0';

		SecWebSocketAccept.assign((const char *)EncStr);
	}
}

bool WSRespSource::WSResponse::GetExtraHeader(unsigned int Index,
	const char **OutHeader, const char **OutHeaderEnd,
	const char **OutHeaderVal, const char **OutHeaderValEnd)
{
	if (Index==0)
	{
		const std::string &HName=Header::GetHeaderName(HH_SEC_WEBSOCKET_ACCEPT);
		*OutHeader=HName.data();
		*OutHeaderEnd=HName.data()+HName.length();
		*OutHeaderVal=SecWebSocketAccept.data();
		*OutHeaderValEnd=SecWebSocketAccept.data()+SecWebSocketAccept.length();

		return true;
	}
	if (Index==1)
	{
		const std::string &HName=Header::GetHeaderName(HN_CONNECTION);
		*OutHeader=HName.data();
		*OutHeaderEnd=HName.data()+HName.length();
		*OutHeaderVal=WSRespSource::ConnUpgradeVal.data();
		*OutHeaderValEnd=WSRespSource::ConnUpgradeVal.data()+WSRespSource::ConnUpgradeVal.length();

		return true;
	}
	if (Index==2)
	{
		const std::string &HName=Header::GetHeaderName(HH_UPGRADE);
		*OutHeader=HName.data();
		*OutHeaderEnd=HName.data()+HName.length();
		*OutHeaderVal=WSRespSource::UpgradeWebSocketVal.data();
		*OutHeaderValEnd=WSRespSource::UpgradeWebSocketVal.data()+WSRespSource::UpgradeWebSocketVal.length();

		return true;
	}
	else if ((Index==3) && (!SubProtocol.empty()))
	{
		const std::string &HName=Header::GetHeaderName(HH_SEC_WEBSOCKET_PROTOCOL);
		*OutHeader=HName.data();
		*OutHeaderEnd=HName.data()+HName.length();
		*OutHeaderVal=SubProtocol.data();
		*OutHeaderValEnd=SubProtocol.data()+SubProtocol.length();

		return true;
	}
	else
		return false;
}

bool WSRespSource::WSResponse::Read(unsigned char *TargetBuff, unsigned int MaxLength, unsigned int &OutLength,
	boost::asio::yield_context &Ctx)
{
	//No response body for websocket.
	OutLength=0;
	return true;
}

HTTP::ConnectionBase *WSRespSource::WSResponse::Upgrade(HTTP::ConnectionBase *CurrConn)
{
	WebSocket::Connection *RetConn=new WebSocket::Connection(CurrConn->MoveSocket(),MyHandler);
	MyHandler->RegisterSender(RetConn);
	return RetConn;
}

HTTP::IResponse *WSRespSource::Create(HTTP::METHOD Method, const std::string &Resource, const HTTP::QueryParams &Query, const std::vector<HTTP::Header> &HeaderA,
	const unsigned char *ContentBuff, const unsigned char *ContentBuffEnd,
	boost::asio::io_service &ParentIOS, void *ParentConn)
{
	//Try to create a websocket response.
	auto RespPair=CreateWSResponse(Method,Resource,Query,HeaderA,ContentBuff,ContentBuffEnd,ParentIOS,ParentConn,true);

	if (RespPair.second)
		return RespPair.second;
	else
		//Failed to create a websocket response. We don't care about the reasons, just return "forbidden".
		return new HTTP::RespSource::CommonError::Response(Resource,HeaderA,nullptr,RC_FORBIDDEN);
}

std::pair<bool, HTTP::IResponse *> WSRespSource::CreateWSResponse(HTTP::METHOD Method, const std::string &Resource, const HTTP::QueryParams &Query,
	const std::vector<HTTP::Header> &HeaderA,
	const unsigned char *ContentBuff, const unsigned char *ContentBuffEnd,
	boost::asio::io_service &ParentIOS, void *ParentConn, bool LogFailed)
{
	const Header *UpgradeHdr=nullptr, *ConnHdr=nullptr,
		*WSKeyHdr=nullptr, *WSVerHdr=nullptr, *WSProtHdr=nullptr,
		*OriginHdr=nullptr;
	for (const Header &CurrHdr : HeaderA)
	{
		if (CurrHdr.IntName==HH_UPGRADE)
			UpgradeHdr=&CurrHdr;
		else if (CurrHdr.IntName==HN_CONNECTION)
			ConnHdr=&CurrHdr;
		else if (CurrHdr.IntName==HH_SEC_WEBSOCKET_KEY)
			WSKeyHdr=&CurrHdr;
		else if (CurrHdr.IntName==HH_SEC_WEBSCOKET_VERSION)
			WSVerHdr=&CurrHdr;
		else if (CurrHdr.IntName==HH_SEC_WEBSOCKET_PROTOCOL)
			WSProtHdr=&CurrHdr;
		else if (CurrHdr.IntName==HH_ORIGIN)
			OriginHdr=&CurrHdr;
	}

	std::vector<std::string> SubProtA;
	IMsgHandler *NewHandler;
	bool IsValidWSReq;
	if ((UpgradeHdr) && (strcmp(UpgradeHdr->Value,UpgradeWebSocketVal.data())==0) &&
		//(ConnHdr) && (strcmp(ConnHdr->Value,"upgrade")==0) &&
		(WSKeyHdr) &&
		(WSVerHdr) && (atoi(WSVerHdr->Value)==13))
	{
		//Parse the list of subprotocols.
		if (WSProtHdr)
			UD::StringUtils::ExtractTrimWSTokens(WSProtHdr->Value,',',StringArrayFiller(SubProtA));

		IsValidWSReq=true;
		NewHandler=CreateMsgHandler(Resource,Query,
			SubProtA,OriginHdr ? OriginHdr->Value : nullptr);
	}
	else
	{
		IsValidWSReq=false;
		NewHandler=nullptr;
	}

	if (NewHandler)
	{
		MyServerLog->OnWebSocket(ParentConn,Resource,true,OriginHdr ? OriginHdr->Value : nullptr,
			SubProtA.size()==1 ? SubProtA.back().data() : nullptr);

		return std::pair<bool, HTTP::IResponse *>(true,new WSResponse(NewHandler,WSKeyHdr->Value,SubProtA.size()==1 ? SubProtA[0].data() : ""));
	}
	else
	{
		if (LogFailed)
			MyServerLog->OnWebSocket(ParentConn,Resource,false,OriginHdr ? OriginHdr->Value : nullptr,nullptr);

		return std::pair<bool, HTTP::IResponse *>(IsValidWSReq,nullptr);
	}
}
