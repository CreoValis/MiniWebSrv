#pragma once

#include <string>
#include <vector>

#include "Common.h"
#include "QueryParams.h"
#include "Header.h"

namespace HTTP
{

class IServerLog
{
public:
	virtual ~IServerLog() { }

	/**Called when a client initiates a new connection.
	@param Connection Address of the new connection object. Could be used to track requests on a single connection.
	@param SourceAddr Source IPv4 address.
	@param IsAllowed If true, the connection was allowed to proceed by the IConnFilter .*/
	virtual void OnConnection(void *Connection, unsigned int SourceAddr, bool IsAllowed) { }
	virtual void OnConnectionFinished(void *Connection) { }

	/**Called when a request is finished processing.
	@param Connection Address of the connection, which processed the request.
	@param ResponseLength Number of bytes sent in the response. Note that this is the number of bytes sent on the TCP/IP
		connection.
	@param UpgradeConn If the response upgraded the connection, then the address of the new connection. Note that
		upgraded connections don't get a separate OnConnection() call.*/
	virtual void OnRequest(void *Connection, HTTP::METHOD Method, const std::string &Resource, const HTTP::QueryParams &Query, const std::vector<HTTP::Header> &HeaderA,
		unsigned long long ContentLength, const unsigned char *ContentBuff, const unsigned char *ContentBuffEnd,
		unsigned int ResponseCode, unsigned long long ResponseLength, void *UpgradeConn=nullptr)=0;
	/**Called when a websocket connection is negotiated.
	@param Connection Original connection, which processed the websocket upgrade request.
	@param IsSuccess If true, the websocket handshake succeeded.
	@param Origin Pointer the value of the "Origin" header, if present.
	@param SubProtocol Pointer the selected subprotocol, if any.*/
	virtual void OnWebSocket(void *Connection, const std::string &Resource, bool IsSuccess, const char *Origin=nullptr, const char *SubProtocol=nullptr) { }
};

}; //HTTP
