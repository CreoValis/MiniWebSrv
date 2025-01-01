#pragma once

#include <tuple>

#include "../IResponse.h"

namespace HTTP
{

namespace RespSource
{

class SimpleResponse : public HTTP::IResponse
{
public:
	inline SimpleResponse() : ResponseCode(500), ContentType("text/plain")
	{ }
	inline SimpleResponse(unsigned int ResponseCode, std::string &&ContentType,
		std::vector<std::tuple<std::string, std::string>> &&Headers) :
		ResponseCode(ResponseCode), ContentType(std::move(ContentType)), Headers(std::move(Headers))
	{ }

	virtual unsigned int GetExtraHeaderCount() override { return (unsigned int)Headers.size(); }
	virtual bool GetExtraHeader(unsigned int Index,
		const char **OutHeader, const char **OutHeaderEnd,
		const char **OutHeaderVal, const char **OutHeaderValEnd) override
	{
		if (Index >= Headers.size())
			return false;

		const auto &CurrHeader = Headers[Index];
		*OutHeader = std::get<0>(CurrHeader).data();
		*OutHeaderEnd = std::get<0>(CurrHeader).data() + std::get<0>(CurrHeader).length();
		*OutHeaderVal = std::get<1>(CurrHeader).data();
		*OutHeaderValEnd = std::get<1>(CurrHeader).data() + std::get<1>(CurrHeader).length();
		return true;
	}
	virtual unsigned int GetResponseCode() override { return ResponseCode; }
	virtual const char *GetContentType() const override { return ContentType.data(); }

protected:
	unsigned int ResponseCode;
	std::string ContentType;
	std::vector<std::tuple<std::string, std::string>> Headers;
};

} //RespSource

} //HTTP
