#include "CORSPreflightRespSource.h"

namespace HTTP
{

namespace RespSource
{


bool CORSPreflight::Response::GetExtraHeader(unsigned int Index,
	const char **OutHeader, const char **OutHeaderEnd,
	const char **OutHeaderVal, const char **OutHeaderValEnd)
{
	if (Index==0)
	{
		static const std::string Name("Access-Control-Allow-Origin"), Value("*");
		*OutHeader=Name.data();
		*OutHeaderEnd=Name.data() + Name.length();
		*OutHeaderVal=Value.data();
		*OutHeaderValEnd=Value.data() + Value.length();
		return true;
	}
	else if (Index==1)
	{
		static const std::string Name("Access-Control-Request-Methods"), Value("GET, POST");
		*OutHeader=Name.data();
		*OutHeaderEnd=Name.data() + Name.length();
		*OutHeaderVal=Value.data();
		*OutHeaderValEnd=Value.data() + Value.length();
		return true;
	}
	else if ((Index==2) && (!AllowedHeaders.empty()))
	{
		static const std::string Name("Access-Control-Allow-Headers");
		*OutHeader=Name.data();
		*OutHeaderEnd=Name.data() + Name.length();
		*OutHeaderVal=AllowedHeaders.data();
		*OutHeaderValEnd=AllowedHeaders.data() + AllowedHeaders.length();
		return true;
	}
	else
		return false;
}

IResponse *CORSPreflight::Create(METHOD Method, const std::string &Resource, const QueryParams &Query, const std::vector<Header> &HeaderA,
	const unsigned char *ContentBuff, const unsigned char *ContentBuffEnd, AsyncHelperHolder AsyncHelpers, void *ParentConn)
{
	if (Method==HTTP::METHOD_OPTIONS)
	{
		bool ACRMPresent=false;
		const char *ACReqHeaders=nullptr;
		for (const auto &CurrHeader : HeaderA)
		{
			switch (CurrHeader.IntName)
			{
			case HTTP::HN_ACCESS_CONTROL_REQUEST_METHOD:
				ACRMPresent=true;
				break;
			case HTTP::HN_ACCESS_CONTROL_REQUEST_HEADERS:
				ACReqHeaders=CurrHeader.Value;
				break;
			}
		}

		if (ACRMPresent)
		{
			if (ACReqHeaders)
				return new Response(ACReqHeaders);
			else
				return new Response();
		}
		else
			//This isn't a preflight request.
			return nullptr;
	}
	else
		//A CORS preflight request's method is OPTIONS.
		return nullptr;
}

} //RespSource

} //HTTP
