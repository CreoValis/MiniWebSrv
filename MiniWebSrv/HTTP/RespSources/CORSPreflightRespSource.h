#pragma once

#include "../IRespSource.h"

#include <stdexcept>

namespace HTTP
{

namespace RespSource
{

/**Simple CORS preflight request handler, which allows all cross-origin calls.*/
class CORSPreflight : public IRespSource
{
public:
	virtual ~CORSPreflight() { }

	class Response : public IResponse
	{
	public:
		inline Response() { }
		inline Response(const std::string &AllowedHeaders) : AllowedHeaders(AllowedHeaders) { }
		virtual ~Response() { }

		virtual unsigned int GetExtraHeaderCount() { return AllowedHeaders.empty() ? 2 : 3; }
		virtual bool GetExtraHeader(unsigned int Index,
			const char **OutHeader, const char **OutHeaderEnd,
			const char **OutHeaderVal, const char **OutHeaderValEnd);
		virtual unsigned int GetResponseCode() { return HTTP::RC_OK; }

		virtual unsigned long long GetLength() { return 0; }
		virtual bool Read(unsigned char *TargetBuff, unsigned int MaxLength, unsigned int &OutLength, boost::asio::yield_context &Ctx)
		{
			OutLength=0;
			return true;
		}

	private:
		std::string AllowedHeaders;
	};

	virtual IResponse *Create(METHOD Method, std::string &Resource, QueryParams &Query, std::vector<Header> &HeaderA,
		unsigned char *ContentBuff, unsigned char *ContentBuffEnd, AsyncHelperHolder AsyncHelpers, void *ParentConn) override;
};

} //RespSource

} //HTTP
