#include "QueryParams.h"

#include <boost/filesystem/fstream.hpp>

#include "Header.h"

using namespace HTTP;

QueryParams::QueryParams() : FMParseState(STATE_HEADERSTART), BoundaryParseCounter(2), CurrFileS(nullptr)
{

}

QueryParams::QueryParams(const FileUploadParams &UploadParams) : FMParseState(STATE_HEADERSTART), BoundaryParseCounter(2),
	FUParams(UploadParams),
	CurrFileS(nullptr)
{

}

bool QueryParams::AddURLEncoded(const char *Begin, const char *End)
{
	enum PARSESTATE
	{
		PS_PARAM_END,
		PS_VALUE_END,
	} CurrState=PS_PARAM_END;

	ParseTmp.reserve(ParseTmp.size());
	ParseTmp.clear();

	std::string *ValStr=nullptr;
	std::string *TargetStr= &ParseTmp;

	while (Begin!=End)
	{
		char CurrVal=*Begin++;

		if (CurrVal=='+')
			TargetStr->append((std::string::size_type)1,' ');
		else if ((CurrVal=='%') && (End-Begin>0))
		{
			TargetStr->append((std::string::size_type)1,(std::string::value_type)(
				(GetHexVal(*Begin) << 4) |
				GetHexVal(*(Begin+1)) ) );
			Begin+=2;
		}
		else
		{
			if (CurrState==PS_PARAM_END)
			{
				if (CurrVal=='=')
				{
					Param &CurrParam=ParamMap[ParseTmp];
					CurrParam.Value.clear();

					ValStr=&CurrParam.Value;

					CurrState=PS_VALUE_END;
					TargetStr=ValStr;
				}
				else
					TargetStr->append((std::string::size_type)1,CurrVal);
			}
			else if (CurrState==PS_VALUE_END)
			{
				if (CurrVal=='&')
				{
					ValStr=nullptr;

					ParseTmp.reserve(ParseTmp.size());
					ParseTmp.clear();

					CurrState=PS_PARAM_END;
					TargetStr=&ParseTmp;
				}
				else
					TargetStr->append((std::string::size_type)1,CurrVal);
			}
		}
	}

	return true;
}

bool QueryParams::AppendFormMultipart(const char *Begin, const char *End)
{
/*Continous boundary detection:
	set content start to start of the input data
	for each input character:
		if the current char equals to the parse position in the boundary string:
			advance the parse position
			if the boundary string is fully matched:
				we have content from content start to the start of the boundary string.
				if the boundary string starts after the end of the input data:
					the bytes in [start of content, start of boundary) are content
				reset the parse position
				set content start to current position + 1
		otherwise:
			the bytes in temporary data are content
			reset the parse position
			clear the temporary data storage
	save the parse position
	the bytes [content start, end-parse position) are content
	store the bytes [end-parse position, end) as temporary data*/

	const char *ContentBegin=Begin;
	while (Begin!=End)
	{
		char CurrVal=*Begin;
		if (CurrVal==BoundaryStr[BoundaryParseCounter])
		{
			if (++BoundaryParseCounter==BoundaryStr.length())
			{
				if (!AppendToCurrentPart(ContentBegin,Begin-BoundaryParseCounter+1))
					return false;
				ContentBegin=Begin + 1;

				BoundaryParseCounter=0;
				StartNewPart();
			}
		}
		else
		{
			if (!ParseTmp.empty())
			{
				if (!AppendToCurrentPart(ParseTmp.data(),ParseTmp.data()+ParseTmp.length()))
					return false;
				ParseTmp.clear();
			}

			BoundaryParseCounter=0;
		}

		Begin++;
	}

	if (!AppendToCurrentPart(ContentBegin,Begin-BoundaryParseCounter))
		return false;
	ParseTmp.append(Begin-BoundaryParseCounter,Begin);

	return true;
}

const QueryParams::Param &QueryParams::Get(const std::string &Name) const
{
	ParamMapType::const_iterator FindI=ParamMap.find(Name);
	if (FindI!=ParamMap.end())
		return FindI->second;
	else
		throw ParameterNotFound();
}

const QueryParams::Param QueryParams::Get(const std::string &Name, const std::string &Default) const
{
	ParamMapType::const_iterator FindI=ParamMap.find(Name);
	if (FindI!=ParamMap.end())
		return FindI->second;
	else
		return Param(Default);
}

void QueryParams::DecodeURLEncoded(std::string &Target, const char *Begin, const char *End)
{
	while (Begin!=End)
	{
		char CurrVal=*Begin++;

		if (CurrVal=='+')
			Target.append((std::string::size_type)1,' ');
		else if ((CurrVal=='%') && (End-Begin>0))
		{
			Target.append((std::string::size_type)1,(std::string::value_type)(
				(GetHexVal(*Begin) << 4) |
				GetHexVal(*(Begin+1)) ) );
			Begin+=2;
		}
		else
			Target.append((std::string::size_type)1,CurrVal);
	}
}

