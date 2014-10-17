#ifdef _MSC_VER
#include <windows.h>
#else
#include <signal.h>
#endif
#include "stdafx.h"

#include <iostream>

#include "HTTP/Server.h"
#include "HTTP/RespSources/FSRespSource.h"
#include "HTTP/RespSources/ZipRespSource.h"
#include "HTTP/RespSources/WSEchoRespSource.h"
#include "HTTP/RespSources/CombinerRespSource.h"
#include "HTTP/ServerLogs/OStreamServerLog.h"


volatile bool IsRunning=true;
#ifdef _MSC_VER
HANDLE MainThreadH=INVALID_HANDLE_VALUE;

BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType)
{
	switch (dwCtrlType)
	{
	case CTRL_C_EVENT:
	case CTRL_CLOSE_EVENT:
	case CTRL_LOGOFF_EVENT:
	case CTRL_SHUTDOWN_EVENT:
		IsRunning=false;
		WaitForSingleObject(MainThreadH,5500);
		break;
	}

	return TRUE;
}
#else
void SigHandler(int sig)
{
	if ((sig==SIGQUIT) || (sig==SIGINT))
		IsRunning=false;
}
#endif

int main(int argc, char* argv[])
{
#ifdef _MSC_VER
	MainThreadH=GetCurrentThread();
	SetConsoleCtrlHandler(&ConsoleCtrlHandler,TRUE);
#else
	signal(SIGQUIT,&SigHandler);
	signal(SIGINT,&SigHandler);
#endif

	std::cout << "Starting." << std::endl;
	HTTP::Server MiniWS(8880);
	MiniWS.SetName("MiniWebServer/v0.2.0");

	{
		HTTP::RespSource::Combiner *Combiner=new HTTP::RespSource::Combiner();
		Combiner->AddRespSource("/daloskonyv",new HTTP::RespSource::Zip("E:\\test.zip"));
		Combiner->AddRespSource("/echo",new HTTP::WebSocket::EchoRespSource());
		Combiner->AddRespSource("",new HTTP::RespSource::FS("E:"));

		Combiner->AddRedirect("/daloskonyv","/daloskonyv/");
		Combiner->AddSimpleRewrite("/daloskonyv/","/daloskonyv/daloskonyv.htm");
		Combiner->AddSimpleRewrite("/husky","/Download/NewFiles/Husky.jpg");

		MiniWS.SetResponseSource(Combiner);

		MiniWS.SetServerLog(new HTTP::ServerLog::OStream(std::cout));
	}

	MiniWS.Run();
	std::cout << "Started." << std::endl;
	while (IsRunning)
	{
		boost::this_thread::sleep(boost::posix_time::milliseconds(100));
		//std::cout << "\rAct: " << MiniWS.GetConnCount() << "; Total: " << MiniWS.GetTotalConnCount() << "; Resp: " << MiniWS.GetResponseCount();
	}

	std::cout << "Stopping." << std::endl;
	if (MiniWS.Stop(boost::posix_time::seconds(4)))
		std::cout << "Stopped gracefully." << std::endl;
	else
		std::cout << "Stopped forcefully." << std::endl;

	return 0;
}
