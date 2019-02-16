#pragma once

#include <string>

#include "../IRespSource.h"

namespace HTTP
{

namespace RespSource
{

class StaticRespSource : public IRespSource
{
public:
	/**Creates a StaticRespSource by copying the specified string, and using the copy as the response.*/
	StaticRespSource(std::string Response, const char *ContentType = "text/plain", const char *Charset = "utf-8", RESPONSECODE RespCode = RC_OK);
	/**Creates a StaticRespSource by using the specified string's internal data as the response. The string must stay
	constant and alive while any response object is alive.*/
	StaticRespSource(const std::string *Response, const char *ContentType = "text/plain", const char *Charset = "utf-8", RESPONSECODE RespCode = RC_OK);
	/**Creates a StaticRespSource by using the memory pointed to by the two pointers as the response. The string must
	stay constant and alive while any response object is alive.*/
	StaticRespSource(const char *ExtData, const char *ExtDataEnd, const char *ContentType = "text/plain", const char *Charset = "utf-8", RESPONSECODE RespCode = RC_OK);

	virtual IResponse *Create(METHOD Method, const std::string &Resource, const QueryParams &Query, const std::vector<Header> &HeaderA,
		const unsigned char *ContentBuff, const unsigned char *ContentBuffEnd, AsyncHelperHolder AsyncHelpers, void *ParentConn) override;

protected:
	std::string RespStr;
	const char *Data, *DataEnd;

	const char *ContentType, *Charset;
	RESPONSECODE RespCode;
};

} //RespSource

} //HTTP
