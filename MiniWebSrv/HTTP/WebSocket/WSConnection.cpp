#include "WSConnection.h"

#include <boost/bind.hpp>

#include "../Common/BinUtils.h"

#include "IMsgHandler.h"

using namespace HTTP::WebSocket;

Connection::Connection(boost::asio::ip::tcp::socket &&SrcSocket, IMsgHandler *MsgHandler) : HTTP::ConnectionBase(std::move(SrcSocket)),
	SafeStates(SAFE_ALL),
	SilentTime(0), CurrFrameLength(UnknownFrameLength), FragOpCode(OCN_CONTINUATION),
	MyHandler(MsgHandler)
{
	//Start reading for incoming messages.
	ClearSafeState<SAFE_READ>();
	StartAsyncRead();
}

void Connection::Stop()
{
	try { MySock.close(); }
	catch (...) { }

	if (MyHandler)
	{
		MyHandler->OnClose(CR_CONN_ERROR);
		MyHandler=nullptr;
	}
}

bool Connection::OnStep(unsigned int StepInterval, ConnectionBase **OutNextConn)
{
	SilentTime+=StepInterval;
	if (SilentTime>Config::MaxSilentTime)
	{
		if (MyHandler)
		{
			MyHandler->OnClose(CR_CONN_ERROR);
			MyHandler=nullptr;
		}

		try { MySock.close(); }
		catch (...) { }
		return SafeStates!=SAFE_ALL;
	}
	else
		return true;
}

unsigned char *Connection::Allocate(MESSAGETYPE Type, unsigned long long Length)
{
	//SocketMtx is locked externally.

	//Create the frame header.
	unsigned long long FrameLength=Length;
	if (Length<126)
		FrameLength+=2;
	else if (Length<655356)
		FrameLength+=2+2;
	else
		FrameLength+=2+8;

	if (unsigned char *FrameBuff=WriteBuff.Allocate((unsigned int)FrameLength))
	{
		//Set the opcode and the FIN flag (we don't send fragmented messsages).
		*FrameBuff++=(unsigned char)(GetOpcode(Type) | FLAG_FIN);

		//Set the length.
		if (Length<126)
			*FrameBuff++=(unsigned char)Length;
		else if (Length<655356)
		{
			*FrameBuff++=126;
			SET_BIGENDIAN_UNALIGNED2(FrameBuff,(unsigned short)Length);
			FrameBuff+=2;
		}
		else
		{
			*FrameBuff++=127;
			SET_BIGENDIAN_UNALIGNED8(FrameBuff,Length);
			FrameBuff+=8;
		}

		return FrameBuff;
	}
	else
		return nullptr;
}

bool Connection::Deallocate()
{
	//SocketMtx is locked externally.
	WriteBuff.Commit(0);
	return true;
}

bool Connection::Send()
{
	//SocketMtx is locked externally.
	WriteBuff.Commit(~0);
	PostWriteReq();
	return true;
}

bool Connection::SendPing()
{
	//SocketMtx is locked externally.
	SendControlFrame(OCN_PING);
	PostWriteReq();
	return true;
}

void Connection::Close(unsigned short Reason)
{
	//SocketMtx is locked externally.
	SendCloseInternal(Reason);
	PostCloseReq();
}

void Connection::OnRead(const boost::system::error_code &error, std::size_t bytes_transferred)
{
	if (!error)
	{
		ReadBuff.OnNewData(bytes_transferred);
		SilentTime=0;
		CLOSEREASON Reason=ProcessIncoming();
		if (Reason==CR_NONE)
			StartAsyncRead();
		else
		{
			OnProtocolError(Reason);
			SetSafeState<SAFE_READ>();
		}
	}
	else
	{
		SetSafeState<SAFE_READ>();
		if (MyHandler)
		{
			MyHandler->OnClose(CR_CONN_ERROR);
			MyHandler=nullptr;
		}
	}
}

