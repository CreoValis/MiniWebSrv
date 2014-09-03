#pragma once

namespace HTTP
{

namespace Config
{
	const unsigned int MaxHeaderLength = 1024;
	const unsigned int MaxHeadersLength = 4*1024;
	const unsigned int MaxPostBodyLength = 16*1024*1024;

	const unsigned int ReadBuffSize = 4*1024;
	const unsigned int WriteBuffSize = 24*1024;
	const unsigned int WriteQueueInitSize = 8;

	const unsigned int MaxSilentTime = 30;
};

namespace WebSocket
{

namespace Config
{
	const unsigned int ReadBuffSize = 4*1024;
	const unsigned int WriteBuffSize = 4*1024;
	const unsigned int WriteQueueInitSize = 8;

	const unsigned int MaxSilentTime = 1*60*60;
	const unsigned int MaxPingInterval = 30;//5*60;
};

};

};
