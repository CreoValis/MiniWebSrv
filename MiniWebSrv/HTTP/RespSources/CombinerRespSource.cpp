#include "CombinerRespSource.h"

#include "CommonErrorRespSource.h"

namespace HTTP
{

namespace RespSource
{

Combiner::RedirectResponse::RedirectResponse(const std::string &Target, bool IsPermanent) :
	RespCode(IsPermanent ? RC_MOVEPERMANENT : RC_TEMP_REDIRECT), TargetLocation(Target)
{
}

bool Combiner::RedirectResponse::GetExtraHeader(unsigned int Index,
	const char **OutHeader, const char **OutHeaderEnd,
	const char **OutHeaderVal, const char **OutHeaderValEnd)
{
	if (Index==0)
	{
		const std::string &HName=Header::GetHeaderName(HN_LOCATION);
		*OutHeader=HName.data();
		*OutHeaderEnd=HName.data() + HName.size();
		*OutHeaderVal=TargetLocation.data();
		*OutHeaderValEnd=TargetLocation.data()+TargetLocation.length();
		return true;
	}
	else
		return false;
}

bool Combiner::RSHolder::operator()(const std::string &Resource) const
{
	if ((!ExactMatchOnly) && (Resource.length() > Prefix.length()))
		return (Resource.find(Prefix) == 0) && (Resource[Prefix.length()] == '/');
	else if (Resource.length() == Prefix.length())
		return Resource == Prefix;
	else
		return false;
}

void Combiner::SetServerLog(IServerLog *NewLog)
{
	for (RSHolder &CurrHolder : HolderA)
		CurrHolder.RespSource->SetServerLog(NewLog);
}

IResponse *Combiner::Create(METHOD Method, const std::string &Resource, const QueryParams &Query, const std::vector<Header> &HeaderA,
	const unsigned char *ContentBuff, const unsigned char *ContentBuffEnd,
	AsyncHelperHolder AsyncHelpers, void *ParentConn)
{
	const std::string *ResPtr=&Resource;
	{
		std::unordered_map<std::string,RewHolder>::const_iterator FindI=RewMap.find(*ResPtr);
		if (FindI!=RewMap.end())
		{
			if (FindI->second.IsRedirect)
				return new RedirectResponse(FindI->second.Target,FindI->second.IsPermanentRedirect);
			else
				ResPtr=&(FindI->second.Target);
		}
	}

	for (const RSHolder &CurrHolder : HolderA)
	{
		if (CurrHolder(*ResPtr))
			return CurrHolder.RespSource->Create(Method,ResPtr->substr(CurrHolder.Prefix.length()),
			Query,HeaderA,ContentBuff,ContentBuffEnd,
			AsyncHelpers,ParentConn);
	}

	return new CommonError::Response(*ResPtr,HeaderA,NULL,RC_NOTFOUND);
}

}; //RespSource

}; //HTTP
