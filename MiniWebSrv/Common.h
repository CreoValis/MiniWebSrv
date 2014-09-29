#pragma once

#include <boost/unordered_map.hpp>

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
	RC_OK           = 200,
	RC_MOVEPERMANENT= 301,
	RC_FOUND        = 302,
	RC_SEEOTHER     = 303,
	RC_NOTMODIFIED  = 304,
	RC_UNAUTHORIZED = 401,
	RC_FORBIDDEN    = 403,
	RC_NOTFOUND     = 404,
	RC_SERVERERROR  = 500,
};

const char *GetResponseName(RESPONSECODE Code);

enum HEADERNAME
{
	HN_UNKNOWN,

	HN_HOST,
	HN_CONNECTION,
	HN_CONTENT_DISPOSITION,
	HN_CONTENT_TYPE,
	HN_CONTENT_LENGTH,
	HN_USER_AGENT,
	HN_IF_MOD_SINCE,
	HN_LAST_MODIFIED,

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
	bool GetContentDispos();
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

}; //HTTP
