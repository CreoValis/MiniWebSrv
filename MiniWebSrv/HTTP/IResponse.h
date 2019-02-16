#pragma once

#include <boost/asio/spawn.hpp>

#include "Common.h"
#include "Header.h"

namespace HTTP
{

class ConnectionBase;

/**Base class for response reader classes.*/
class IResponse
{
public:
	virtual ~IResponse() { }

	virtual unsigned int GetExtraHeaderCount() { return 0; }
	virtual bool GetExtraHeader(unsigned int Index,
		const char **OutHeader, const char **OutHeaderEnd,
		const char **OutHeaderVal, const char **OutHeaderValEnd) { return false; }
	virtual unsigned int GetResponseCode() { return RC_OK; }
	virtual const char *GetContentType() const { return "text/plain"; }
	virtual const char *GetContentTypeCharset() const { return NULL; }
	/**Queries the response length, in bytes.
	@return The length of the response, or ~0, if it's unknown.*/
	virtual unsigned long long GetLength()=0;
	/**@return True, if the response is finished.*/
	virtual bool Read(unsigned char *TargetBuff, unsigned int MaxLength, unsigned int &OutLength,
		boost::asio::yield_context &Ctx)=0;

	/**Upgrades the specified connection to another type.
	This method will be called after the response was successfully sent.
	@param CurrConn The connection to upgrade. This is the connection that sent this response.
	@return A new ConnectionBase object, or nullptr, if the connection shouldn't be upgraded.*/
	virtual ConnectionBase *Upgrade(ConnectionBase *CurrConn) { return NULL; }
};

}; //HTTP