void Connection::OnWrite(const boost::system::error_code &error, std::size_t bytes_transferred)
{
	SetSafeState<SAFE_WRITE>();
	if (!error)
	{
		boost::unique_lock<boost::mutex> lock(SendBuffMtx);

		WriteBuff.Release();
		StartAsyncWrite();
	}
	else
	{
		if (MyHandler)
		{
			MyHandler->OnClose(CR_CONN_ERROR);
			MyHandler=nullptr;
		}
	}
}

CLOSEREASON Connection::ProcessIncoming()
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
				return CR_NONE;
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
					return CR_NONE;
				}

				CurrFrameLength=GET_BIGENDIAN_UNALIGNED2(DataBuff+2) + 2 + 2;
			}
			else //if (LengthMarker==127)
			{
				//Length field is 64 bit wide.
				if (DataLength<2+8)
				{
					ReadBuff.RequestData(2+8);
					return CR_NONE;
				}

				CurrFrameLength=GET_BIGENDIAN_UNALIGNED8(DataBuff+2) + 2 + 8;
			}

			if (DataBuff[1] & FLAG_MASK)
				CurrFrameLength+=4; //Include the length of the mask byte, too.
		}

		if (CurrFrameLength>Config::MaxFrameSize)
			return CR_SIZE_LIMIT;

		//At this point, we have determined the full frame length, or exited the function.
		if (DataLength<CurrFrameLength)
		{
			ReadBuff.RequestData((unsigned int)CurrFrameLength);
			return CR_NONE;
		}

		CLOSEREASON RetVal=ProcessFrame(DataBuff,(unsigned int)CurrFrameLength);
		ReadBuff.Consume((unsigned int)CurrFrameLength);

		return RetVal;
	}
}

CLOSEREASON Connection::ProcessFrame(const unsigned char *Buff, unsigned int Length)
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
			//MaskKey=GET_BIGENDIAN_UNALIGNED4(PayloadPos);
			MaskKey=*(const unsigned int *)PayloadPos;
			PayloadPos+=4;
		}
	}

	Length-=PayloadPos-Buff;

	if (IsMask)
	{
		//Unmask the received data in-place.
		unsigned char *ProcessPos=(unsigned char *)PayloadPos, *EndPos=ProcessPos+Length;
		if (Length>=4)
		{
			while (true)
			{
				unsigned char *NextProcPos=ProcessPos+4;
				if (NextProcPos<EndPos)
				{
					*(unsigned int *)ProcessPos=*(unsigned int *)ProcessPos ^ MaskKey;
					ProcessPos=NextProcPos;
				}
				else
					break;
			}
		}

		while (ProcessPos!=EndPos)
		{
			*ProcessPos++=*ProcessPos ^ (MaskKey & 0xFF);
			MaskKey>>=8;
		}
	}
	else if (!AllowMaskedOnly)
		return CR_PROT_ERROR;

	if (IsFin)
	{
		if (OpCode<OCN_CONTROL_BEGIN)
		{
			if ((FragOpCode!=OCN_CONTINUATION) && (OpCode==OCN_CONTINUATION))
			{
				if (FragMsgData.length()+Length>Config::MaxFragmentedSize)
					return CR_SIZE_LIMIT;

				//This is the last frame of the fragmented message we're receiving.
				FragMsgData.append(PayloadPos,PayloadPos+Length);
				MyHandler->OnMessage(GetMessageType(FragOpCode),(const unsigned char *)FragMsgData.data(),FragMsgData.length());

				FragMsgData.reserve(Config::ReadBuffSize);
				FragMsgData.clear();
				FragOpCode=OCN_CONTINUATION;
			}
			else if ((FragOpCode==OCN_CONTINUATION) && (OpCode!=OCN_CONTINUATION))
				//This is a single frame, with no fragmented data present.
				MyHandler->OnMessage(GetMessageType(OpCode),PayloadPos,Length);
			else
				//Every other combination is invalid.
				return CR_PROT_ERROR;
		}
		else
			//This is a control frame, which is always allowed.
			return ProcessControlFrame(OpCode,PayloadPos,Length);
	}
	else if (OpCode<OCN_CONTROL_BEGIN)
	{
		if ((FragOpCode!=OCN_CONTINUATION) && (OpCode==OCN_CONTINUATION))
		{
			//This is an intermediate frame of the fragmented message we're receiving.
			if (FragMsgData.length()+Length>Config::MaxFragmentedSize)
				return CR_SIZE_LIMIT;

			FragMsgData.append(PayloadPos,PayloadPos+Length);
		}
		else if ((FragOpCode==OCN_CONTINUATION) && (OpCode!=OCN_CONTINUATION))
		{
			//This is the first frame of a fragmented message.
			FragMsgData.assign(PayloadPos,PayloadPos+Length);
			FragOpCode=OpCode;
		}
		else
			//Every other combination is invalid.
			return CR_PROT_ERROR;
	}
	else
		//A control frame with fragmentation is not allowed.
		return CR_PROT_ERROR;

	return CR_NONE;
}

