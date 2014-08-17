#pragma once

#include <string>
#include <vector>

#include <boost/asio.hpp>

#include "Common.h"
#include "QueryParams.h"
#include "IResponse.h"

namespace HTTP
{

/**Base class for response generators.
It can create an IResponse object from the parts of a HTTP request.*/
class IRespSource
{
public:
	virtual ~IRespSource() { }

	virtual IResponse *Create(METHOD Method, const std::string &Resource, const QueryParams &Query, const std::vector<Header> &HeaderA,
		const unsigned char *ContentBuff, const unsigned char *ContentBuffEnd,
		boost::asio::io_service &ParentIOS)=0;
};

}; //HTTP
