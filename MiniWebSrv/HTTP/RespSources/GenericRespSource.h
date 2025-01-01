#pragma once

#include <string_view>

#include "../IRespSource.h"

namespace HTTP
{

namespace RespSource
{

class GenericBase : public HTTP::IRespSource
{
public:

	struct CallParams
	{
		inline CallParams(METHOD Method, const std::string &Resource, const QueryParams &Query, const std::vector<Header> &HeaderA,
			const unsigned char *ContentBuff, const unsigned char *ContentBuffEnd,
			AsyncHelperHolder AsyncHelpers, void *ParentConn) :
			Method(Method), Resource(Resource), Query(Query), HeaderA(HeaderA), Content((const char *)ContentBuff, ContentBuffEnd-ContentBuff),
			AsyncHelpers(AsyncHelpers), ParentConn(ParentConn)
		{ }

		METHOD Method;
		const std::string &Resource;
		const QueryParams &Query;
		const std::vector<Header> &HeaderA;

		std::string_view Content;

		AsyncHelperHolder AsyncHelpers;
		void *ParentConn;
	};

};

template<class Callable>
class Generic : public GenericBase
{
public:
	/**@param target Signature: IResponse *target(const GenericBase::CallParams &Call) .*/
	Generic(Callable &&target) : target(std::forward<Callable>(target))
	{ }

	virtual void SetServerLog(IServerLog *NewLog)  override { }

	virtual IResponse *Create(METHOD Method, const std::string &Resource, const QueryParams &Query, const std::vector<Header> &HeaderA,
		const unsigned char *ContentBuff, const unsigned char *ContentBuffEnd,
		AsyncHelperHolder AsyncHelpers, void *ParentConn) override
	{
		return target(CallParams(Method, Resource, Query, HeaderA, ContentBuff, ContentBuffEnd, AsyncHelpers, ParentConn));
	}

private:
	Callable target;
};

template<class Callable>
Generic<Callable> *make_generic(Callable &&target)
{
	return new Generic<Callable>(std::forward<Callable>(target));
}

} //RespSource

} //HTTP
