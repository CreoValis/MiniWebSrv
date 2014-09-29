#include "stdafx.h"

#include "Server.h"
#include "RespSources/FSRespSource.h"

int main(int argc, char* argv[])
{
	HTTP::Server MiniWS(8880);
	MiniWS.SetResponseSource(new HTTP::RespSource::FS("E:"));
	MiniWS.Run();
	while (true)
		boost::this_thread::sleep(boost::posix_time::seconds(1));

	return 0;
}
