#pragma once

#include <string>
#include <list>
#include <chrono>
#include <thread>
#include <mutex>

#include <boost/asio.hpp>

#include "BuildConfig.h"
#include "Common.h"
#include "ConnectionBase.h"

#include "ConnFilters/AllowAllConnFilter.h"
#include "RespSources/CommonErrorRespSource.h"
#include "RespSources/CORSPreflightRespSource.h"
#include "ServerLogs/DummyServerLog.h"

namespace HTTP
{

class IConnFilter;
class IRespSource;
class IServerLog;

class Server
{
public:
	Server(unsigned short BindPort);
	Server(boost::asio::ip::address BindAddr, unsigned short BindPort);
	~Server();

	void SetCORS(bool EnableCrossOriginCalls);
	void SetConnectionFilter(IConnFilter *NewCF);
	void SetResponseSource(IRespSource *NewRS);
	void SetServerLog(IServerLog *NewLog);
	void SetName(const std::string &NewName);

	bool Run();
	bool Stop(std::chrono::steady_clock::duration Timeout);

	inline unsigned int GetConnCount() { return ConnCount; }
	inline unsigned int GetTotalConnCount() { return TotalConnCount; }
	inline unsigned int GetResponseCount() { return TotalRespCount; }

protected:
	boost::asio::io_service MyIOS;
	boost::asio::basic_waitable_timer<std::chrono::steady_clock> MyStepTim;
	boost::asio::ip::tcp::endpoint ListenEndp;
	boost::asio::ip::tcp::acceptor MyAcceptor;

	std::thread *RunTh;
	std::timed_mutex RunMtx;
	IConnFilter *MyConnF;
	IRespSource *MyRespSource;
	IServerLog *MyLog;
	std::string MyName;

	volatile unsigned int ConnCount;
	volatile unsigned int TotalConnCount, TotalRespCount;
	unsigned int BaseRespCount;
	std::list<ConnectionBase *> ConnLst;

	boost::asio::ip::tcp::endpoint PeerEndp;
	ConnectionBase *NextConn;
	bool IsRunning;

	RespSource::CORSPreflight *CorsRS;

	static ConnFilter::AllowAll DefaultConnFilter;
	static RespSource::CommonError CommonErrRespSource;
	static RespSource::CORSPreflight CorsPFRespSource;
	static ServerLog::Dummy DefaultServerLog;

	static const unsigned int StepDurationSeconds = 1;
	static const std::chrono::steady_clock::duration StepDuration;

	void OnAccept(const boost::system::error_code &error);
	void OnTimer(const boost::system::error_code &error);
	void StopInternal();

	void RestartAccept();
	void RestartTimer();

	void ProcessThread();
};

};
