#pragma once

#include <algorithm>
#include <utility>

#include "../../IResponse.h"

namespace HTTP
{

/**Simple response class, which can return a string, specified at construction time.*/
class StringResponse : public IResponse
{
public:
	template<class InputIterator>
	inline StringResponse(InputIterator Begin, InputIterator End, const char *ContentType="text/plain", const char *ContentTypeCharset="UTF-8", unsigned int ResponseCode=RC_OK) :
		Response(Begin, End), ContentType(ContentType), ContentTypeCharset(ContentTypeCharset), ResponseCode(ResponseCode),
		ReadPos(Response.data()), EndPos(Response.data() + Response.size())
	{ }
	inline StringResponse(const std::string &Source, const char *ContentType="text/plain", const char *ContentTypeCharset="UTF-8", unsigned int ResponseCode=RC_OK) :
		Response(Source), ContentType(ContentType), ContentTypeCharset(ContentTypeCharset), ResponseCode(ResponseCode),
		ReadPos(Response.data()), EndPos(Response.data() + Response.size())
	{ }
	inline StringResponse(std::string &&Source, const char *ContentType="text/plain", const char *ContentTypeCharset="UTF-8", unsigned int ResponseCode=RC_OK) :
		Response(std::move(Source)), ContentType(ContentType), ContentTypeCharset(ContentTypeCharset), ResponseCode(ResponseCode),
		ReadPos(Response.data()), EndPos(Response.data() + Response.size())
	{ }
	/**Constructs a StringResponse object, which uses the specified string as it's source. The string must be available
	and stay constant while the response is alive.*/
	inline StringResponse(const std::string *Source, const char *ContentType="text/plain", const char *ContentTypeCharset="UTF-8", unsigned int ResponseCode=RC_OK) :
		ContentType(ContentType), ContentTypeCharset(ContentTypeCharset), ResponseCode(ResponseCode),
		ReadPos(Source->data()), EndPos(Source->data() + Source->size())
	{ }
	/**Constructs a StringResponse object, which uses the specified (begin, end) pair as it's source. The buffer must
	be available and stay constant while the response is alive.*/
	inline StringResponse(std::pair<const char *, const char *> Source, const char *ContentType = "text/plain", const char *ContentTypeCharset = "UTF-8", unsigned int ResponseCode = RC_OK) :
		ContentType(ContentType), ContentTypeCharset(ContentTypeCharset), ResponseCode(ResponseCode),
		ReadPos(Source.first), EndPos(Source.second)
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
	virtual unsigned long long GetLength() { return EndPos-ReadPos; }
	virtual bool Read(unsigned char *TargetBuff, unsigned int MaxLength, unsigned int &OutLength,
		boost::asio::yield_context &Ctx)
	{
		if (ReadPos<EndPos)
		{
			auto ReadLen=std::min((std::string::size_type)MaxLength, (std::string::size_type)(EndPos-ReadPos));
			memcpy(TargetBuff, ReadPos, ReadLen);
			OutLength=(unsigned int)ReadLen;
			ReadPos+=ReadLen;
		}
		else
			OutLength=0;

		return ReadPos>=EndPos;
	}

private:
	std::string Response;
	unsigned int ResponseCode;
	const char *ContentType, *ContentTypeCharset;

	const std::string::value_type *ReadPos, *EndPos;
};

} //HTTP
