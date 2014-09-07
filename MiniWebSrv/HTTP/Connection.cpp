#include "Connection.h"

#include "IRespSource.h"

#include "RespSources/CommonErrorRespSource.h"

using namespace HTTP;

Connection::Connection(boost::asio::io_service &MyIOS, RespSource::CommonError *NewErrorRS) : ConnectionBase(MyIOS),
	MyStrand(MyIOS), SilentTime(0), IsDeletable(true),
	MyRespSource(nullptr), ServerName("MiniWebSrv"),
	ErrorRS(NewErrorRS),
	PostHeaderBuff(nullptr), PostHeaderBuffEnd(nullptr), PostHeaderBuffPos(nullptr),
	NextConn(nullptr)
{

}

Connection::~Connection()
{
	delete[] PostHeaderBuff;

	CurrQuery.DeleteUploadedFiles();

	delete NextConn;
}

void Connection::Start(IRespSource *NewRespSource)
{
	IsDeletable=false;

	MyRespSource=NewRespSource;
	boost::asio::spawn(MyStrand,boost::bind(&Connection::ProtocolHandler,this,_1));
}

void Connection::Stop()
{
	try { MySock.close(); }
	catch (...) { }
}

bool Connection::OnStep(unsigned int StepInterval, ConnectionBase **OutNextConn)
{
	if (NextConn)
	{
		//Release our "next" connection, and immediately time out.
		*OutNextConn=NextConn;
		NextConn=nullptr;
		SilentTime=Config::MaxSilentTime;
	}

	SilentTime+=StepInterval;
	if (SilentTime>Config::MaxSilentTime)
	{
		try { MySock.close(); }
		catch (...) { }
		return !IsDeletable;
	}
	else
		return true;
}

void Connection::ContinueRead(boost::asio::yield_context &Yield)
{
	unsigned int FreeLength;
	unsigned char *ReadPos=ReadBuff.GetReadInfo(FreeLength);

	std::size_t ReadCount=MySock.async_read_some(boost::asio::buffer(ReadPos,FreeLength),Yield);
	ReadBuff.OnNewData(ReadCount);
	SilentTime=0;
}

void Connection::WriteNext(boost::asio::yield_context &Yield)
{
	unsigned int WriteLength;
	if (const unsigned char *WritePos=WriteBuff.Pop(WriteLength))
	{
		boost::asio::async_write(MySock,boost::asio::buffer(WritePos,WriteLength),Yield);
		WriteBuff.Release();
		SilentTime=0;
	}
}

void Connection::WriteAll(boost::asio::yield_context &Yield)
{
	unsigned int WriteLength;
	while (const unsigned char *WritePos=WriteBuff.Pop(WriteLength))
	{
		boost::asio::async_write(MySock,boost::asio::buffer(WritePos,WriteLength),Yield);
		WriteBuff.Release();
		SilentTime=0;
	}
}

