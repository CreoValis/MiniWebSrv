#pragma once

#include <list>
#include <utility>

#include <boost/asio.hpp>

#include "IRespSource.h"

namespace HTTP
{

class IServerLog;

class ConnectionBase
{
public:
	inline ConnectionBase(boost::asio::io_service &MyIOS) : MySock(MyIOS), ResponseCount(0) { }
	inline ConnectionBase(boost::asio::ip::tcp::socket &&SrcSocket) : MySock(std::move(SrcSocket)), ResponseCount(0) { }
	virtual ~ConnectionBase()
	{
		try { MySock.close(); }
		catch (...) { }
	}

	virtual void Start(IRespSource *NewRespSource, IServerLog *NewLog)=0;
	/**Closes the connection socket.*/
	virtual void Stop()=0;
	/**@return False, if the connection is closed, and it should be deleted.*/
	virtual bool OnStep(unsigned int StepInterval, ConnectionBase **OutNextConn)=0;

	inline boost::asio::ip::tcp::socket &GetSocket() { return MySock; }
	inline boost::asio::ip::tcp::socket &&MoveSocket() { return std::move(MySock); }
	inline unsigned int GetResponseCount() const { return ResponseCount; }

protected:
	boost::asio::ip::tcp::socket MySock;

	unsigned int ResponseCount;
};

};