CLOSEREASON Connection::ProcessControlFrame(OPCODENAME OpCode, const unsigned char *PayloadBuff, unsigned int Length)
{
	switch (OpCode)
	{
	case OCN_PING:
		{
			boost::unique_lock<boost::mutex> lock(SendBuffMtx);
			if (SendControlFrame(OCN_PONG))
				StartAsyncWrite();
		}
		break;
	case OCN_CLOSE:
		if (MyHandler)
		{
			unsigned short ReasonCode;
			if (Length>=2)
				ReasonCode=GET_BIGENDIAN_UNALIGNED2(PayloadBuff);
			else
				ReasonCode=0;

			MyHandler->OnClose(ReasonCode);

			boost::unique_lock<boost::mutex> lock(SendBuffMtx);
			if (SendCloseInternal(ReasonCode))
				StartAsyncWrite();

			MyHandler=nullptr;
		}
		break;
	default:
		//Unknown control frame.
		return CR_PROT_ERROR;
	}

	return CR_NONE;
}

void Connection::OnProtocolError(CLOSEREASON Reason)
{
	if (MyHandler)
	{
		MyHandler->OnClose(Reason);

		boost::unique_lock<boost::mutex> lock(SendBuffMtx);
		if (SendCloseInternal(Reason))
			StartAsyncWrite();

		MyHandler=nullptr;
	}
}

bool Connection::SendControlFrame(OPCODENAME OpCode)
{
	if (unsigned char *Buff=WriteBuff.Allocate(2))
	{
		*Buff=(unsigned char)(FLAG_FIN | OpCode);
		Buff[1]=0; //Emtpy payload.
		WriteBuff.Commit(2);

		return true;
	}
	else
		return false;
}

bool Connection::SendCloseInternal(unsigned short Reason)
{
	if (unsigned char *Buff=WriteBuff.Allocate(4))
	{
		*Buff=(unsigned char)(FLAG_FIN | OCN_CLOSE);
		Buff[1]=2;
		SET_BIGENDIAN_UNALIGNED2(Buff+2,Reason);
		WriteBuff.Commit(4);

		return true;
	}
	else
		return false;
}

void Connection::StartAsyncWriteExternal()
{
	boost::unique_lock<boost::mutex> lock(SendBuffMtx);

	StartAsyncWrite();
}

void Connection::StartAsyncWriteCloseExternal()
{
	boost::unique_lock<boost::mutex> lock(SendBuffMtx);

	MyHandler=nullptr;
	StartAsyncWrite();
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
	if (!IsSafeState<SAFE_WRITE>())
		return;

	unsigned int WriteLength;
	while (const unsigned char *WritePos=WriteBuff.Pop(WriteLength))
	{
		ClearSafeState<SAFE_WRITE>();
		boost::asio::async_write(MySock,boost::asio::buffer(WritePos,WriteLength),
			boost::bind(&Connection::OnWrite,this,boost::asio::placeholders::error,boost::asio::placeholders::bytes_transferred));
	}
}


void Connection::PostWriteReq()
{
	MySock.get_io_service().post(boost::bind(&Connection::StartAsyncWriteExternal,this));
}

void Connection::PostCloseReq()
{
	MySock.get_io_service().post(boost::bind(&Connection::StartAsyncWriteCloseExternal,this));
}
