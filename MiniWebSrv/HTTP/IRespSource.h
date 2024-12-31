#pragma once

#include <string>
#include <vector>

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>

#include "Common.h"
#include "Header.h"
#include "QueryParams.h"
#include "IResponse.h"

namespace HTTP
{

class IServerLog;

/**Base class for response generators.
It can create an IResponse object from the parts of a HTTP request.*/
class IRespSource
{
public:
	virtual ~IRespSource() { }

	struct AsyncHelperHolder
	{
		inline AsyncHelperHolder(boost::asio::strand<boost::asio::io_context::executor_type> &NewStrand, boost::asio::io_context &MyIOS, boost::asio::yield_context &NewCtx) :
			Strand(NewStrand), MyIOS(MyIOS), Ctx(NewCtx)
		{ }

		boost::asio::strand<boost::asio::io_context::executor_type> &Strand;
		boost::asio::io_context &MyIOS;
		boost::asio::yield_context &Ctx;

		inline boost::asio::io_context &IOService() { return MyIOS; }
	};

	/**Called before any other interface calls to set the server log instance.
	The default implementation is a stub.*/
	virtual void SetServerLog(IServerLog *NewLog) { }

	virtual IResponse *Create(METHOD Method, const std::string &Resource, const QueryParams &Query, const std::vector<Header> &HeaderA,
		const unsigned char *ContentBuff, const unsigned char *ContentBuffEnd,
		AsyncHelperHolder AsyncHelpers, void *ParentConn)=0;
};

}; //HTTP
