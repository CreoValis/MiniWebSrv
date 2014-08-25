#pragma once

#include <boost/unordered_map.hpp>

namespace HTTP
{

namespace detail
{

class MimeDB
{
public:
	static const char *GetMimeType(const std::string &FileExt);
	static inline const char *GetUnknownMimeType() { return UnknownMimeType; }

private:
	static const char *UnknownMimeType;
	static boost::unordered_map<std::string,std::string> ExtMimeMap;
	static bool IsExtMimeMapInit;

	static bool InitExtMimeMap();
};

};

}; //HTTP
