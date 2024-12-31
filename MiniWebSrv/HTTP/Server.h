#pragma once

#include <string>
#include <list>
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>

#include <boost/asio.hpp>

#include "BuildConfig.h"
#include "Common.h"
#include "ConnectionBase.h"
#include "ConnectionConfig.h"
#include "FileUploadConfig.h"

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
	Server(unsigned short BindPort, boost::asio::io_context *Target=nullptr);
	Server(boost::asio::ip::address BindAddr, unsigned short BindPort, boost::asio::io_context *Target=nullptr);
	~Server();

	void SetCORS(bool EnableCrossOriginCalls);
	void SetConnectionFilter(IConnFilter *NewCF);
	void SetResponseSource(IRespSource *NewRS);
	void SetServerLog(IServerLog *NewLog);
	void SetName(const std::string &NewName);
	void SetConfig(const Config::Connection &ConnConf, const Config::FileUpload &FUConf);

	bool Run();
	bool Stop(std::chrono::steady_clock::duration Timeout);

	inline unsigned int GetConnCount() { return ConnCount.load(std::memory_order_consume); }
	inline unsigned int GetTotalConnCount() { return TotalConnCount.load(std::memory_order_consume); }
	inline unsigned int GetResponseCount() { return TotalRespCount.load(std::memory_order_consume); }

protected:
	boost::asio::io_context &MyIOS;
	boost::asio::io_context OwnIOS;
	boost::asio::basic_waitable_timer<std::chrono::steady_clock> MyStepTim;
	boost::asio::ip::tcp::endpoint ListenEndp;
	boost::asio::ip::tcp::acceptor MyAcceptor;

	std::thread *RunTh;
	std::timed_mutex RunMtx;
	IConnFilter *MyConnF;
	IRespSource *MyRespSource;
	IServerLog *MyLog;
	std::string MyName;

	std::atomic_uint32_t ConnCount;
	std::atomic_uint32_t TotalConnCount, TotalRespCount;
	unsigned int BaseRespCount;
	std::list<ConnectionBase *> ConnLst;

	boost::asio::ip::tcp::endpoint PeerEndp;
	ConnectionBase *NextConn;
	bool IsRunning;

	RespSource::CORSPreflight *CorsRS;

	Config::Connection ConnConf;
	Config::FileUpload FUConf;

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

	inline const bool IsOwnIOS() const { return &MyIOS==&OwnIOS; }
};

};
