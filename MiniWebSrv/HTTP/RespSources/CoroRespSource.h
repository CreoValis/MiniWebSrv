#pragma once

#include <string_view>

#include <boost/context/continuation.hpp>

#include "SimpleResponse.h"
#include "GenericRespSource.h"

namespace HTTP
{

namespace RespSource
{

/**Control flow:
- Ctor is called with lamda, which should set up the ResponseLength, ResponseCode, ContentType, Headers on the passed
  ResponseParams object.
- After everything is set up, it should call ResponseParams::Finalize() . This will suspend the coroutine the lambda
  runs in, returning control to the Connection object's coroutine.
- The response headers are sent to the client.
- (A) The response coroutine is entered again. This time, it should use the OutStream object to write data to the
  output. The object contains the output buffer, which should be filled with the response data. The number of bytes
  that should be sent has to be specified in the Write() call.
- The Write() method suspends the response coroutine, returning control to the Connection object's coroutine.
- The newly written response data is sent to the client.
- Execution continues at point "A". Writing 0 bytes to the stream, or exting the coroutine will stop the finish the
  response.

In the response coroutine, the asio::yield_context returned by Stream.GetContext() can be used to carry out other
async IO. Specifying this as a contination token in asio async functions, the HTTP connection's coroutine, and with it,
the response coroutine will be suspended until the async operation finishes.

Simple example:

new CoroResponse([](const GenericBase::CallParams &CParams, ResponseParams &RParams, OutStream &Stream) {
	RParams.SetResponseLength(6);
	RParams.Finalize(Stream);

	memcpy(Stream.GetBuff(), "abc", 3);
	Stream.Write(3);

	memcpy(Stream.GetBuff(), "def", 3);
	Stream.Write(3);
});*/
class CoroResponse : public SimpleResponse
{
public:
	class OutStream
	{
	public:
		inline unsigned char *GetBuff() { return TargetBuff; }
		inline unsigned int GetBuffLength() const { return MaxLength; }
		boost::asio::yield_context &GetContext() { return *Ctx; }

		inline void Write(unsigned int Length)
		{
			PendingLength=Length;
			Cont=Cont.resume();
		}

	protected:
		unsigned char *TargetBuff = nullptr;
		unsigned int MaxLength = 0, PendingLength = 0;

		boost::context::continuation Cont;
		boost::asio::yield_context *Ctx = nullptr;
	};

	class ResponseParams
	{
	public:
		inline ResponseParams(CoroResponse &Target) : Target(Target)
		{ }

		inline void SetResponseCode(unsigned int ResponseCode) { Target.ResponseCode=ResponseCode; }
		inline void SetContentType(std::string_view CT) { Target.ContentType=CT; }
		inline std::vector<std::tuple<std::string, std::string>> &GetResponseHeaders() { return Target.Headers; }

		inline void SetResponseLength(unsigned long long Length) { Target.ResponseLength=Length; }
		inline void SetUnknownResponseLength() { Target.ResponseLength=~(unsigned long long)0; }

		inline void Finalize(OutStream &Target) { ((OutStreamImpl &)Target).SetCont(Cont.resume()); }

	protected:
		CoroResponse &Target;
		boost::context::continuation Cont;
	};

	/**@param Target Callable with the signature:
		void Target(const GenericBase::CallParams &, ResponseParams &, OutStream &)*/
	template<class Callable>
	CoroResponse(Callable &&Target, const GenericBase::CallParams &ReqParams) : ResponseLength(~(unsigned long long)0), RespParamHelper(*this)
	{
		RespGen=CreateCoroResponse(std::forward<Callable>(Target), ReqParams);
	}

	virtual unsigned long long GetLength() override { return ResponseLength; }
	virtual bool Read(unsigned char *TargetBuff, unsigned int MaxLength, unsigned int &OutLength,
		boost::asio::yield_context &Ctx) override
	{
		StreamHelper.SetBuffer(TargetBuff, MaxLength);
		RespGen=RespGen.resume();
		return (OutLength=StreamHelper.GetPendingLength())==0;
	}

protected:
	unsigned long long ResponseLength;

private:
	class OutStreamImpl : public OutStream
	{
	public:
		inline OutStreamImpl()
		{ }

		inline void SetCont(boost::context::continuation &&Cont) { this->Cont = std::move(Cont); }
		inline boost::context::continuation &&GetCont() { return std::move(Cont); }
		inline void SetBuffer(unsigned char *TargetBuff, unsigned int MaxLength)
		{
			this->TargetBuff = TargetBuff;
			this->MaxLength = MaxLength;
			this->PendingLength = 0;
		}
		inline unsigned int GetPendingLength() const { return PendingLength; }

		inline void SetContext(boost::asio::yield_context &Ctx) { this->Ctx = &Ctx; }
	};

	class ResponseParamsImpl : public ResponseParams
	{
	public:
		inline void SetCont(boost::context::continuation &&Cont) { this->Cont=std::move(Cont); }
	};

	ResponseParams RespParamHelper;
	OutStreamImpl StreamHelper;
	boost::context::continuation RespGen;

	template<class Callable>
	boost::context::continuation CreateCoroResponse(Callable Target, const GenericBase::CallParams &ReqParams)
	{
		return boost::context::callcc([&ReqParams, &Target, this](boost::context::continuation &&Sink) {
			StreamHelper.SetContext(ReqParams.AsyncHelpers.Ctx);
			((ResponseParamsImpl &)RespParamHelper).SetCont(std::move(Sink));
			Target(ReqParams, RespParamHelper, StreamHelper);
			return StreamHelper.GetCont();
		});
		//At this point, RespGen has called RespParamHelper.Finalize(), and stored the internal continuation in StreamHelper.
	}
};


/**Creates a ResponseSource which uses a coroutine-based response.
@param RespGen Callable with (const GenericBase::CallParams &CParams, CoroResponse::ResponseParams &RParams, CoroResponse::OutStream &OutS) .*/
template<class Callable>
auto make_coro_respsource(Callable &&RespGen)
{
	return HTTP::RespSource::make_generic([&](const HTTP::RespSource::GenericBase::CallParams &CallParams) {
		using namespace HTTP::RespSource;
		return new CoroResponse(std::forward<Callable>(RespGen), CallParams);
	});
}

} //RespSource

} //HTTP
