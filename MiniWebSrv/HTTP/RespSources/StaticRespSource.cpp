#include "StaticRespSource.h"

#include "Helper/StringResponse.h"

namespace HTTP
{

namespace RespSource
{

StaticRespSource::StaticRespSource(std::string Response, const char *ContentType, const char *Charset, RESPONSECODE RespCode) :
	RespStr(std::move(RespStr)), Data(RespStr.data()), DataEnd(RespStr.data() + RespStr.length()),
	ContentType(ContentType), Charset(Charset), RespCode(RespCode)
{ }
StaticRespSource::StaticRespSource(const std::string *Response, const char *ContentType, const char *Charset, RESPONSECODE RespCode) :
	Data(Response->data()), DataEnd(Response->data() + Response->length()),
	ContentType(ContentType), Charset(Charset), RespCode(RespCode)
{ }
StaticRespSource::StaticRespSource(const char *ExtData, const char *ExtDataEnd, const char *ContentType, const char *Charset, RESPONSECODE RespCode) :
	Data(ExtData), DataEnd(ExtDataEnd),
	ContentType(ContentType), Charset(Charset), RespCode(RespCode)
{}

IResponse *StaticRespSource::Create(METHOD Method, std::string &Resource, QueryParams &Query, std::vector<Header> &HeaderA,
	unsigned char *ContentBuff, unsigned char *ContentBuffEnd, AsyncHelperHolder AsyncHelpers, void *ParentConn)
{
	return new StringResponse(std::make_pair(Data, DataEnd), ContentType, Charset, RespCode);
}

} //RespSource

} //HTTP
