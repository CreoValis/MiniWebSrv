#include "Connection.h"

#include "Common/TimeUtils.h"

#include "IRespSource.h"
#include "IServerLog.h"

#include "RespSources/CommonErrorRespSource.h"
#include "RespSources/CORSPreflightRespSource.h"

using namespace HTTP;

Connection::Connection(boost::asio::io_context &MyIOS, RespSource::CommonError *NewErrorRS, RespSource::CORSPreflight *NewCorsPFRS, const char *NewServerName,
	Config::Connection Conf, Config::FileUpload FUConf) :
	ConnectionBase(MyIOS),
	MyIOS(MyIOS), MyStrand(MyIOS.get_executor()), SilentTime(0), IsDeletable(true),
	CurrQuery(FUConf),
	ContentLength(0), ContentBuff(nullptr), ContentEndBuff(nullptr),
	ServerName(NewServerName), MyRespSource(nullptr), MyLog(nullptr), ErrorRS(NewErrorRS), CorsPFRS(NewCorsPFRS),
	PostHeaderBuff(nullptr), PostHeaderBuffEnd(nullptr),
	NextConn(nullptr), Conf(Conf)
{

}

Connection::~Connection()
{
	if (MyLog)
		MyLog->OnConnectionFinished(this);

	delete[] PostHeaderBuff;

	CurrQuery.DeleteUploadedFiles();

	delete NextConn;
}

void Connection::Start(IRespSource *NewRespSource, IServerLog *NewLog)
{
	IsDeletable=false;

	MyRespSource=NewRespSource;
	MyLog=NewLog;
	boost::asio::spawn(MyStrand, boost::bind(&Connection::ProtocolHandler, this, boost::placeholders::_1), boost::asio::detached);
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
		SilentTime=Conf.MaxSilentTime;
	}

	SilentTime+=StepInterval;
	if (SilentTime>Conf.MaxSilentTime)
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

	if (FreeLength)
	{
		std::size_t ReadCount=MySock.async_read_some(boost::asio::buffer(ReadPos, FreeLength), Yield);
		ReadBuff.OnNewData(ReadCount);
		SilentTime=0;
	}
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
	/**Parser algorithm:
	- Read as many bytes as possible.
	- While still reading headers:
		- Consume and keep every byte, looking for '\n', and recoding the position of the first ':'.
		- If this is the first line in this request:
			- Parse as a request line.
			- Otherwise, parse as a header. If no ':' was found, close the connection.
		- If the line is empty, then search the headers for content-type and content-length, and switch to content mode.
	- If this is a POST request, and content should be read:
		- Save the header's data to a temporary buffer, and modify the current header
		- Discard the currently kept header data.
		- If reading form/url-encoded content:
			- Read the whole content, keep every byte. When finished, give the content to the QueryParams object.
		- If reading form/multipart content:
			- Read the whole content in blocks.Give every block to the QueryParams object.
		- When reading some other content:
			- Read the whole content, keep every byte.
	- When the content was read fully (or there was no content), call ResponseHandler.
	- Discard every content byte, and start reading headers again.
	*/

	try
	{
		bool IsKeepAlive=true;

		//Read new requests while we can.
		while (IsKeepAlive)
		{
			//Reset the request states.
			ResetRequestData();
			ReqStartTime=std::chrono::steady_clock::time_point(std::chrono::steady_clock::duration(0));

			if (!HeaderHandler(Yield))
				//Header parse error. Close the connection.
				break;

			if (CurrMethod==METHOD_POST)
			{
				//Parse request content.
				if (!ContentHandler(Yield))
					break;
			}

			//Process the fully parsed response.
			if (!ResponseHandler(Yield))
				break;

			//Clear every kept byte in the read buffer.
			ReadBuff.ResetRelevant();

			if (CurrVersion==VERSION_10)
				IsKeepAlive=false;
			else if (CurrVersion==VERSION_11)
				//We keep the connection alive by default, unless the client asks otherwise.
				for (const auto &Header : HeaderA)
					if ((Header.IntName==HTTP::HN_CONNECTION) && (CompareLowercaseSimple(Header.Value, "close")))
						IsKeepAlive=false;
		}
	}
	catch (boost::context::detail::forced_unwind &) { throw; }
	catch (...)
	{ }

	try { MySock.close(); }
	catch (...) { }
	IsDeletable=true;
}

