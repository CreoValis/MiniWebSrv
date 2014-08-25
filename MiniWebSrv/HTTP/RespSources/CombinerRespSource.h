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

	virtual IResponse *Create(METHOD Method, const std::string &Resource, const QueryParams &Query, const std::vector<Header> &HeaderA,
		const unsigned char *ContentBuff, const unsigned char *ContentBuffEnd,
		boost::asio::io_service &ParentIOS);

	void AddSimpleRewrite(const std::string &Resource, const std::string &Target) { RewriteMap[Resource]=Target; }
	void AddRespSource(const std::string &Prefix, IRespSource *RespSource) { HolderA.push_back(RSHolder(Prefix,RespSource)); }

protected:
	struct RSHolder
	{
		inline RSHolder() { }
		inline RSHolder(const std::string &NewPrefix, IRespSource *NewRespSource) : Prefix(NewPrefix), RespSource(NewRespSource)
		{ }

		std::string Prefix;
		std::auto_ptr<IRespSource> RespSource;

		bool operator()(const std::string &Resource) const;
	};

	std::vector<RSHolder> HolderA;
	boost::unordered_map<std::string,std::string> RewriteMap;
};

}; //RespSource

}; //HTTP
