#include "Server.h"

#include "IRespSource.h"
#include "IConnFilter.h"

using namespace HTTP;

const boost::posix_time::time_duration Server::StepDuration=boost::posix_time::seconds(StepDurationSeconds);
ConnFilter::AllowAll Server::DefaultConnFilter;
RespSource::CommonError Server::CommonErrRespSource;

Server::Server(unsigned short BindPort) : MyStepTim(MyIOS), ListenEndp(boost::asio::ip::tcp::v4(),BindPort), MyAcceptor(MyIOS,ListenEndp),
	RunTh(nullptr), MyConnF(&DefaultConnFilter), MyRespSource(nullptr),
	ConnCount(0), TotalConnCount(0), TotalRespCount(0), BaseRespCount(0),
	NextConn(nullptr), IsRunning(false)
{
	
}

Server::Server(boost::asio::ip::address BindAddr, unsigned short BindPort) : MyStepTim(MyIOS), ListenEndp(boost::asio::ip::tcp::v4(),BindPort), MyAcceptor(MyIOS,ListenEndp),
	RunTh(nullptr), MyConnF(&DefaultConnFilter), MyRespSource(nullptr),
	ConnCount(0), TotalConnCount(0), TotalRespCount(0), BaseRespCount(0),
	NextConn(nullptr), IsRunning(false)
{

}

Server::~Server()
{
	Stop(boost::posix_time::seconds(0));
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

bool Server::Run()
{
	if (!RunTh)
	{
		IsRunning=true;

		RestartAccept();
		RestartTimer();

		RunTh=new boost::thread(boost::bind(&boost::asio::io_service::run,&MyIOS));
		return true;
	}
	else
		return false;
}

bool Server::Stop(boost::posix_time::time_duration Timeout)
{
	if (RunTh)
	{
		MyIOS.post(boost::bind(&Server::StopInternal,this));
		bool RetVal;
		if (!RunTh->timed_join(Timeout))
		{
			//Terminate the threads forcefully.
			MyIOS.stop();
			//Wait for the last handler invocation to return.
			RunTh->timed_join(boost::posix_time::seconds(1));

			RetVal=false;
		}
		else
			RetVal=true;

		delete RunTh;
		RunTh=nullptr;

		for (std::list<Connection *>::iterator NowI=ConnLst.begin(), EndI=ConnLst.end(); NowI!=EndI; ++NowI)
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
			TotalConnCount++;
			NextConn->Start(MyRespSource);
			ConnLst.push_back(NextConn);
			NextConn=nullptr;
		}
		else
		{
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

	unsigned int ActiveConnCount=0, CurrRespCount=0;
	for (std::list<Connection *>::iterator NowI=ConnLst.begin(), EndI=ConnLst.end(); NowI!=EndI; )
	{
		if ((*NowI)->OnStep(StepDurationSeconds))
		{
			++ActiveConnCount;
			CurrRespCount+=(*NowI)->GetResponseCount();
			++NowI;
		}
		else
		{
			BaseRespCount+=(*NowI)->GetResponseCount();

			std::list<Connection *>::iterator DelConnI=NowI;
			++NowI;
			delete *DelConnI;
			ConnLst.erase(DelConnI);
		}
	}

	ConnCount=ActiveConnCount;
	TotalRespCount=BaseRespCount + CurrRespCount;

	RestartTimer();
}

void Server::StopInternal()
{
	IsRunning=false;
	try { MyAcceptor.close(); }
	catch (...) { }

	try { MyStepTim.cancel(); }
	catch (...) { }

	for (std::list<Connection *>::iterator NowI=ConnLst.begin(), EndI=ConnLst.end(); NowI!=EndI; ++NowI)
		(*NowI)->Stop();
}

void Server::RestartAccept()
{
	if (IsRunning)
	{
		if (!NextConn)
			NextConn=new Connection(MyIOS,&CommonErrRespSource);

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
