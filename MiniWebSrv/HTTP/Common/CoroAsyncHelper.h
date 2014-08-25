#pragma once

#include <memory>

#include <boost/bind.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/spawn.hpp>

class CoroAsyncHelper
{
public:
	inline CoroAsyncHelper() : MyStrand(nullptr), MyCtx(nullptr){ }
	CoroAsyncHelper(boost::asio::strand *NewStrand, boost::asio::yield_context *Ctx) :
		MyStrand(NewStrand), MyCtx(Ctx)
	{
		CoroCallee=MyCtx->coro_.lock();
	}

	inline operator char() { return MyCtx!=nullptr; }

	inline void Wake() { MyStrand->post(boost::bind(&CoroAsyncHelper::WakeInternal,CoroCallee)); }
	inline void Wait() { MyCtx->ca_(); }

private:
	boost::asio::strand *MyStrand;
	boost::asio::yield_context *MyCtx;

	std::shared_ptr<boost::asio::yield_context::callee_type> CoroCallee;

	static void WakeInternal(std::shared_ptr<boost::asio::yield_context::callee_type> CC) { (*CC)(); }
};