#include "Server.h"

#include "IRespSource.h"
#include "IConnFilter.h"

#include "Connection.h"

using namespace HTTP;

const std::chrono::steady_clock::duration Server::StepDuration=std::chrono::seconds(StepDurationSeconds);
ConnFilter::AllowAll Server::DefaultConnFilter;
RespSource::CommonError Server::CommonErrRespSource;
RespSource::CORSPreflight Server::CorsPFRespSource;
ServerLog::Dummy Server::DefaultServerLog;

Server::Server(unsigned short BindPort, boost::asio::io_context *Target) :
	MyIOS(Target ? *Target : OwnIOS),
	MyStepTim(MyIOS), ListenEndp(boost::asio::ip::tcp::v4(),BindPort), MyAcceptor(MyIOS,ListenEndp),
	RunTh(nullptr), MyConnF(&DefaultConnFilter), MyRespSource(nullptr), MyLog(&DefaultServerLog), MyName("EmbeddedHTTPd"),
	ConnCount(0), TotalConnCount(0), TotalRespCount(0), BaseRespCount(0),
	NextConn(nullptr), IsRunning(false),
	CorsRS(nullptr)
{
	
}

Server::Server(boost::asio::ip::address BindAddr, unsigned short BindPort, boost::asio::io_context *Target) :
	MyIOS(Target ? *Target : OwnIOS),
	MyStepTim(MyIOS), ListenEndp(boost::asio::ip::tcp::v4(),BindPort), MyAcceptor(MyIOS,ListenEndp),
	RunTh(nullptr), MyConnF(&DefaultConnFilter), MyRespSource(nullptr), MyLog(&DefaultServerLog),
	ConnCount(0), TotalConnCount(0), TotalRespCount(0), BaseRespCount(0),
	NextConn(nullptr), IsRunning(false),
	CorsRS(nullptr)
{

}

Server::~Server()
{
	Stop(std::chrono::steady_clock::duration(0));
	delete MyRespSource;

	if (MyConnF!=&DefaultConnFilter)
		delete MyConnF;

	if (MyLog!=&DefaultServerLog)
		delete MyLog;
}

void Server::SetCORS(bool EnableCrossOriginCalls)
{
	if (EnableCrossOriginCalls)
		CorsRS=&CorsPFRespSource;
	else
		CorsRS=nullptr;
}

void Server::SetConnectionFilter(IConnFilter *NewCF)
{
	if (NewCF)
	{
		if (MyConnF!=&DefaultConnFilter)
			delete MyConnF;

		MyConnF=NewCF;
	}
	else
		MyConnF=&DefaultConnFilter;
}

void Server::SetResponseSource(IRespSource *NewRS)
{
	delete MyRespSource;
	MyRespSource=NewRS;
}

void Server::SetServerLog(IServerLog *NewLog)
{
	if (NewLog)
	{
		if (MyLog!=&DefaultServerLog)
			delete MyLog;

		MyLog=NewLog;
	}
	else
		MyLog=&DefaultServerLog;
}

void Server::SetName(const std::string &NewName)
{
	MyName=NewName;
}

void Server::SetConfig(const Config::Connection &ConnConf, const Config::FileUpload &FUConf)
{
	this->ConnConf = ConnConf;
	this->FUConf = FUConf;
}

bool Server::Run()
{
	if (!RunTh)
	{
		IsRunning=true;

		MyRespSource->SetServerLog(MyLog);

		RestartAccept();
		RestartTimer();

		RunTh=new std::thread(&Server::ProcessThread, this);
		return true;
	}
	else
		return false;
}

