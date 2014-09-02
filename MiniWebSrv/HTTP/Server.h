#pragma once

#include <string>
#include <list>
#include <boost/date_time.hpp>
#include <boost/thread.hpp>
#include <boost/asio.hpp>

#include "BuildConfig.h"
#include "Common.h"
#include "ConnectionBase.h"

#include "ConnFilters/AllowAllConnFilter.h"
#include "RespSources/CommonErrorRespSource.h"

namespace HTTP
{

class IConnFilter;
class IRespSource;

class Server
{
public:
	Server(unsigned short BindPort);
	Server(boost::asio::ip::address BindAddr, unsigned short BindPort);
	~Server();

	void SetConnectionFilter(IConnFilter *NewCF);
	void SetResponseSource(IRespSource *NewRS);

	bool Run();
	bool Stop(boost::posix_time::time_duration Timeout);

	inline unsigned int GetConnCount() { return ConnCount; }
	inline unsigned int GetTotalConnCount() { return TotalConnCount; }
	inline unsigned int GetResponseCount() { return TotalRespCount; }

protected:
	boost::asio::io_service MyIOS;
	boost::asio::deadline_timer MyStepTim;
	boost::asio::ip::tcp::endpoint ListenEndp;
	boost::asio::ip::tcp::acceptor MyAcceptor;

	boost::thread *RunTh;
	IConnFilter *MyConnF;
	IRespSource *MyRespSource;

	volatile unsigned int ConnCount;
	volatile unsigned int TotalConnCount, TotalRespCount;
	unsigned int BaseRespCount;
	std::list<ConnectionBase *> ConnLst;

	boost::asio::ip::tcp::endpoint PeerEndp;
	ConnectionBase *NextConn;
	bool IsRunning;

	static ConnFilter::AllowAll DefaultConnFilter;
	static RespSource::CommonError CommonErrRespSource;

	static const unsigned int StepDurationSeconds = 1;
	static const boost::posix_time::time_duration StepDuration;

	void OnAccept(const boost::system::error_code &error);
	void OnTimer(const boost::system::error_code &error);
	void StopInternal();

	void RestartAccept();
	void RestartTimer();
};

};
