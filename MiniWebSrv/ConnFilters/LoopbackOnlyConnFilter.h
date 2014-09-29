#pragma once

#include "../IConnFilter.h"

namespace HTTP
{

namespace ConnFilter
{

class LoopbackOnly : public IConnFilter
{
public:
	virtual ~LoopbackOnly() { }

	virtual bool operator()(boost::asio::ip::address SourceAddr) { return SourceAddr.is_loopback(); }
	virtual bool operator()(boost::asio::ip::address SourceAddr, const std::string &Resource) { return SourceAddr.is_loopback(); }
};

};

};
