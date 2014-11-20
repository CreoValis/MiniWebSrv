#pragma once

#include "../WebSocket/WSRespSource.h"

namespace HTTP
{

namespace WebSocket
{

class EchoRespSource : public WSRespSource
{
public:
	EchoRespSource () { }
	virtual ~EchoRespSource() { }

protected:
	virtual IMsgHandler *CreateMsgHandler(const std::string &Resource, const HTTP::QueryParams &Query, std::vector<std::string> &SubProtocolA,
		AsyncHelperHolder AsyncHelpers, const char *Origin=nullptr);

};

};

};