bool Connection::HeaderHandler(boost::asio::yield_context Yield)
{
	//Read the header block.

	bool RequestLineFound=false;
	unsigned int LineStartPos=0;
	while (true)
	{
		if (!ReadBuff.GetAvailableDataLength())
		{
			ContinueRead(Yield);

			if (!ReadBuff.GetAvailableDataLength())
				//Headers longer than static read buffer.
				return false;
		}

		if (!ReqStartTime.time_since_epoch().count())
			//Start counting the request time from the point we received the first bytes.
			ReqStartTime=std::chrono::steady_clock::now();

		unsigned int AvailableDataLength;
		const unsigned char *InBuff=ReadBuff.GetAvailableData(AvailableDataLength), *InBuffEnd=InBuff+AvailableDataLength;

		unsigned int RelevantDataLength;
		const unsigned char *RelevantBuff=ReadBuff.GetRelevantData(RelevantDataLength);

		//Parse this block of data.
		while (InBuff!=InBuffEnd)
		{
			unsigned char CurrVal=*InBuff;
			if (CurrVal=='\n')
			{
				if (InBuff-RelevantBuff-LineStartPos>1)
				{
					//This is a non-empty line.
					if (RequestLineFound)
						HeaderA.push_back(Header((char *)RelevantBuff+LineStartPos,(char *)InBuff));
					else
					{
						//This is the request line.
						if (ParseRequestLine(RelevantBuff,InBuff))
							RequestLineFound=true;
						else
							return false;
					}

					LineStartPos=InBuff-RelevantBuff+1;
				}
				else
				{
					//This is an empty line. Consume every byte so far, including this one.
					ReadBuff.Consume(InBuff-ReadBuff.GetAvailableData(AvailableDataLength)+1,true);
					return true;
				}
			}

			++InBuff;
		}

		//Consume every byte we've read.
		ReadBuff.Consume(AvailableDataLength,true);
		ContinueRead(Yield);
	}

	return false;
}

