#ifndef _MSC_VER
#include <signal.h>
#endif

#include <iostream>
#include <sstream>
#include <optional>

#include "HTTP/Server.h"
#include "HTTP/RespSources/FSRespSource.h"
#include "HTTP/RespSources/ZipRespSource.h"
#include "HTTP/RespSources/WSEchoRespSource.h"
#include "HTTP/RespSources/CombinerRespSource.h"
#include "HTTP/RespSources/StaticRespSource.h"
#include "HTTP/RespSources/GenericRespSource.h"
#include "HTTP/RespSources/CoroRespSource.h"
#include "HTTP/ServerLogs/OStreamServerLog.h"

 //////////////////////////////////////
// OS-specific helpers.

#ifdef _MSC_VER
#include <windows.h>
#endif

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

 //////////////////////////////////////
// Form parameter tester.

class FormTestRS : public HTTP::IRespSource
{
public:

	class SStreamResp : public HTTP::IResponse
	{
	public:
		virtual unsigned int GetResponseCode() { return HTTP::RC_OK; }
		virtual const char *GetContentType() const { return "text/plain"; }
		virtual const char *GetContentTypeCharset() const { return "utf-8"; }

		virtual unsigned long long GetLength() { return (unsigned long long)MyStream.tellp(); }
		virtual bool Read(unsigned char *TargetBuff, unsigned int MaxLength, unsigned int &OutLength, boost::asio::yield_context &Ctx)
		{
			MyStream.read((char *)TargetBuff,MaxLength);
			return (OutLength=(unsigned int)(MyStream.gcount()))!=0;
		}

		inline operator std::stringstream &() { return MyStream; }

	private:
		std::stringstream MyStream;
	};

	virtual HTTP::IResponse *Create(HTTP::METHOD Method, const std::string &Resource, const HTTP::QueryParams &Query, const std::vector<HTTP::Header> &HeaderA,
		const unsigned char *ContentBuff, const unsigned char *ContentBuffEnd, AsyncHelperHolder AsyncHelpers, void *ParentConn)
	{
		SStreamResp *MyResp=new SStreamResp();
		std::stringstream &RespStream=*MyResp;

		RespStream << "Got method ";
		switch (Method)
		{
		case HTTP::METHOD_GET:
			RespStream << "GET.\n";
			break;
		case HTTP::METHOD_POST:
			RespStream << "POST.\n";
			break;
		case HTTP::METHOD_HEAD:
			RespStream << "HEAD.\n";
			break;
		default:
			RespStream << "(unknown).\n";
			break;
		}

		RespStream << "\nParams:\n";
		for (const HTTP::QueryParams::ParamMapType::value_type &Param : Query.Params())
			RespStream << Param.first << ": \"" << Param.second << "\"\n";

		RespStream << "\nFiles:\n";
		for (const HTTP::QueryParams::FileMapType::value_type &File : Query.Files())
			RespStream << File.first << ": OrigFileName: \"" << File.second.OrigFileName << "\", "
				"Path: \"" << File.second.Path.string() << "\", MimeType: \"" << File.second.MimeType << "\", "
				"[Size]: " << boost::filesystem::file_size(File.second.Path) << "\n";

		return MyResp;
	}
};

 //////////////////////////////////////
// Main entry point.

int main(int argc, char* argv[])
{
#ifdef _MSC_VER
	MainThreadH=GetCurrentThread();
	SetConsoleCtrlHandler(&ConsoleCtrlHandler,TRUE);
#else
	signal(SIGQUIT,&SigHandler);
	signal(SIGINT,&SigHandler);
#endif

	std::string StaticRespStr("<h1>Static response, resource embedded in executable</h1>");

	std::cout << "Starting." << std::endl;
	HTTP::Server MiniWS(8880);
	MiniWS.SetName("MiniWebServer/v0.2.0");

	{
		HTTP::RespSource::Combiner *Combiner=new HTTP::RespSource::Combiner();
		Combiner->AddRespSource("/gallery",new HTTP::RespSource::Zip("../Doc/gallery.zip"));
		Combiner->AddRespSource("/formtest",new FormTestRS());
		Combiner->AddRespSource("/echo",new HTTP::WebSocket::EchoRespSource());
		Combiner->AddRespSource("/static", new HTTP::RespSource::StaticRespSource(&StaticRespStr, "text/html"), true);
		Combiner->AddRespSource("/corotest", HTTP::RespSource::make_generic([](const HTTP::RespSource::GenericBase::CallParams &CallParams) {
			using namespace HTTP::RespSource;
			return new CoroResponse([](const GenericBase::CallParams &CParams, CoroResponse::ResponseParams &RParams, CoroResponse::OutStream &OutS) {

				std::chrono::steady_clock::duration SleepDuration;
				std::optional<boost::asio::steady_timer> SleepTim;
				if (auto SleepMs=CParams.Query.GetPtr("sleep"))
				{
					SleepDuration=std::chrono::milliseconds(strtoul(SleepMs->data(), nullptr, 10));
					if (SleepDuration>std::chrono::steady_clock::duration::zero())
						SleepTim.emplace(CParams.AsyncHelpers.MyIOS);
				}
				else
					SleepDuration=std::chrono::steady_clock::duration::zero();

				RParams.SetResponseCode(200);
				RParams.SetContentType("text/plain");
				RParams.GetResponseHeaders().emplace_back("X-Custom", "Custom header value");
				RParams.Finalize(OutS);

				//Send 9 bytes in 3 Write() calls.
				char Response[]="abcdefghi";
				constexpr unsigned int StepSize=3, ResponseSize=sizeof(Response)-1;
				for (unsigned int x=0; x<ResponseSize; x+=StepSize)
				{
					const auto CopyLen=std::min(StepSize, ResponseSize-x);
					memcpy(OutS.GetBuff(), Response + x, CopyLen);
					OutS.Write(CopyLen);

					if (SleepTim)
					{
						SleepTim->expires_after(SleepDuration);
						SleepTim->async_wait(OutS.GetContext());
					}
				}

				//Exiting the coroutine will finalize the response.
			}, CallParams);
		}));

		Combiner->AddRespSource("", new HTTP::RespSource::FS("../Doc"));

		Combiner->AddRedirect("/","/test.html");

		MiniWS.SetResponseSource(Combiner);

		MiniWS.SetServerLog(new HTTP::ServerLog::OStream(std::cout));
	}

	MiniWS.Run();
	std::cout << "Started." << std::endl;
	while (IsRunning)
		std::this_thread::sleep_for(std::chrono::milliseconds(100));

	std::cout << "Stopping." << std::endl;
	if (MiniWS.Stop(std::chrono::seconds(4)))
		std::cout << "Stopped gracefully." << std::endl;
	else
		std::cout << "Stopped forcefully." << std::endl;

	return 0;
}
