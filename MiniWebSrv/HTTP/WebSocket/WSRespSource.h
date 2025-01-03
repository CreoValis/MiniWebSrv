#pragma once

#include "Common.h"
#include "../IResponse.h"
#include "../IRespSource.h"

namespace HTTP
{

namespace WebSocket
{

class IMsgHandler;

/**Websocket response factory object. To create a websocket connection handler, derive a class from this, and
override the CreateMsgHandler method. This will be called to create an IMsgHandler from the query parameters.*/
class WSRespSource : public IRespSource
{
public:
	inline WSRespSource() : MyServerLog(nullptr) { }
	virtual ~WSRespSource() { }

	class WSResponse : public IResponse
	{
	public:
		WSResponse(IMsgHandler *NewHandler,
			const char *SecWebSocketKey,
			const char *SubProtocol);
		virtual ~WSResponse() { }

		virtual unsigned int GetExtraHeaderCount() { return SubProtocol.empty() ? 3 : 4; }
		virtual bool GetExtraHeader(unsigned int Index,
			const char **OutHeader, const char **OutHeaderEnd,
			const char **OutHeaderVal, const char **OutHeaderValEnd);
		virtual unsigned int GetResponseCode() { return RC_SWITCH_PROT; }
		virtual const char *GetContentType() const { return "text/plain"; }
		virtual const char *GetContentTypeCharset() const { return NULL; }
		/**Queries the response length, in bytes.
		@return The length of the response, or ~0, if it's unknown.*/
		virtual unsigned long long GetLength() { return 0; }
		/**@return True, if there's more data to read.*/
		virtual bool Read(unsigned char *TargetBuff, unsigned int MaxLength, unsigned int &OutLength,
			boost::asio::yield_context &Ctx);

		/**Upgrades the speified connection to another type.
		This method will be called after the response was successfully sent.
		@param CurrConn The connection to upgrade. This is the connection that sent this response.
		@return A new ConnectionBase object, or nullptr, if the connection shouldn't be upgraded.*/
		virtual HTTP::ConnectionBase *Upgrade(HTTP::ConnectionBase *CurrConn);

	private:
		std::string SecWebSocketAccept, SubProtocol;
		IMsgHandler *MyHandler;
	};

	virtual void SetServerLog(IServerLog *NewLog) { MyServerLog=NewLog; }

	virtual IResponse *Create(HTTP::METHOD Method, std::string &Resource, HTTP::QueryParams &Query, std::vector<HTTP::Header> &HeaderA,
		unsigned char *ContentBuff, unsigned char *ContentBuffEnd,
		AsyncHelperHolder AsyncHelpers, void *ParentConn) override;

protected:
	/**Creates a WSResponse from the specified request, if possible.
	@param LogFailed If true, non-websocket requests will be logged as a failed websocket connection attempt.
	@return The first value of the pair specifies if the request contained a websocket upgrade or not. The second value
		specifies the response, if the request was a valid valid websocket upgrade request, and CreateMsgHandler returned
		a valid IMsgHandler object.*/
	std::pair<bool, IResponse *> CreateWSResponse(HTTP::METHOD Method, const std::string &Resource, const HTTP::QueryParams &Query, const std::vector<HTTP::Header> &HeaderA,
		const unsigned char *ContentBuff, const unsigned char *ContentBuffEnd,
		AsyncHelperHolder AsyncHelpers, void *ParentConn, bool LogFailed);
	/**Abstract method, which will be called to create a websocket message handler.
	@param SubProtocolA A list of subprotocols, which the client claims to support. To select one, leave only the
		selected subprotocol name in the vector after the call. Otherwise, no subprotocol will be selected.
	@return A new message handler, which will be used after servicing the upgrade request. If nullptr, then the
		connection can't be upgraded.*/
	virtual IMsgHandler *CreateMsgHandler(const std::string &Resource, const HTTP::QueryParams &Query, std::vector<std::string> &SubProtocolA,
		AsyncHelperHolder AsyncHelpers,
		const char *Origin=nullptr)=0;

	IServerLog *MyServerLog;

	static const std::string ConnUpgradeVal;
	static const std::string WebSocketGUID, UpgradeWebSocketVal;
};

}; //WebSocket

}; //HTTP
