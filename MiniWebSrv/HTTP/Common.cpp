#include "Common.h"

const char *HTTP::GetResponseName(RESPONSECODE Code)
{
	switch (Code)
	{
	case RC_OK: return "OK";
	case RC_MOVEPERMANENT: return "MovedPermanently";
	case RC_FOUND: return "Found";
	case RC_SEEOTHER: return "SeeOther";
	case RC_NOTMODIFIED: return "NotModified";
	case RC_UNAUTHORIZED: return "Unauthorized";
	case RC_FORBIDDEN: return "YouShallNotPass";
	case RC_NOTFOUND: return "NotFound";
	case RC_SERVERERROR: return "ServerError";
	default: return "Whatever";
	}
}
