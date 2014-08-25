#pragma once

#include "../IRespSource.h"

#include <boost/unordered_map.hpp>
#include <boost/filesystem.hpp>

namespace HTTP
{

namespace RespSource
{

class FS : public IRespSource
{
public:
	FS(const boost::filesystem::path &NewRoot);
	virtual ~FS() { }

	class Response : public IResponse
	{
	public:
		Response(const boost::filesystem::path &FileName, const char *MimeType, time_t IfModifiedSince=0);
		virtual ~Response() { }

		virtual unsigned int GetExtraHeaderCount() { return 1; }
		virtual bool GetExtraHeader(unsigned int Index,
			const char **OutHeader, const char **OutHeaderEnd,
			const char **OutHeaderVal, const char **OutHeaderValEnd);
		virtual unsigned int GetResponseCode() { return FileSize != NotModifiedSize ? RC_OK : RC_NOTMODIFIED; }
		virtual const char *GetContentType() const { return MyMimeType; }
		virtual const char *GetContentTypeCharset() const { return NULL; }

		virtual unsigned long long GetLength() { return FileSize; }
		virtual bool Read(unsigned char *TargetBuff, unsigned int MaxLength, unsigned int &OutLength,
			boost::asio::yield_context &Ctx);

	private:
		static const unsigned long long NotModifiedSize=~(unsigned long long)0;

		unsigned long long FileSize, FilePos;

		std::ifstream InS;

		const char *MyMimeType;
		char LastModifiedStr[Header::DateStringLength + 1];
	};

	virtual IResponse *Create(METHOD Method, const std::string &Resource, const QueryParams &Query, const std::vector<Header> &HeaderA,
		const unsigned char *ContentBuff, const unsigned char *ContentBuffEnd,
		boost::asio::io_service &ParentIOS);

private:
	boost::filesystem::path Root;

	static const char *GetMimeType(const boost::filesystem::path &FileName);
};

}; //RespSource

}; //HTTP
