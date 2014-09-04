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
	/**Called when a new message arrives on the websocket connection.
	As this method will be called on the HTTPd thread, it should not block for long.*/
	virtual void OnMessage(MESSAGETYPE Type, const unsigned char *Msg, unsigned long long MsgLength)=0;
	/**Called when the peer closes the websocket connection. After this call, no more messages could be sent.
	As this method will be called on the HTTPd thread, it should not block for long.*/
	virtual void OnClose(unsigned short ReasonCode)=0;
};

}; //WebSocket

}; //namespace HTTP
