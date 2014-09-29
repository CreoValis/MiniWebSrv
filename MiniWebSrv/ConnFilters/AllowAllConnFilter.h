#pragma once

#include "../IConnFilter.h"

namespace HTTP
{

namespace ConnFilter
{

class AllowAll : public IConnFilter
{
public:
	virtual ~AllowAll() { }

	virtual bool operator()(boost::asio::ip::address SourceAddr) { return true; }
	virtual bool operator()(boost::asio::ip::address SourceAddr, const std::string &Resource) { return true; }
};

};

};
