#include "WSConnection.h"

#include <boost/bind.hpp>

#include "../Common/BinUtils.h"

using namespace HTTP::WebSocket;

Connection::Connection(boost::asio::ip::tcp::socket &&SrcSocket) : HTTP::ConnectionBase(std::move(SrcSocket)),
	SafeStates(SAFE_ALL),
	SilentTime(0), CurrFrameLength(UnknownFrameLength)
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
		ProcessIncoming();
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

void Connection::ProcessIncoming()
{
	while (true)
	{
		unsigned int DataLength;
		const unsigned char *DataBuff=ReadBuff.GetAvailableData(DataLength);
		if (CurrFrameLength==UnknownFrameLength)
		{
			//Try to determine the length of the frame. At least 2 bytes is a must.
			if (DataLength<2)
			{
				ReadBuff.RequestData(2);
				return;
			}

			unsigned char LengthMarker=DataBuff[1] & 0x7F;
			if (LengthMarker<126)
				CurrFrameLength=LengthMarker + 2;
			else if (LengthMarker==126)
			{
				//Length field is 16 bit wide.
				if (DataLength<2+2)
				{
					ReadBuff.RequestData(2+2);
					return;
				}

				CurrFrameLength=GET_BIGENDIAN_UNALIGNED2(DataBuff+2) + 2 + 2;
			}
			else //if (LengthMarker==127)
			{
				//Length field is 64 bit wide.
				if (DataLength<2+8)
				{
					ReadBuff.RequestData(2+8);
					return;
				}

				CurrFrameLength=GET_BIGENDIAN_UNALIGNED8(DataBuff+2) + 2 + 8;
			}

			if (DataBuff[1] & FLAG_MASK)
				CurrFrameLength+=4; //Include the length of the mask byte, too.
		}

		//At this point, we have determined the full frame length, or exited the function.
		if (DataLength<CurrFrameLength)
		{
			ReadBuff.RequestData((unsigned int)CurrFrameLength);
			return;
		}

		ProcessFrame(DataBuff,CurrFrameLength);

		ReadBuff.Consume(CurrFrameLength);
	}
}

void Connection::ProcessFrame(const unsigned char *Buff, unsigned int Length)
{
	bool IsFin=(*Buff & FLAG_FIN)!=0, IsMask=(Buff[1] & FLAG_MASK)!=0;
	OPCODENAME OpCode=(OPCODENAME)(*Buff & OCN_MASK);

	unsigned int MaskKey;
	const unsigned char *PayloadPos;
	{
		unsigned char LengthMarker=Buff[1] & 0x7F;
		if (LengthMarker<126)
			PayloadPos=Buff+2;
		else if (LengthMarker==126)
			PayloadPos=Buff+2+2;
		else //if (LengthMarker==127)
			PayloadPos=Buff+2+8;

		if (IsMask)
		{
			MaskKey=GET_BIGENDIAN_UNALIGNED4(PayloadPos);
			PayloadPos+=4;
		}
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
