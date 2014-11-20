#pragma once

#include <memory>
#include <vector>

#include <boost/unordered_map.hpp>

#include "../IRespSource.h"

namespace HTTP
{

namespace RespSource
{

class Combiner : public IRespSource
{
public:
	virtual ~Combiner() { }

	class RedirectResponse : public IResponse
	{
	public:
		RedirectResponse(const std::string &Target, bool IsPermanent);
		virtual ~RedirectResponse() { }

		virtual unsigned int GetExtraHeaderCount() { return 1; }
		virtual bool GetExtraHeader(unsigned int Index,
			const char **OutHeader, const char **OutHeaderEnd,
			const char **OutHeaderVal, const char **OutHeaderValEnd);
		virtual unsigned int GetResponseCode() { return RespCode; }
		virtual const char *GetContentType() const { return "text/plain"; }
		virtual const char *GetContentTypeCharset() const { return NULL; }
		virtual unsigned long long GetLength() { return ~(unsigned long long)0; }
		virtual bool Read(unsigned char *TargetBuff, unsigned int MaxLength, unsigned int &OutLength, boost::asio::yield_context &Ctx)
		{
			OutLength=0;
			return false;
		}

	private:
		unsigned int RespCode;
		const std::string &TargetLocation;
	};

	virtual void SetServerLog(IServerLog *NewLog);

	virtual IResponse *Create(METHOD Method, const std::string &Resource, const QueryParams &Query, const std::vector<Header> &HeaderA,
		const unsigned char *ContentBuff, const unsigned char *ContentBuffEnd,
		AsyncHelperHolder AsyncHelpers, void *ParentConn);

	void AddSimpleRewrite(const std::string &Resource, const std::string &Target) { RewMap[Resource]=RewHolder(Target); }
	void AddRedirect(const std::string &Resource, const std::string &Target, bool IsPermanent=true) { RewMap[Resource]=RewHolder(Target,IsPermanent); }
	void AddRespSource(const std::string &Prefix, IRespSource *RespSource) { HolderA.emplace_back(RSHolder(Prefix,RespSource)); }

protected:
	struct RSHolder
	{
		inline RSHolder() : RespSource(nullptr) { }
		inline RSHolder(RSHolder &&Src) : Prefix(std::move(Src.Prefix)), RespSource(std::move(Src.RespSource)) { }
		inline RSHolder(const std::string &NewPrefix, IRespSource *NewRespSource) : Prefix(NewPrefix), RespSource(NewRespSource)
		{ }

		std::string Prefix;
		std::unique_ptr<IRespSource> RespSource;

		bool operator()(const std::string &Resource) const;
	};

	struct RewHolder
	{
		inline RewHolder() { }
		inline RewHolder(const std::string &NewTarget) : Target(NewTarget), IsRedirect(false) { }
		inline RewHolder(const std::string &NewTarget, bool IsPermRedirect) : Target(NewTarget), IsRedirect(true), IsPermanentRedirect(IsPermRedirect) { }

		std::string Target;
		bool IsRedirect, IsPermanentRedirect;
	};

	std::vector<RSHolder> HolderA;
	boost::unordered_map<std::string,RewHolder> RewMap;
};

}; //RespSource

}; //HTTP