bool Connection::ContentHandler(boost::asio::yield_context Yield)
{
	//This is a POST request. Search for a content-type and a content-length header.
	enum HEADERFLAG
	{
		HF_CONTENTTYPE    = 1 << 0,
		HF_CONTENTLENGTH  = 1 << 1,
		HF_REQUIRED       = HF_CONTENTLENGTH,
		HF_ALL            = HF_CONTENTLENGTH | HF_CONTENTTYPE,
	};

	unsigned int HeaderFoundFlags=0;
	HTTP::CONTENTTYPE ContentType=CT_UNKNOWN;
	for (const Header &CurrHeader : HeaderA)
	{
		if (CurrHeader.IntName==HN_CONTENT_LENGTH)
		{
			ContentLength=CurrHeader.GetULongLong();
			HeaderFoundFlags|=HF_CONTENTLENGTH;
		}
		else if (CurrHeader.IntName==HN_CONTENT_TYPE)
		{
			ContentType=CurrHeader.GetContentType(CurrQuery.GetBoundaryStr());
			CurrQuery.OnBoundaryParsed();
			HeaderFoundFlags|=HF_CONTENTTYPE;
		}

		if ((HeaderFoundFlags & HF_ALL)==HF_ALL)
			break;
	}

	if ((HeaderFoundFlags & HF_REQUIRED)!=HF_REQUIRED)
		return false;

	ContentBuff=nullptr;
	ContentEndBuff=nullptr;

	if (!ContentLength)
		//No content to parse.
		return true;
	else if (ContentLength>Conf.MaxPostBodyLength)
		//Content too large.
		return false;

	if ((ContentType==CT_URL_ENCODED) || (ContentType==CT_UNKNOWN))
	{
		while (!ReadBuff.RequestData((unsigned int)ContentLength))
			ContinueRead(Yield);

		//The actual content is the currently available data.
		unsigned int AvailableLength;
		ContentBuff=ReadBuff.GetAvailableData(AvailableLength);
		ContentEndBuff=ContentBuff+AvailableLength;

		if (ContentType==CT_URL_ENCODED)
			CurrQuery.AddURLEncoded((const char *)ContentBuff,(const char *)ContentEndBuff);

		ReadBuff.Consume(AvailableLength,true);
		return true;
	}
	else if (ContentType==CT_FORM_MULTIPART)
	{
		//We have to save the header's contents from the read buffer to a separate buffer.
		unsigned int RelevantLength;
		const unsigned char *RelevantBuff=ReadBuff.GetRelevantData(RelevantLength);

		if ((unsigned int)(PostHeaderBuffEnd-PostHeaderBuff)<RelevantLength)
		{
			delete[] PostHeaderBuff;
			PostHeaderBuff=new char[RelevantLength];
			PostHeaderBuffEnd=PostHeaderBuff+RelevantLength;
		}

		memcpy(PostHeaderBuff,RelevantBuff,RelevantLength);

		//Now that we have the header's data saved, we have to offset the Header structures' internal pointers.
		std::ptrdiff_t Offset=PostHeaderBuff-(const char *)RelevantBuff;
		for (Header &CurrHeader : HeaderA)
		{
			CurrHeader.Name+=Offset;
			CurrHeader.Value+=Offset;
		}

		//We can now release the buffer space held by the headers' data, and start reading the content in chunks.
		ReadBuff.ResetRelevant();

		unsigned long long RemLength=ContentLength;
		while (RemLength)
		{
			unsigned int AvailableLength;
			const unsigned char *AvailableBuff=ReadBuff.GetAvailableData(AvailableLength);
			if (AvailableLength<RemLength)
			{
				CurrQuery.AppendFormMultipart((const char *)AvailableBuff,(const char *)AvailableBuff+AvailableLength);
				ReadBuff.Consume((unsigned int)AvailableLength);
				RemLength-=AvailableLength;

				ReadBuff.RequestData(RemLength<BuildConfig::ReadBuffSize ? (unsigned int)RemLength : BuildConfig::ReadBuffSize);
				ContinueRead(Yield);
			}
			else
			{
				CurrQuery.AppendFormMultipart((const char *)AvailableBuff,(const char *)AvailableBuff+RemLength);
				ReadBuff.Consume((unsigned int)RemLength);
				RemLength=0;
			}
		}

		return true;
	}
	else
		//Unknown ContentType.
		return false;
}

bool Connection::ParseRequestLine(const unsigned char *Begin, const unsigned char *End)
{
	const unsigned char *CurrPos=Begin;
	while (true)
	{
		if (CurrPos==End)
			return false;

		if (*CurrPos==' ')
		{
			CurrMethod=ParseMethod(Begin,CurrPos);
			++CurrPos;
			break;
		}
		else
			++CurrPos;
	}

	Begin=CurrPos;
	unsigned char CurrVal;
	while (true)
	{
		if (CurrPos==End)
			return false;

		CurrVal=*CurrPos;
		if ((CurrVal==' ') || (CurrVal=='?'))
		{
			CurrResource.assign((const char *)Begin,(const char *)CurrPos);

			++CurrPos;
			break;
		}
		else
			++CurrPos;
	}

	if (CurrVal=='?')
	{
		//We have a query part to parse.

		Begin=CurrPos;
		while (true)
		{
			if (CurrPos==End)
				return false;

			if (*CurrPos==' ')
			{
				CurrQuery.AddURLEncoded((const char *)Begin,(const char *)CurrPos);
				++CurrPos;
				break;
			}
			else
				++CurrPos;
		}
	}

	Begin=CurrPos;
	while (true)
	{
		if (CurrPos==End)
			return false;

		if (*CurrPos++=='/')
		{
			if (End-CurrPos>=3)
			{
				if (memcmp(CurrPos,"1.0",3)==0)
					CurrVersion=VERSION_10;
				else
					CurrVersion=VERSION_11;
			}
			else
			{
				CurrVersion=VERSION_11;
			}

			return true;
		}
	}
}

