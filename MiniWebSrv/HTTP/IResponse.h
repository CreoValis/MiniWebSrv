#pragma once

#include <boost/asio/spawn.hpp>

#include "Common.h"

namespace HTTP
{

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
	/**@return True, if there's more data to read.*/
	virtual bool Read(unsigned char *TargetBuff, unsigned int MaxLength, unsigned int &OutLength,
		boost::asio::yield_context &Ctx)=0;
};

}; //HTTP
