#pragma once

#include <algorithm>

#include "../../IResponse.h"

namespace HTTP
{

/**Simple response class, which can return a string, specified at construction time.*/
class StringResponse : public IResponse
{
public:
	template<class InputIterator>
	inline StringResponse(InputIterator Begin, InputIterator End, const char *ContentType="text/plain", const char *ContentTypeCharset="UTF-8", unsigned int ResponseCode=RC_OK) :
		Response(Begin, End), ContentType(ContentType), ContentTypeCharset(ContentTypeCharset), ResponseCode(ResponseCode), ReadPos(0)
	{ }
	inline StringResponse(const std::string &Source, const char *ContentType="text/plain", const char *ContentTypeCharset="UTF-8", unsigned int ResponseCode=RC_OK) :
		Response(Source), ContentType(ContentType), ContentTypeCharset(ContentTypeCharset), ResponseCode(ResponseCode), ReadPos(0)
	{ }
	inline StringResponse(std::string &&Source, const char *ContentType="text/plain", const char *ContentTypeCharset="UTF-8", unsigned int ResponseCode=RC_OK) :
		Response(std::move(Source)), ContentType(ContentType), ContentTypeCharset(ContentTypeCharset), ResponseCode(ResponseCode), ReadPos(0)
	{ }
	virtual ~StringResponse() { }

	virtual unsigned int GetExtraHeaderCount() { return 0; }
	virtual bool GetExtraHeader(unsigned int Index,
		const char **OutHeader, const char **OutHeaderEnd,
		const char **OutHeaderVal, const char **OutHeaderValEnd)
	{
		return false;
	}
	virtual unsigned int GetResponseCode() { return ResponseCode; }
	virtual const char *GetContentType() const { return ContentType; }
	virtual const char *GetContentTypeCharset() const { return ContentTypeCharset; }
	virtual unsigned long long GetLength() { return Response.length(); }
	virtual bool Read(unsigned char *TargetBuff, unsigned int MaxLength, unsigned int &OutLength,
		boost::asio::yield_context &Ctx)
	{
		if (ReadPos<Response.length())
		{
			auto ReadLen=std::min((std::string::size_type)MaxLength, Response.length()-ReadPos);
			memcpy(TargetBuff, Response.data() + ReadPos, ReadLen);
			OutLength=(unsigned int)ReadLen;
			ReadPos+=ReadLen;
		}
		else
			OutLength=0;

		return ReadPos==Response.length();
	}

private:
	std::string Response;
	unsigned int ResponseCode;
	const char *ContentType, *ContentTypeCharset;

	std::string::size_type ReadPos;
};

} //HTTP
