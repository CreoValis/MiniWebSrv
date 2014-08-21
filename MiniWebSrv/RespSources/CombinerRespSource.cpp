#include "CombinerRespSource.h"

#include "CommonErrorRespSource.h"

namespace HTTP
{

namespace RespSource
{

bool Combiner::RSHolder::operator()(const std::string &Resource) const
{
	if (Resource.length()>=Prefix.length())
		return Resource.find(Prefix)==0;
	else
		return false;
}

IResponse *Combiner::Create(METHOD Method, const std::string &Resource, const QueryParams &Query, const std::vector<Header> &HeaderA,
	const unsigned char *ContentBuff, const unsigned char *ContentBuffEnd,
	boost::asio::io_service &ParentIOS)
{
	const std::string *ResPtr=&Resource;
	{
		boost::unordered_map<std::string,std::string>::const_iterator FindI=RewriteMap.find(*ResPtr);
		if (FindI!=RewriteMap.end())
			ResPtr=&(FindI->second);
	}

	for (const RSHolder &CurrHolder : HolderA)
	{
		if (CurrHolder(*ResPtr))
			return CurrHolder.RespSource->Create(Method,ResPtr->substr(CurrHolder.Prefix.length()),Query,HeaderA,ContentBuff,ContentBuffEnd,ParentIOS);
	}

	return new CommonError::Response(*ResPtr,HeaderA,NULL,RC_NOTFOUND);
}

}; //RespSource

}; //HTTP
