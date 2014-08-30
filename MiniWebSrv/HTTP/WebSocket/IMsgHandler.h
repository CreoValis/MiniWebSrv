#pragma once

#include "Common.h"

namespace HTTP
{

namespace WebSocket
{

/**Interface which receives asynchronous messages and events about an active websocket connection.*/
class IMsgHandler
{
public:
	virtual void OnMessage(MESSAGETYPE Type, const unsigned char *Msg, unsigned long long MsgLength)=0;
	virtual void OnClose(unsigned int ReasonCode)=0;
};

}; //WebSocket

}; //namespace HTTP
