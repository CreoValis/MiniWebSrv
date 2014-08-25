#pragma once

#include "../IRespSource.h"

#include <boost/unordered_map.hpp>
#include <boost/filesystem.hpp>

#include "detail/ZipArchive.h"

namespace HTTP
{

namespace RespSource
{

class Zip : public IRespSource
{
public:
	Zip(const boost::filesystem::path &ArchiveFN);
	virtual ~Zip() { }

	class Response : public IResponse
	{
	public:
		Response(ZipArchive::Stream *ArchS, const char *MimeType, time_t IfModifiedSince=0);
		virtual ~Response() { delete SourceS; }

		virtual unsigned int GetExtraHeaderCount() { return 2; }
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

		enum CONTENTSTATE
		{
			CS_HEADER,
			CS_BODY,
			CS_BODYONLY,
			CS_FOOTER,
		};

		unsigned long long FileSize;
		ZipArchive::Stream *SourceS;
		unsigned char GzipFooterA[8];
		CONTENTSTATE CState;
		unsigned int CStatePos;

		const char *MyMimeType;
		char LastModifiedStr[Header::DateStringLength + 1];
		static const char *IdentityEncoding, *GZipEncoding;
		static unsigned char GZipHeaderA[10];
	};

	virtual IResponse *Create(METHOD Method, const std::string &Resource, const QueryParams &Query, const std::vector<Header> &HeaderA,
		const unsigned char *ContentBuff, const unsigned char *ContentBuffEnd,
		boost::asio::io_service &ParentIOS);

private:
	ZipArchive MyArch;

	static const char *GetMimeType(const std::string &FileName);
};

}; //RespSource

}; //HTTP
