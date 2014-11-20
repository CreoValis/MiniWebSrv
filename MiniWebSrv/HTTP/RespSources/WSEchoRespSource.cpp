#include "WSEchoRespSource.h"

#include "../WebSocket/IMsgHandler.h"
#include "../WebSocket/IMsgSender.h"

using namespace HTTP::WebSocket;

class EchoMsgHandler : public IMsgHandler
{
public:
	inline EchoMsgHandler() : MySender(nullptr) { }
	virtual ~EchoMsgHandler() { }

	virtual void RegisterSender(IMsgSender *NewSender)
	{
		MySender=NewSender;
	}

	virtual void OnStep(unsigned int StepDuration) { }

	virtual void OnMessage(MESSAGETYPE Type, const unsigned char *Msg, unsigned long long MsgLength)
	{
		boost::unique_lock<boost::mutex> lock(MySender->GetSendMutex());

		unsigned char *SendBuff=MySender->Allocate(Type,MsgLength);
		memcpy(SendBuff,Msg,(unsigned int)MsgLength);
		MySender->Send();
	}
	virtual void OnClose(unsigned short ReasonCode)
	{
		delete this;
	}

private:
	IMsgSender *MySender;
};

IMsgHandler *EchoRespSource::CreateMsgHandler(const std::string &Resource, const HTTP::QueryParams &Query, std::vector<std::string> &SubProtocolA,
	AsyncHelperHolder AsyncHelpers, const char *Origin)
{
	bool IsEchoProt=false;
	for (const std::string &CurrProt : SubProtocolA)
		if (CurrProt=="echo")
		{
			IsEchoProt=true;
			break;
		}

	if (IsEchoProt)
	{
		SubProtocolA.clear();
		SubProtocolA.push_back("echo");
	}
	else
		SubProtocolA.clear();

	return new EchoMsgHandler();
}
