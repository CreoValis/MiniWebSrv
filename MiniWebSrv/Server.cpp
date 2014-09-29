#include "Server.h"

#include "IRespSource.h"
#include "IConnFilter.h"

using namespace HTTP;

const boost::posix_time::time_duration Server::StepDuration=boost::posix_time::seconds(StepDurationSeconds);
ConnFilter::AllowAll Server::DefaultConnFilter;
RespSource::CommonError Server::CommonErrRespSource;

Server::Server(unsigned short BindPort) : MyStepTim(MyIOS), ListenEndp(boost::asio::ip::tcp::v4(),BindPort), MyAcceptor(MyIOS,ListenEndp),
	RunTh(nullptr), MyConnF(&DefaultConnFilter), MyRespSource(nullptr), NextConn(nullptr)
{
	
}

Server::Server(boost::asio::ip::address BindAddr, unsigned short BindPort) : MyStepTim(MyIOS), ListenEndp(boost::asio::ip::tcp::v4(),BindPort), MyAcceptor(MyIOS,ListenEndp),
	RunTh(nullptr), MyConnF(&DefaultConnFilter), MyRespSource(nullptr), NextConn(nullptr)
{

}

Server::~Server()
{
	if (MyConnF!=&DefaultConnFilter)
		delete MyConnF;

	delete MyRespSource;

	delete NextConn;
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
	return true;
}

void Server::OnAccept(const boost::system::error_code &error)
{
	if (!error)
	{
		if ((*MyConnF)(PeerEndp.address()))
		{
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

	for (std::list<Connection *>::iterator NowI=ConnLst.begin(), EndI=ConnLst.end(); NowI!=EndI; )
	{
		if ((*NowI)->OnStep(StepDurationSeconds))
			++NowI;
		else
		{
			std::list<Connection *>::iterator DelConnI=NowI;
			++NowI;
			delete *DelConnI;
			ConnLst.erase(DelConnI);
		}
	}

	RestartTimer();
}

void Server::RestartAccept()
{
	if (!NextConn)
		NextConn=new Connection(MyIOS,&CommonErrRespSource);

	MyAcceptor.async_accept(NextConn->GetSocket(),PeerEndp,
		boost::bind(&Server::OnAccept,this,boost::asio::placeholders::error));
}

void Server::RestartTimer()
{
	MyStepTim.expires_from_now(StepDuration);
	MyStepTim.async_wait(boost::bind(&Server::OnTimer,this,boost::asio::placeholders::error));
}