void Connection::ProtocolHandler(boost::asio::yield_context Yield)
{
	enum PARSESTATE
	{
		REQUEST_METHOD_END,
		REQUEST_RES_BEGIN,
		REQUEST_RES_END,
		REQUEST_QUERY_END,
		REQUEST_END,

		HEADER_END, //Searching for end of a header.
		CONTENT_END, //Searching for end of the content.
	} CurrState=REQUEST_METHOD_END;

	union
	{
		struct
		{
			unsigned int MethodPos, MethodEndPos, ResPos, ResEndPos, QueryPos, QueryEndPos, VerBeginPos;
			void Reset() { MethodPos=MethodEndPos=ResPos=ResEndPos=QueryPos=QueryEndPos=VerBeginPos=0; }
		} ReqHead;
		struct
		{
			unsigned int HeaderPos;
			void Reset() { HeaderPos=0; }
		} Header;
		struct
		{
			unsigned int TotalLength, RemLength;
			CONTENTTYPE Type;

			void Reset() { TotalLength=RemLength=~(unsigned int)0; Type=CT_NOTUSED; }
			inline bool IsValid() const { return (RemLength!=~(unsigned int)0) && (Type!=CT_NOTUSED); }
		} Content;
	} PState;

	PState.ReqHead.Reset();

	try
	{
		bool IsKeepAlive=true;
		while (IsKeepAlive)
		{
			ContinueRead(Yield);
			if (!IsKeepAlive)
				return;

			unsigned int RelevantLength;
			const unsigned char *RelevantBuff=ReadBuff.GetRelevantData(RelevantLength);

			unsigned int InBuffLen;
			const unsigned char *InBuff=ReadBuff.GetAvailableData(InBuffLen);

			const unsigned char *InBuffEnd=InBuff+InBuffLen;

			//Consume the available bytes.
			while (InBuff!=InBuffEnd)
			{
				unsigned char CurrVal=*InBuff;
				switch (CurrState)
				{
				case REQUEST_METHOD_END:
					if (CurrVal==' ')
					{
						PState.ReqHead.MethodEndPos=InBuff-RelevantBuff;
						CurrState=REQUEST_RES_BEGIN;
					}
					else if (CurrVal=='\n')
						throw std::runtime_error("Method end not found");
					break;
				case REQUEST_RES_BEGIN:
					if (CurrVal!=' ')
					{
						PState.ReqHead.ResPos=InBuff-RelevantBuff;
						CurrState=REQUEST_RES_END;
					}
					else if (CurrVal=='\n')
						throw std::runtime_error("Resource not found");
					break;
				case REQUEST_RES_END:
					if (CurrVal==' ')
					{
						PState.ReqHead.QueryPos=PState.ReqHead.QueryEndPos=PState.ReqHead.ResEndPos=InBuff-RelevantBuff;
						CurrState=REQUEST_END;
					}
					else if (CurrVal=='?')
					{
						PState.ReqHead.ResEndPos=InBuff-RelevantBuff;
						PState.ReqHead.QueryPos=PState.ReqHead.ResEndPos+1;
						CurrState=REQUEST_QUERY_END;
					}
					else if (CurrVal=='\n')
						throw std::runtime_error("Resource end not found");
					break;
				case REQUEST_QUERY_END:
					if (CurrVal!=' ')
					{
						PState.ReqHead.QueryEndPos=InBuff-RelevantBuff;
						CurrState=REQUEST_END;
					}
					else if (CurrVal=='\n')
						throw std::runtime_error("Query end not found");
					break;
				case REQUEST_END:
					if (CurrVal=='\n')
					{
						//We've got to the end of the request line. Parse it's parts.
						CurrMethod=ParseMethod(RelevantBuff + PState.ReqHead.MethodPos,RelevantBuff + PState.ReqHead.MethodEndPos);
						CurrResource.assign((const std::string::value_type *)(RelevantBuff + PState.ReqHead.ResPos),
							PState.ReqHead.ResEndPos-PState.ReqHead.ResPos);

						CurrQuery=QueryParams();
						CurrQuery.AddURLEncoded((const char *)(RelevantBuff + PState.ReqHead.QueryPos),(const char *)(RelevantBuff + PState.ReqHead.QueryEndPos));

						if (PState.ReqHead.VerBeginPos+3<=(unsigned int)(InBuff-RelevantBuff))
						{
							if (!memcmp(RelevantBuff + PState.ReqHead.VerBeginPos,"1.0",3))
								//Request is HTTP/1.0, no keepalive.
								IsKeepAlive=false;
						}
						else
							throw std::runtime_error("Protocol version not found or invalid");

						ReadBuff.Consume(InBuff-RelevantBuff + 1); //Consume this byte, too.
						ReadBuff.ResetRelevant();
						RelevantBuff=ReadBuff.GetRelevantData(RelevantLength);

						//When parsing methods with request body, store the headers in a separate buffer.
						if (CurrMethod==METHOD_POST)
							CreatePostHeaderBuff();

						CurrState=HEADER_END;
						PState.Header.Reset();
						HeaderA.reserve(HeaderA.size());
						HeaderA.clear();
					}
					else if (CurrVal=='/')
						PState.ReqHead.VerBeginPos=InBuff-RelevantBuff + 1;
					break;
				case HEADER_END:
					if (CurrVal=='\n')
					{
						unsigned int CurrPos=(unsigned int)(InBuff-RelevantBuff);

						if (CurrPos-PState.Header.HeaderPos>1)
						{
							if (CurrMethod!=METHOD_POST)
							{
								HeaderA.push_back(Header((char *)RelevantBuff+PState.Header.HeaderPos,
									(char *)RelevantBuff+CurrPos));

								ReadBuff.Consume(CurrPos-PState.Header.HeaderPos,true); //Consume the header, but keep as relevant.

								//Wait for the next header.
								PState.Header.HeaderPos=CurrPos + 1;
							}
							else
							{
								char *HeaderPtr=AddPostHeader(RelevantBuff+PState.Header.HeaderPos,
									RelevantBuff+CurrPos);

								if (HeaderPtr)
									HeaderA.push_back(Header(HeaderPtr,HeaderPtr+CurrPos));

								ReadBuff.Consume(InBuff-RelevantBuff + 1); //Consume this byte, too.
								ReadBuff.ResetRelevant();
								RelevantBuff=ReadBuff.GetRelevantData(RelevantLength);

								//Wait for the next header.
								PState.Header.Reset();
							}
						}
						else
						{
							//Parse the "Connection" header.
							for (const Header &CurrHeader : HeaderA)
								if (CurrHeader.IntName==HN_CONNECTION)
								{
									IsKeepAlive=IsKeepAlive && (!CurrHeader.IsConnectionClose());
									break;
								}

							/*We've found the end of the headers. We've can either construct the response now, and reset the
							parser state, or read the request body.*/
							if (CurrMethod!=METHOD_POST)
							{
								IsKeepAlive= ResponseHandler(Yield) && IsKeepAlive;

								//Reset the request data.
								ReadBuff.Consume(InBuff-RelevantBuff + 1); //Consume this byte, too.
								ReadBuff.ResetRelevant();
								RelevantBuff=ReadBuff.GetRelevantData(RelevantLength);

								//Reset the parser state.
								CurrQuery=QueryParams();
								HeaderA.clear();
								CurrState=REQUEST_METHOD_END;
								PState.ReqHead.Reset();
							}
							else
							{
								PState.Content.Reset();
								for (const Header &CurrHeader : HeaderA)
								{
									if (CurrHeader.IntName==HN_CONTENT_LENGTH)
										PState.Content.TotalLength=PState.Content.RemLength=(unsigned int)CurrHeader.GetULongLong();
									else if (CurrHeader.IntName==HN_CONTENT_TYPE)
									{
										PState.Content.Type=CurrHeader.GetContentType(CurrQuery.GetBoundaryStr());
										CurrQuery.OnBoundaryParsed();
									}

									if (PState.Content.IsValid())
										break;
								}

								if (!PState.Content.IsValid())
									throw std::runtime_error("Content-Length header not found");
								else if (PState.Content.RemLength>Config::MaxPostBodyLength)
									throw std::runtime_error("Content-Length value too large");

								ReadBuff.Consume(InBuff-RelevantBuff + 1); //Consume this byte, too.
								ReadBuff.ResetRelevant();
								RelevantBuff=ReadBuff.GetRelevantData(RelevantLength);

								CurrState=CONTENT_END;
							}
						}
					}
					break;
				case CONTENT_END:
					//Parse any content we received.
					switch (PState.Content.Type)
					{
					case CT_FORM_MULTIPART:
						if (PState.Content.RemLength>(unsigned int)(InBuffEnd-InBuff))
						{
							CurrQuery.AppendFormMultipart((char *)InBuff,(char *)InBuffEnd);

							PState.Content.RemLength-=InBuffEnd-InBuff;
							InBuff=InBuffEnd-1;
						}
						else
						{
							CurrQuery.AppendFormMultipart((char *)InBuff,(char *)InBuff+PState.Content.RemLength);

							InBuff+=PState.Content.RemLength-1;
							PState.Content.RemLength=0;
						}

						//We've fully consumed the incoming bytes.
						ReadBuff.ResetRelevant();
						RelevantBuff=ReadBuff.GetRelevantData(RelevantLength);
						break;
					case CT_URL_ENCODED:
						if (PState.Content.RemLength>(unsigned int)(InBuffEnd-InBuff))
						{
							PState.Content.RemLength-=InBuffEnd-InBuff;
							InBuff=InBuffEnd-1;
						}
						else
						{
							ReadBuff.Consume(PState.Content.RemLength,true);

							RelevantBuff=ReadBuff.GetRelevantData(RelevantLength);
							CurrQuery.AddURLEncoded((char *)RelevantBuff,(char *)RelevantBuff+RelevantLength);

							InBuff+=PState.Content.RemLength-1;
							PState.Content.RemLength=0;

							ReadBuff.ResetRelevant();
							RelevantBuff=ReadBuff.GetRelevantData(RelevantLength);
						}
						break;
					default:
						//Keep every byte from the request content.
						if (PState.Content.RemLength>(unsigned int)(InBuffEnd-InBuff))
						{
							PState.Content.RemLength-=InBuffEnd-InBuff;
							InBuff=InBuffEnd-1;
						}
						else
						{
							InBuff+=PState.Content.RemLength-1;
							PState.Content.RemLength=0;

							RelevantBuff=ReadBuff.GetRelevantData(RelevantLength);
							ContentBuff=RelevantBuff;
							ContentEndBuff=RelevantBuff+RelevantLength;

							ReadBuff.ResetRelevant();
							RelevantBuff=ReadBuff.GetRelevantData(RelevantLength);
						}
						break;
					}

					if (!PState.Content.RemLength)
					{
						//We've fully read this request. Process the response.
						IsKeepAlive= ResponseHandler(Yield) && IsKeepAlive;
						CurrQuery.DeleteUploadedFiles();
						PState.ReqHead.Reset();
						CurrState=REQUEST_METHOD_END;
					}

					break;
				} //END switch (CurrState)

				++InBuff;
			} //END while (InBuff!=InBuffEnd)

			//Every remaining byte in the buffer is relevant.
			ReadBuff.Consume(~(unsigned int)0,true);
			if (RelevantLength<Config::ReadBuffSize)
				ReadBuff.RequestData(Config::ReadBuffSize-RelevantLength);
			else
				ReadBuff.RequestData(Config::ReadBuffSize);
		}
	}
	catch (std::exception &Ex)
	{
		(void)Ex;
	}
	catch (...)
	{
	}

	try { MySock.close(); }
	catch (...) { }
	IsDeletable=true;
}

