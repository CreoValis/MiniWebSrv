#include "CommonErrorRespSource.h"

using namespace HTTP;
using namespace HTTP::RespSource;

CommonError::Response::Response(const std::string &Resource, const std::vector<Header> &HeaderA, const std::exception *Ex, RESPONSECODE Code) :
	RespCode(Code), Res(Resource)
{
	if (Ex)
	{
		ExType=typeid(*Ex).name();
		ExMsg=Ex->what();
	}
	else
		ExType="[Unknown]";
}

bool CommonError::Response::Read(unsigned char *TargetBuff, unsigned int MaxLength, unsigned int &OutLength)
{
	switch (RespCode)
	{
	case RC_UNAUTHORIZED:
		OutLength=(unsigned int)sprintf_s((char *)TargetBuff,MaxLength,
			"<h1>401 Unauthorized</h1>\n"
			"Access denied for resource: <b>%s</b><br />\n"
			"Sorry :( !",
			Res.data());
		break;
	case RC_FORBIDDEN:
		OutLength=(unsigned int)sprintf_s((char *)TargetBuff,MaxLength,
			"<h1>403 Forbidden</h1>\n"
			"Access denied for resource: <b>%s</b><br />\n"
			"Sorry :( !",
			Res.data());
		break;
	case RC_NOTFOUND:
		OutLength=(unsigned int)sprintf_s((char *)TargetBuff,MaxLength,
			"<h1>404 Not found</h1>\n"
			"The requested resource was not found: <b>%s</b><br />\n"
			"Sorry :( !",
			Res.data());
		break;
	default:
		OutLength=(unsigned int)sprintf_s((char *)TargetBuff,MaxLength,
			"<h1>500 Internal error</h1>\n"
			"Caught an exception while creating a response for <b>%s</b>:\n"
			"<ul>\n"
			"<li>Type: <pre>%s</pre></li>\n"
			"<li>Message: %s</li>\n"
			"</ul>\n"
			"Sorry :( !",
			Res.data(),ExType.data(),ExMsg.data());
		break;
	}

	return true;
}

IResponse *CommonError::CreateFromException(METHOD Method, const std::string &Resource, const QueryParams &Query, const std::vector<Header> &HeaderA,
	const unsigned char *ContentBuff, const unsigned char *ContentBuffEnd, const std::exception *Ex)
{
	return new Response(Resource,HeaderA,Ex);
}