void QueryParams::OnBoundaryParsed()
{
	BoundaryStr="\r\n--" + BoundaryStr;
}

void QueryParams::DeleteUploadedFiles()
{
	for (FileMapType::value_type &CurrFile : FileMap)
	{
		boost::system::error_code DelErr;
		boost::filesystem::remove(CurrFile.second.Path,DelErr);
	}
}

void QueryParams::StartNewPart()
{
	FMParseState=STATE_HEADERSTART;
	FMParseCounter=0;
	HeaderParseTmp.clear();
}

bool QueryParams::AppendToCurrentPart(const char *Begin, const char *End)
{
	if ((Begin>=End) || (FMParseState==STATE_FINISHED))
		return true;

	const char *OrigBegin=Begin;
	while ((Begin!=End) && (FMParseState!=STATE_READ))
	{
		char CurrVal=*Begin;
		switch (FMParseState)
		{
		case STATE_HEADERSTART:
			if (FMParseCounter==0)
			{
				if ((CurrVal=='\r') || (CurrVal=='-'))
					FMParseCounter++;
				else
					return false;
			}
			else if (FMParseCounter==1)
			{
				if (CurrVal=='\n')
				{
					FMParseCounter=0;
					FMParseState=STATE_HEADERS_END;
					OrigBegin=Begin + 1;
				}
				else if (CurrVal=='-')
					FMParseState=STATE_FINISHED;
				else
					return false;
			}
			break;
		case STATE_HEADERS_END:
			if (CurrVal=="\r\n\r\n"[FMParseCounter])
			{
				if (++FMParseCounter==4)
				{
					//End of headers found.
					if (Begin-OrigBegin-4>0)
						HeaderParseTmp.append(OrigBegin,Begin+1);

					if (ParseFMHeaders(HeaderParseTmp.data(),HeaderParseTmp.data()+HeaderParseTmp.length()))
					{
						//Now, we can read the actual content until StartNewPart is called.
						FMParseState=STATE_READ;
						break;
					}
					else
					{
						HeaderParseTmp.clear();
						return false;
					}
				}
			}
			else
				FMParseCounter=0;
			break;
		case STATE_FINISHED:
			if (CurrFileS)
			{
				CurrFileS->close();
				delete CurrFileS;
				CurrFileS=nullptr;
			}
			return true;
		}

		Begin++;
	}

	if (FMParseState==STATE_READ)
	{
		if (IsCurrPartValid())
		{
			if (IsCurrPartFile())
			{
				CurrFileS->write(Begin,End-Begin);
				return !CurrFileS->fail();
			}
			else
			{
				CurrFMPart.TargetParam->Value.append(Begin,End);
				return true;
			}
		}
		else
			return false;
	}
	else if (FMParseState==STATE_HEADERS_END)
	{
		HeaderParseTmp.append(OrigBegin,Begin);
		return true;
	}
	else
		return true;
}

bool QueryParams::ParseFMHeaders(const char *HeadersBegin, const char *HeadersEnd)
{
	std::string Name, FileName;
	const char *ContentTypeVal=nullptr;

	const char *HBegin=HeadersBegin;
	while (HeadersBegin!=HeadersEnd)
	{
		char CurrVal=*HeadersBegin;
		if (CurrVal=='\n')
		{
			//Header in [HBegin,HeadersBegin[ .
			Header CurrHeader((char *)HBegin,(char *)HeadersBegin);
			if (CurrHeader.IntName==HN_CONTENT_DISPOSITION)
				CurrHeader.ParseContentDisposition(Name,FileName);
			else if (CurrHeader.IntName==HN_CONTENT_TYPE)
				ContentTypeVal=CurrHeader.Value;

			HBegin=HeadersBegin+1;
		}

		HeadersBegin++;
	}

	if (!Name.empty())
	{
		if (CurrFileS)
		{
			CurrFileS->close();
			delete CurrFileS;
			CurrFileS=nullptr;
		}

		if (ContentTypeVal)
		{
			try
			{
				boost::filesystem::path TmpFilePath=
					(FUParams.Root.empty() ? boost::filesystem::temp_directory_path() : FUParams.Root) /
					boost::filesystem::unique_path();

				CurrFileS=new boost::filesystem::ofstream(TmpFilePath,std::ios_base::binary);

				CurrFMPart.TargetFile=&FileMap[Name];
				CurrFMPart.TargetFile->MimeType=ContentTypeVal;
				CurrFMPart.TargetFile->OrigFileName=FileName;
				CurrFMPart.TargetFile->Path=TmpFilePath;
			}
			catch (...)
			{
				CurrFMPart.TargetFile=NULL;

				return false;
			}
		}
		else
			CurrFMPart.TargetParam=&ParamMap[Name];

		return true;
	}
	else
	{
		CurrFMPart.TargetFile=NULL;
		return false;
	}
}
