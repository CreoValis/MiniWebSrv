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
	StaticRespSource(std::string Response, const char *ContentType = "text/plain", const char *Charset = "utf-8", RESPONSECODE RespCode = RC_OK);
	StaticRespSource(const std::string *Response, const char *ContentType = "text/plain", const char *Charset = "utf-8", RESPONSECODE RespCode = RC_OK);
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
