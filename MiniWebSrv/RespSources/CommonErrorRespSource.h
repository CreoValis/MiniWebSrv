#include "../IRespSource.h"

#include <stdexcept>

namespace HTTP
{

namespace RespSource
{

class CommonError : public IRespSource
{
public:
	virtual ~CommonError() { }

	class Response : public IResponse
	{
	public:
		Response(const std::string &Resource, const std::vector<Header> &HeaderA, const std::exception *Ex, RESPONSECODE Code=RC_SERVERERROR);
		virtual ~Response() { }

		virtual unsigned int GetResponseCode() { return RespCode; }
		virtual const char *GetContentType() const { return "text/html"; }
		virtual const char *GetContentTypeCharset() const { return "utf-8"; }

		virtual unsigned long long GetLength() { return ~(unsigned int)0; }
		virtual bool Read(unsigned char *TargetBuff, unsigned int MaxLength, unsigned int &OutLength);

	private:
		RESPONSECODE RespCode;
		std::string Res;
		std::string ExType, ExMsg;
		std::string ServerName;
	};

	virtual IResponse *Create(METHOD Method, const std::string &Resource, const QueryParams &Query, const std::vector<Header> &HeaderA,
		const unsigned char *ContentBuff, const unsigned char *ContentBuffEnd)
	{ return CreateFromException(Method,Resource,Query,HeaderA,ContentBuff,ContentBuffEnd,NULL); }

	IResponse *CreateFromException(METHOD Method, const std::string &Resource, const QueryParams &Query, const std::vector<Header> &HeaderA,
		const unsigned char *ContentBuff, const unsigned char *ContentBuffEnd, const std::exception *Ex);

	inline void SetServerName(const char *NewName) { ServerNameStr=NewName; }

protected:
	std::string ServerNameStr;
};

}; //RespSource

}; //HTTP