bool Server::Stop(std::chrono::steady_clock::duration Timeout)
{
	if (RunTh)
	{
		boost::asio::post(MyIOS, boost::bind(&Server::StopInternal, this));
		bool RetVal, IsLocked;
		if (!RunMtx.try_lock_for(Timeout))
		{
			//Terminate the threads forcefully.
			MyIOS.stop();
			//Wait for the last handler invocation to return.
			IsLocked=RunMtx.try_lock_for(std::chrono::seconds(1));

			RetVal=false;
		}
		else
			IsLocked=RetVal=true;

		if (IsLocked)
			RunMtx.unlock();

		if (RunTh->joinable())
			RunTh->join();
		else
			RunTh->detach();

		delete RunTh;
		RunTh=nullptr;

		for (std::list<ConnectionBase *>::iterator NowI=ConnLst.begin(), EndI=ConnLst.end(); NowI!=EndI; ++NowI)
			delete *NowI;

		ConnLst.clear();
		try { NextConn->Stop(); delete NextConn; NextConn=nullptr; }
		catch (...) { }

		return RetVal;
	}
	else
		return true;
}

void Server::OnAccept(const boost::system::error_code &error)
{
	if (!error)
	{
		if ((*MyConnF)(PeerEndp.address()))
		{
			MyLog->OnConnection(NextConn,(unsigned int)PeerEndp.address().to_v4().to_uint(), true);

			TotalConnCount.fetch_add(1, std::memory_order_acq_rel);
			NextConn->Start(MyRespSource,MyLog);
			ConnLst.push_back(NextConn);
			NextConn=nullptr;
		}
		else
		{
			MyLog->OnConnection(NextConn,(unsigned int)PeerEndp.address().to_v4().to_uint(),false);

			NextConn->GetSocket().close();
			delete NextConn; //Lazy.
			NextConn=nullptr;
		}
	}

	RestartAccept();
}

void Server::OnTimer(const boost::system::error_code &error)
{
	if (error)
		return;

	std::list<ConnectionBase *> NextConnLst;

	unsigned int ActiveConnCount=0, CurrRespCount=0;
	for (std::list<ConnectionBase *>::iterator NowI=ConnLst.begin(), EndI=ConnLst.end(); NowI!=EndI; )
	{
		ConnectionBase *NextConn=nullptr;
		if ((*NowI)->OnStep(StepDurationSeconds,&NextConn))
		{
			++ActiveConnCount;
			CurrRespCount+=(*NowI)->GetResponseCount();
			++NowI;
		}
		else
		{
			BaseRespCount+=(*NowI)->GetResponseCount();

			std::list<ConnectionBase *>::iterator DelConnI=NowI;
			++NowI;
			delete *DelConnI;
			ConnLst.erase(DelConnI);
		}

		if (NextConn)
			NextConnLst.push_back(NextConn);
	}

	//Append the new connections to the connection list.
	ConnLst.splice(ConnLst.end(),std::move(NextConnLst));

	ConnCount.store(ActiveConnCount, std::memory_order_release);
	TotalRespCount.store(BaseRespCount + CurrRespCount, std::memory_order_release);

	RestartTimer();
}

void Server::StopInternal()
{
	IsRunning=false;
	try { MyAcceptor.close(); }
	catch (...) { }

	try { MyStepTim.cancel(); }
	catch (...) { }

	for (std::list<ConnectionBase *>::iterator NowI=ConnLst.begin(), EndI=ConnLst.end(); NowI!=EndI; ++NowI)
		(*NowI)->Stop();
}

void Server::RestartAccept()
{
	if (IsRunning)
	{
		if (!NextConn)
			//Create a new HTTP Connection object.
			NextConn=new Connection(MyIOS,&CommonErrRespSource,CorsRS,MyName.data(), ConnConf, FUConf);

		MyAcceptor.async_accept(NextConn->GetSocket(),PeerEndp,
			boost::bind(&Server::OnAccept,this,boost::asio::placeholders::error));
	}
}

void Server::RestartTimer()
{
	if (IsRunning)
	{
		MyStepTim.expires_from_now(StepDuration);
		MyStepTim.async_wait(boost::bind(&Server::OnTimer,this,boost::asio::placeholders::error));
	}
}

void Server::ProcessThread()
{
	std::unique_lock<std::timed_mutex> lock(RunMtx);
	MyIOS.run();
}
