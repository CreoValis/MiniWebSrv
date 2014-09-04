#pragma once

#include <boost/thread/mutex.hpp>

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

	/**Retrieves the send mutex, which must be held while calling the methods of this class.*/
	virtual boost::mutex &GetSendMutex()=0;
	/**Allocates space for the websocket message in the send buffer, and prepares the frame header.
	There can be only one allocated message in the send buffer at a time.
	@return Pointer where the message should be written, or nullptr, if there was an error.*/
	virtual unsigned char *Allocate(MESSAGETYPE Type, unsigned long long Length)=0;
	/**Frees the previously allocated space in the send buffer.*/
	virtual bool Deallocate()=0;
	/**Sends the previously allocated message.*/
	virtual bool Send()=0;

	/**Sends a "ping" message. Can only called when there's no allocated space in the send buffer.
	@see Allocate */
	virtual bool SendPing()=0;

	/**Explicitly closes the connection with the specified reason code.*/
	virtual void Close(unsigned short Reason)=0;
};

}; //WebSocket

}; //namespace HTTP
