#include "WSConnection.h"

#include <boost/bind.hpp>

using namespace HTTP::WebSocket;

Connection::Connection(boost::asio::ip::tcp::socket &&SrcSocket) : HTTP::ConnectionBase(std::move(SrcSocket)),
	SafeStates(SAFE_ALL),
	SilentTime(0)
{
	//Start reading for incoming messages.
	ClearSafeState<SAFE_READ>();
	StartAsyncRead();
}

void Connection::Stop()
{
	try { MySock.close(); }
	catch (...) { }
}

bool Connection::OnStep(unsigned int StepInterval, ConnectionBase **OutNextConn)
{
	SilentTime+=StepInterval;
	if (SilentTime>Config::MaxSilentTime)
	{
		try { MySock.close(); }
		catch (...) { }
		return SafeStates!=SAFE_ALL;
	}
	else
		return true;
}

void Connection::OnRead(const boost::system::error_code &error, std::size_t bytes_transferred)
{
	if (!error)
	{
		ReadBuff.OnNewData(bytes_transferred);
		SilentTime=0;
	}
	else
	{
		SetSafeState<SAFE_READ>();
		//TODO: handle read error!!!
	}
}

void Connection::OnWrite(const boost::system::error_code &error, std::size_t bytes_transferred)
{
	SetSafeState<SAFE_WRITE>();
	if (!error)
	{
		WriteBuff.Release();
		StartAsyncWrite();
	}
	else
	{
		//TODO: handle write error!!!
	}
}

void Connection::StartAsyncRead()
{
	unsigned int FreeLength;
	unsigned char *ReadPos=ReadBuff.GetReadInfo(FreeLength);

	MySock.async_read_some(boost::asio::buffer(ReadPos,FreeLength),
		boost::bind(&Connection::OnRead,this,boost::asio::placeholders::error,boost::asio::placeholders::bytes_transferred));
}

void Connection::StartAsyncWrite()
{
	unsigned int WriteLength;
	while (const unsigned char *WritePos=WriteBuff.Pop(WriteLength))
	{
		ClearSafeState<SAFE_WRITE>();
		boost::asio::async_write(MySock,boost::asio::buffer(WritePos,WriteLength),
			boost::bind(&Connection::OnWrite,this,boost::asio::placeholders::error,boost::asio::placeholders::bytes_transferred));
	}
}
