#pragma once

namespace HTTP
{

enum METHOD
{
	METHOD_GET,
	METHOD_HEAD,
	METHOD_POST,

	METHOD_UNKNOWN,
};

enum RESPONSECODE
{
	RC_SWITCH_PROT  = 101,
	RC_OK           = 200,
	RC_MOVEPERMANENT= 301,
	RC_FOUND        = 302,
	RC_SEEOTHER     = 303,
	RC_NOTMODIFIED  = 304,
	RC_TEMP_REDIRECT= 307,
	RC_UNAUTHORIZED = 401,
	RC_FORBIDDEN    = 403,
	RC_NOTFOUND     = 404,
	RC_SERVERERROR  = 500,
};

const char *GetResponseName(RESPONSECODE Code);

}; //HTTP
