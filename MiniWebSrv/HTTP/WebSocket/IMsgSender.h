#pragma once

#include "Common.h"

namespace HTTP
{

namespace WebSocket
{

/**Helper interface, which can be used by WebSocket::IMsgHandler objects to send messages on a websocket
connection.*/
class IMsgSender
{
public:
	/**Allocates space for the websocket message in the send buffer, and prepares the frame header.
	There can be only one allocated message in the send buffer at a time.
	@return Pointer where the message should be written, or nullptr, if there was an error.*/
	virtual unsigned char *Allocate(MESSAGETYPE Type, unsigned long long Length)=0;
	/**Frees the previously allocated space in the send buffer.*/
	virtual bool Deallocate()=0;
	/**Sends the previously allocated message.*/
	virtual bool Send()=0;

	/**Explicitly closes the connection with the specified reason code.*/
	virtual void Close(unsigned int Reason)=0;
};

}; //WebSocket

}; //namespace HTTP