#pragma once

#include <memory>

#include <boost/bind/bind.hpp>
#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>

namespace HTTP
{

class CoroAsyncHelper
{
public:
	inline CoroAsyncHelper() : MyStrand(nullptr), MyCtx(nullptr){ }
	CoroAsyncHelper(boost::asio::strand<boost::asio::io_context::executor_type> *NewStrand, boost::asio::yield_context *Ctx) :
		MyStrand(NewStrand), MyCtx(Ctx)
	{
		CoroCallee=MyCtx->coro_.lock();
	}

	inline operator char() { return MyCtx!=nullptr; }

	inline void Wake() { MyStrand->post(boost::bind(&CoroAsyncHelper::WakeInternal,CoroCallee), boost::asio::get_associated_allocator(MyStrand)); }
	inline void Wait() { MyCtx->ca_(); }

private:
	boost::asio::strand<boost::asio::io_context::executor_type> *MyStrand;
	boost::asio::yield_context *MyCtx;

	std::shared_ptr<boost::asio::yield_context::callee_type> CoroCallee;

	static void WakeInternal(std::shared_ptr<boost::asio::yield_context::callee_type> CC) { (*CC)(); }
};

};