bool Connection::ResponseHandler(boost::asio::yield_context &Yield)
{
	ResponseCount++;

	std::chrono::steady_clock::time_point ReqEndTime=std::chrono::steady_clock::now();

	IResponse *CurrResp;
	IRespSource::AsyncHelperHolder AsyncHelper(MyStrand, MyIOS,Yield);

	bool WriteCORSHeaders;
	try
	{
		if (HandleCORS())
		{
			//Try a CORS preflight request.
			CurrResp=CorsPFRS->Create(CurrMethod, CurrResource, CurrQuery,
				HeaderA, ContentBuff, ContentEndBuff, AsyncHelper, this);

			if (!CurrResp)
			{
				//Not a CORS preflight request.
				CurrResp=MyRespSource->Create(CurrMethod, CurrResource, CurrQuery,
					HeaderA, ContentBuff, ContentEndBuff, AsyncHelper, this);
				WriteCORSHeaders=true;
			}
			else
				WriteCORSHeaders=false;
		}
		else
			CurrResp=MyRespSource->Create(CurrMethod,CurrResource,CurrQuery,
				HeaderA,ContentBuff,ContentEndBuff,AsyncHelper,this);
	}
	catch (const std::exception &Ex)
	{
		CurrResp=ErrorRS->CreateFromException(CurrMethod,CurrResource,CurrQuery,
			HeaderA,ContentBuff,ContentEndBuff,AsyncHelper,this,&Ex);
		WriteCORSHeaders=true;
	}
	catch (...)
	{
		CurrResp=ErrorRS->Create(CurrMethod,CurrResource,CurrQuery,
			HeaderA,ContentBuff,ContentEndBuff,AsyncHelper,this);
		WriteCORSHeaders=true;
	}

	char *CurrPos=(char *)WriteBuff.Allocate(Conf.MaxHeadersLength);
	char *CurrPosBegin=CurrPos;
	char *CurrPosEnd=CurrPos+Conf.MaxHeadersLength - 2; //Leave room for the final "\r\n".

	unsigned int RespCode=CurrResp->GetResponseCode();
	{
		CurrPos+=snprintf(CurrPos,CurrPosEnd-CurrPos,"HTTP/1.1 %d %s\r\nServer: %s\r\n",
			RespCode,GetResponseName((RESPONSECODE)RespCode),ServerName);
	}

	if (const char *EncodingStr=CurrResp->GetContentTypeCharset())
		CurrPos+=snprintf(CurrPos, CurrPosEnd-CurrPos,"Content-Type: %s; charset=\"%s\"\r\n",CurrResp->GetContentType(),EncodingStr);
	else
		CurrPos+=snprintf(CurrPos, CurrPosEnd-CurrPos,"Content-Type: %s\r\n",CurrResp->GetContentType());

	if ((HandleCORS()) && (WriteCORSHeaders))
		CurrPos+=snprintf(CurrPos, CurrPosEnd-CurrPos, "Access-Control-Allow-Origin: *\r\n");

	{
		//Append the current date.
		time_t CurrTime=time(NULL);
		tm TM;
		if (UD::TimeUtils::GMTime(&CurrTime,&TM))
			CurrPos+=strftime(CurrPos,40,"Date: %a, %d %b %Y %H:%M:%S GMT\r\n",&TM);
	}

	unsigned long long RespLength=CurrResp->GetLength();
	if (RespLength!=~(unsigned long long)0)
		CurrPos+=snprintf(CurrPos, CurrPosEnd-CurrPos,"Content-Length: %llu\r\n",RespLength);
	else
		//Response length not known: use chunked encoding.
		CurrPos+=snprintf(CurrPos, CurrPosEnd-CurrPos,"Transfer-Encoding: chunked\r\n");

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
	CurrPos+=snprintf(CurrPos, CurrPosEnd-CurrPos,"\r\n");
	WriteBuff.Commit((unsigned int)(CurrPos-CurrPosBegin));
	WriteNext(Yield);

	if (RespLength!=~(unsigned long long)0)
	{
		unsigned long long TotalWriteLength=0;

		bool RetVal=false;
		while (RespLength)
		{
			SilentTime=0;
			CurrPos=(char *)WriteBuff.Allocate(BuildConfig::WriteBuffSize);

			unsigned int ReadLength;
			bool IsFinished=CurrResp->Read((unsigned char *)CurrPos,BuildConfig::WriteBuffSize,ReadLength,
				Yield);

			if (ReadLength<=RespLength)
				RespLength-=ReadLength;
			else
				RespLength=0;

			WriteBuff.Commit(ReadLength);
			TotalWriteLength+=ReadLength;
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

		std::chrono::steady_clock::time_point RespEndTime=std::chrono::steady_clock::now();

		MyLog->OnRequest(this,CurrMethod,CurrResource,CurrQuery,
			HeaderA,ContentLength,ContentBuff,ContentEndBuff,
			RespCode,TotalWriteLength,
			(ReqEndTime-ReqStartTime).count()/(double)std::chrono::steady_clock::duration::period::den,
			(RespEndTime-ReqEndTime).count()/(double)std::chrono::steady_clock::duration::period::den,
			NextConn);

		return RetVal && !NextConn;
	}
	else
	{
		static const unsigned int ChunkHeaderLen=8 + 2;
		static const unsigned int ChunkFooterLength=2;
		static const unsigned int FinalChunkLength=1 + 2 + 2;

		unsigned long long TotalWriteLength=0;
		while (true)
		{
			SilentTime=0;
			CurrPos=(char *)WriteBuff.Allocate(BuildConfig::WriteBuffSize);

			unsigned int ReadLength;
			bool IsFinished=CurrResp->Read((unsigned char *)CurrPos + ChunkHeaderLen,BuildConfig::WriteBuffSize - ChunkHeaderLen - ChunkFooterLength,ReadLength,
				Yield);
			if (ReadLength)
			{
				snprintf(CurrPos, ChunkHeaderLen + 1,"%08x\r",ReadLength);
				CurrPos[8+1]='\n'; //Replace the '\0' with '\n' in the chunk header.

				CurrPos[ReadLength + ChunkHeaderLen]='\r';
				CurrPos[ReadLength + ChunkHeaderLen + 1]='\n';

				WriteBuff.Commit(ReadLength + ChunkHeaderLen + ChunkFooterLength);
				TotalWriteLength+=ReadLength;
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

		std::chrono::steady_clock::time_point RespEndTime=std::chrono::steady_clock::now();

		MyLog->OnRequest(this,CurrMethod,CurrResource,CurrQuery,
			HeaderA,ContentLength,ContentBuff,ContentEndBuff,
			RespCode,TotalWriteLength,
			(ReqEndTime-ReqStartTime).count()/(double)std::chrono::steady_clock::duration::period::den,
			(RespEndTime-ReqEndTime).count()/(double)std::chrono::steady_clock::duration::period::den,
			NextConn);

		return !NextConn;
	}
}

void Connection::ResetRequestData()
{
	CurrVersion=VERSION_11;
	CurrMethod=METHOD_UNKNOWN;

	CurrResource.reserve(CurrResource.capacity());
	CurrQuery=QueryParams();
	ContentLength=0;

	HeaderA.reserve(HeaderA.capacity());
	HeaderA.clear();
}

METHOD Connection::ParseMethod(const unsigned char *Begin, const unsigned char *End)
{
	if ((End-Begin==3) && (memcmp(Begin,"GET",3)==0))
		return METHOD_GET;
	else if ((End-Begin==4) && (memcmp(Begin,"POST",4)==0))
		return METHOD_POST;
	else if ((End-Begin==7) && (memcmp(Begin, "OPTIONS", 7)==0))
		return METHOD_OPTIONS;
	else
		return METHOD_UNKNOWN;
}

bool Connection::CompareLowercaseSimple(const char *TestStr, const char *LowerCaseStr)
{
	while (char Val=*TestStr++)
	{
		if ((Val>='A') && (Val<='Z'))
			Val=Val-'A'+'a';

		auto TestVal=*LowerCaseStr++;
		if (Val!=TestVal)
			return false;
	}

	return !*LowerCaseStr;
}
