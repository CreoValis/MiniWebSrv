#pragma once

#include <unordered_map>
#include <string>

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
	static std::unordered_map<std::string,std::string> ExtMimeMap;
	static bool IsExtMimeMapInit;

	static bool InitExtMimeMap();
};

};

}; //HTTP
