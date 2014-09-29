#pragma once

#include <string>

#include <boost/asio.hpp>

namespace HTTP
{

class IConnFilter
{
public:
	virtual ~IConnFilter() { }

	virtual bool operator()(boost::asio::ip::address SourceAddr)=0;
	virtual bool operator()(boost::asio::ip::address SourceAddr, const std::string &Resource) { return this->operator()(SourceAddr); }
};

};