bool Connection::ResponseHandler(boost::asio::yield_context &Yield)
{
	ResponseCount++;

	IResponse *CurrResp;
	try
	{
		CurrResp=MyRespSource->Create(CurrMethod,CurrResource,CurrQuery,
			HeaderA,ContentBuff,ContentEndBuff,MySock.get_io_service());
	}
	catch (const std::exception &Ex)
	{
		(void)Ex;
		CurrResp=ErrorRS->CreateFromException(CurrMethod,CurrResource,CurrQuery,
			HeaderA,ContentBuff,ContentEndBuff,MySock.get_io_service(),&Ex);
	}
	catch (...)
	{
		CurrResp=ErrorRS->Create(CurrMethod,CurrResource,CurrQuery,
			HeaderA,ContentBuff,ContentEndBuff,MySock.get_io_service());
	}

	char *CurrPos=(char *)WriteBuff.Allocate(Config::MaxHeadersLength);
	char *CurrPosBegin=CurrPos;
	char *CurrPosEnd=CurrPos+Config::MaxHeadersLength - 2; //Leave room for the final "\r\n".

	{
		unsigned int RespCode=CurrResp->GetResponseCode();
		CurrPos+=sprintf(CurrPos,"HTTP/1.1 %d %s\r\nServer: %s\r\n",
			RespCode,GetResponseName((RESPONSECODE)RespCode),ServerName);
	}

	if (const char *EncodingStr=CurrResp->GetContentTypeCharset())
		CurrPos+=sprintf(CurrPos,"Content-Type: %s; charset=\"%s\"\r\n",CurrResp->GetContentType(),EncodingStr);
	else
		CurrPos+=sprintf(CurrPos,"Content-Type: %s\r\n",CurrResp->GetContentType());

	{
		//Append the current date.
		time_t CurrTime=time(NULL);
		tm TM;
		if (!gmtime_s(&TM,&CurrTime))
			CurrPos+=strftime(CurrPos,40,"Date: %a, %d %b %Y %H:%M:%S GMT\r\n",&TM);
	}

	unsigned long long RespLength=CurrResp->GetLength();
	if (RespLength!=~(unsigned long long)0)
		CurrPos+=sprintf(CurrPos,"Content-Length: %llu\r\n",RespLength);
	else
		//Response length not known: use chunked encoding.
		CurrPos+=sprintf(CurrPos,"Transfer-Encoding: chunked\r\n");

	for (unsigned int HeaderI=0, HeaderCount=CurrResp->GetExtraHeaderCount(); HeaderI!=HeaderCount; ++HeaderI)
	{
		const char *Name, *NameEnd, *Value, *ValueEnd;
		if (CurrResp->GetExtraHeader(HeaderI,&Name,&NameEnd,&Value,&ValueEnd))
		{
			unsigned int NameLen=(unsigned int)(NameEnd-Name), ValueLen=(unsigned int)(ValueEnd-Value);
			if (NameLen+ValueLen+2+2<(unsigned int)(CurrPosEnd-CurrPos))
			{
				memcpy(CurrPos,Name,NameLen); CurrPos+=NameLen;
				*CurrPos++=':'; *CurrPos++=' ';
				memcpy(CurrPos,Value,ValueLen); CurrPos+=ValueLen;
				*CurrPos++='\r'; *CurrPos++='\n';
			}
		}
	}

	//Close the headers, and send them to the client.
	CurrPos+=sprintf(CurrPos,"\r\n");
	WriteBuff.Commit((unsigned int)(CurrPos-CurrPosBegin));
	WriteNext(Yield);

	if (RespLength!=~(unsigned long long)0)
	{
		bool RetVal=false;
		while (RespLength)
		{
			SilentTime=0;
			CurrPos=(char *)WriteBuff.Allocate(Config::WriteBuffSize);

			unsigned int ReadLength;
			bool IsFinished=CurrResp->Read((unsigned char *)CurrPos,Config::WriteBuffSize,ReadLength,
				Yield);

			if (ReadLength<=RespLength)
				RespLength-=ReadLength;
			else
				RespLength=0;

			WriteBuff.Commit(ReadLength);
			WriteNext(Yield);

			if ((IsFinished) && (!RespLength))
			{
				RetVal=true;
				break;
			}
			else if ((IsFinished) || (!RespLength))
			{
				RetVal=false;
				break;
			}
		}

		WriteAll(Yield);

		NextConn=CurrResp->Upgrade(this);
		delete CurrResp;

		return RetVal && !NextConn;
	}
	else
	{
		static const unsigned int ChunkHeaderLen=8 + 2;
		static const unsigned int ChunkFooterLength=2;
		static const unsigned int FinalChunkLength=1 + 2 + 2;

		while (true)
		{
			SilentTime=0;
			CurrPos=(char *)WriteBuff.Allocate(Config::WriteBuffSize);

			unsigned int ReadLength;
			bool IsFinished=CurrResp->Read((unsigned char *)CurrPos + ChunkHeaderLen,Config::WriteBuffSize - ChunkHeaderLen - ChunkFooterLength,ReadLength,
				Yield);
			if (ReadLength)
			{
				sprintf(CurrPos,"%08x\r",ReadLength);
				CurrPos[8+1]='\n'; //Replace the '\0' with '\n' in the chunk header.

				CurrPos[ReadLength + ChunkHeaderLen]='\r';
				CurrPos[ReadLength + ChunkHeaderLen + 1]='\n';

				WriteBuff.Commit(ReadLength + ChunkHeaderLen + ChunkFooterLength);
			}
			else
			{
				IsFinished=true;
				WriteBuff.Commit(0);
			}

			if (!IsFinished)
				WriteNext(Yield);
			else
				break;
		}

		CurrPos=(char *)WriteBuff.Allocate(FinalChunkLength);
		memcpy(CurrPos,"0\r\n\r\n",FinalChunkLength);
		WriteBuff.Commit(FinalChunkLength);
		WriteAll(Yield);

		NextConn=CurrResp->Upgrade(this);
		delete CurrResp;

		return !NextConn;
	}
}

void Connection::CreatePostHeaderBuff()
{
	if (!PostHeaderBuff)
	{
		PostHeaderBuff=new char[Config::MaxHeadersLength];
		PostHeaderBuffEnd=PostHeaderBuff+Config::MaxHeadersLength;
	}

	PostHeaderBuffPos=PostHeaderBuff;
}

char *Connection::AddPostHeader(const unsigned char *Begin, const unsigned char *End)
{
	unsigned int Length=(unsigned int)(End-Begin);
	if (PostHeaderBuffPos+Length<=PostHeaderBuffEnd)
	{
		char *PrevPos=PostHeaderBuffPos;
		memcpy(PrevPos,Begin,Length);
		PostHeaderBuffPos+=Length;
		return PrevPos;
	}
	else
		return NULL;
}

METHOD Connection::ParseMethod(const unsigned char *Begin, const unsigned char *End)
{
	if ((End-Begin==3) && (memcmp(Begin,"GET",3)==0))
		return METHOD_GET;
	if ((End-Begin==4) && (memcmp(Begin,"POST",4)==0))
		return METHOD_POST;
	else
		return METHOD_UNKNOWN;
}
