#pragma once

#include <time.h>
#include <string>
#include <stdexcept>

#include <boost/unordered_map.hpp>

namespace HTTP
{

enum HEADERNAME
{
	HN_UNKNOWN,

	HN_HOST,
	HN_CONNECTION,
	HN_CONTENT_DISPOSITION,
	HN_CONTENT_TYPE,
	HN_CONTENT_LENGTH,
	HN_CONTENT_ENCODING,
	HN_USER_AGENT,
	HN_IF_MOD_SINCE,
	HH_ORIGIN,
	HH_UPGRADE,
	HH_SEC_WEBSOCKET_KEY,
	HH_SEC_WEBSCOKET_VERSION,
	HH_SEC_WEBSOCKET_PROTOCOL,
	HN_LAST_MODIFIED,
	HN_LOCATION,
	HH_SEC_WEBSOCKET_ACCEPT,

	HN_NOTUSED,
};

enum CONTENTTYPE
{
	CT_UNKNOWN,

	CT_URL_ENCODED,
	CT_FORM_MULTIPART,

	CT_NOTUSED,
};

struct Header
{
	struct SkipParseOption { };

	inline Header() : Name(nullptr), Value(nullptr) { }
	Header(char *LineBegin, char *LineEnd);
	Header(char *LineBegin, char *LineEnd, SkipParseOption);

	class FormatException : public std::runtime_error
	{
	public:
		FormatException(const char *Message) : std::runtime_error(Message)
		{ }
	};

	static const unsigned int DateStringLength = 29;

	const char *Name, *Value;

	HEADERNAME IntName;

	/**Parses the header value, as if it were a content-type.*/
	CONTENTTYPE GetContentType(std::string &OutBoundary) const;
	void ParseContentDisposition(std::string &OutName, std::string &OutFileName) const;
	bool IsConnectionClose() const;
	/**Parses the header value, as if were a http-date.
	@throw FormatException
	@return The parsed timestamp.*/
	time_t GetDateTime() const;
	/**@throw FormatException*/
	long long GetLongLong() const;
	/**@throw FormatException*/
	unsigned long long GetULongLong() const;

	static HEADERNAME ParseHeader(const char *Name);
	static const std::string &GetHeaderName(HEADERNAME Name);
	static char *ExtractHeader(char *LineBegin, char *LineEnd, char **OutName, char **OutValue);

	/**@param TargetBuff Buffer to write the date string. Must be at least 30 bytes long.*/
	static void FormatDateTime(time_t Timestamp, char *TargetBuff);

private:
	static const std::string BoundaryParamName;

	static bool IsHeaderNameInit;
	static boost::unordered_map<std::string,HEADERNAME> HeaderNameMap;
	static bool InitHeaderNameMap();
	static inline void AddToHeaderMap(HEADERNAME Name) { HeaderNameMap[GetHeaderName(Name)]=Name; }
};

};
