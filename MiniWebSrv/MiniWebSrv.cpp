#include "stdafx.h"

#include <iostream>

#include "Server.h"
#include "RespSources/FSRespSource.h"
#include "RespSources/ZipRespSource.h"
#include "RespSources/CombinerRespSource.h"

HANDLE MainThreadH=INVALID_HANDLE_VALUE;
volatile bool IsRunning=true;

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

int main(int argc, char* argv[])
{
	MainThreadH=GetCurrentThread();
	SetConsoleCtrlHandler(&ConsoleCtrlHandler,TRUE);

	std::cout << "Starting." << std::endl;
	HTTP::Server MiniWS(8880);

	{
		HTTP::RespSource::Combiner *Combiner=new HTTP::RespSource::Combiner();
		Combiner->AddRespSource("/daloskonyv",new HTTP::RespSource::Zip("E:\\test.zip"));
		Combiner->AddRespSource("",new HTTP::RespSource::FS("E:"));

		Combiner->AddSimpleRewrite("/daloskonyv","/daloskonyv/daloskonyv.htm");
		Combiner->AddSimpleRewrite("/husky","/Download/NewFiles/Husky.jpg");

		MiniWS.SetResponseSource(Combiner);
	}

	MiniWS.Run();
	std::cout << "Started." << std::endl;
	while (IsRunning)
	{
		boost::this_thread::sleep(boost::posix_time::milliseconds(100));
		std::cout << "\rAct: " << MiniWS.GetConnCount() << "; Total: " << MiniWS.GetTotalConnCount() << "; Resp: " << MiniWS.GetResponseCount();
	}

	std::cout << "Stopping." << std::endl;
	if (MiniWS.Stop(boost::posix_time::seconds(4)))
		std::cout << "Stopped gracefully." << std::endl;
	else
		std::cout << "Stopped forcefully." << std::endl;

	return 0;
}
