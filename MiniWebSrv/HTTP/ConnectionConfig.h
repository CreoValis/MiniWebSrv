#pragma once

namespace HTTP
{

namespace Config
{

struct Connection
{
	unsigned int MaxHeaderLength = 1024;
	unsigned int MaxHeadersLength = 4 * 1024;
	unsigned int MaxPostBodyLength = 16 * 1024 * 1024;

	unsigned int MaxSilentTime = 30;
};

} //Config

} //HTTP
