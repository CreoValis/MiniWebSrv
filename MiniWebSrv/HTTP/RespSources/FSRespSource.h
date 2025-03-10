#pragma once

#include "../IRespSource.h"

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

		virtual unsigned long long GetLength() { return FileSize!=NotModifiedSize ? FileSize : 0; }
		virtual bool Read(unsigned char *TargetBuff, unsigned int MaxLength, unsigned int &OutLength,
			boost::asio::yield_context &Ctx);

	protected:
		void CloseStream() { InS.close(); }

	private:
		static const unsigned long long NotModifiedSize=~(unsigned long long)0;

		unsigned long long FileSize, FilePos;

		std::ifstream InS;

		const char *MyMimeType;
		char LastModifiedStr[Header::DateStringLength + 1];
	};

	virtual IResponse *Create(METHOD Method, std::string &Resource, QueryParams &Query, std::vector<Header> &HeaderA,
		unsigned char *ContentBuff, unsigned char *ContentBuffEnd,
		AsyncHelperHolder AsyncHelpers, void *ParentConn) override;

private:
	boost::filesystem::path Root;

	static const char *GetMimeType(const boost::filesystem::path &FileName);
};

}; //RespSource

}; //HTTP
