#include "QueryParams.h"

#include "Header.h"

namespace HTTP
{

std::tuple<boost::filesystem::path, bool> QueryParams::TempFileUploadHelper::OnNewFile(const std::string &Name, const std::string &OrigFileName, const std::string &MimeType)
{
	if (CurrFileS)
	{
		delete CurrFileS;
		CurrFileS=nullptr;
	}

	if ((!Params.MaxUploadSize) || (!Params.MaxTotalUploadSize))
		return std::make_tuple(boost::filesystem::path(), false);

	try
	{
		boost::filesystem::path TmpFilePath=
			(Params.Root.empty() ? boost::filesystem::temp_directory_path() : Params.Root) /
			boost::filesystem::unique_path();

		CurrFileS=new boost::filesystem::ofstream(TmpFilePath, std::ios_base::binary);
		return std::make_tuple(TmpFilePath, CurrFileS->is_open());
	}
	catch (...)
	{
		return std::make_tuple(boost::filesystem::path(), false);
	}
}

bool QueryParams::TempFileUploadHelper::OnFileData(const char *Begin, const char *End)
{
	if (!CurrFileS)
		return false;

	uintmax_t PartSize=(uintmax_t)(End-Begin);
	if ((CurrFileSize+=PartSize)>Params.MaxUploadSize)
		return false;

	if ((TotalFileSize+=PartSize)>Params.MaxTotalUploadSize)
		return false;

	CurrFileS->write(Begin, End-Begin);
	return !CurrFileS->fail();
}

void QueryParams::TempFileUploadHelper::OnFileEnd()
{
	if (CurrFileS)
	{
		CurrFileS->close();
		delete CurrFileS;
		CurrFileS=nullptr;
	}
}

const std::string QueryParams::UnknownFileContentType;

QueryParams::QueryParams() : FMParseState(STATE_HEADERSTART), BoundaryParseCounter(2), CurrPartMode(PARTMODE_STRING),
	UploadHelper(&TempUploadHelper)
{

}

QueryParams::QueryParams(QueryParams &&other) :
	BoundaryParseCounter(other.BoundaryParseCounter), FMParseCounter(other.FMParseCounter),
	TempUploadHelper(std::move(other.TempUploadHelper)),
	BoundaryStr(std::move(other.BoundaryStr)),
	ParseTmp(std::move(other.ParseTmp)), HeaderParseTmp(std::move(other.HeaderParseTmp)),
	ParamMap(std::move(other.ParamMap)),
	FileMap(std::move(other.FileMap)),
	CurrFMPart(other.CurrFMPart),
	CurrPartMode(other.CurrPartMode),
	UploadHelper(other.UploadHelper==&other.TempUploadHelper ? &TempUploadHelper : other.UploadHelper)
{

}

QueryParams::QueryParams(const std::tuple<const char *, const char *> &URLEncodedRange) :
	FMParseState(STATE_HEADERSTART), BoundaryParseCounter(2), CurrPartMode(PARTMODE_STRING), UploadHelper(&TempUploadHelper)
{
	AddURLEncoded(URLEncodedRange);
}

QueryParams::QueryParams(const Config::FileUpload &UploadParams) : FMParseState(STATE_HEADERSTART), BoundaryParseCounter(2),
	TempUploadHelper(UploadParams),
	CurrPartMode(PARTMODE_STRING),
	UploadHelper(&TempUploadHelper)
{

}

QueryParams::QueryParams(IFileUpdloadHelper * UploadHelper) : FMParseState(STATE_HEADERSTART), BoundaryParseCounter(2),
	CurrPartMode(PARTMODE_STRING),
	UploadHelper(UploadHelper)
{

}

bool QueryParams::AddURLEncoded(const char *Begin, const char *End)
{
	if ((!Begin) || (!End))
		return false;

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
					std::string &CurrParam=ParamMap[ParseTmp];
					CurrParam.clear();

					ValStr=&CurrParam;

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

	const char *Pos=Begin, *ContentBegin=Begin;
	while (Pos!=End)
	{
		if (!BoundaryParseCounter)
		{
			while (Pos!=End)
			{
				if (*Pos!='\r')
					++Pos;
				else
				{
					++Pos;
					BoundaryParseCounter=1;
					break;
				}
			}

			if (Pos==End)
				break;
		}

		if (*Pos==BoundaryStr[BoundaryParseCounter])
		{
			//Boundary match; advance the test position.
			++BoundaryParseCounter;
			++Pos;
			if (BoundaryParseCounter==BoundaryStr.length())
			{
				//Full boundary match. The currently kept ParseTmp might contain content.
				if ((unsigned int)(Pos-Begin)<BoundaryParseCounter)
				{
					const char *TmpBoundaryStart=(ParseTmp.data()+ParseTmp.length()) - (BoundaryParseCounter-(Pos-Begin));
					if (TmpBoundaryStart>ParseTmp.data())
						AppendToCurrentPart(ParseTmp.data(), TmpBoundaryStart);
				}
				else if (ContentBegin<Pos-BoundaryParseCounter)
					AppendToCurrentPart(ContentBegin, Pos-BoundaryParseCounter);

				BoundaryParseCounter=0;
				ParseTmp.clear();
				ContentBegin=Pos;
				StartNewPart();
			}
		}
		else if (BoundaryParseCounter)
		{
			//Match broken.
			if (!ParseTmp.empty())
			{
				//The currently kept ParseTmp is actually content.
				AppendToCurrentPart(ParseTmp.data(), ParseTmp.data() + ParseTmp.size());
				ParseTmp.clear();
			}

			AppendToCurrentPart(ContentBegin, Pos);
			BoundaryParseCounter=0;
			ContentBegin=Pos;
		}
		else
			++Pos;
	}

	if (BoundaryParseCounter)
	{
		AppendToCurrentPart(ContentBegin, Pos-BoundaryParseCounter);
		ParseTmp.append(std::max(Pos-BoundaryParseCounter, Begin), Pos);
	}
	else if (Pos!=Begin)
		AppendToCurrentPart(ContentBegin, Pos);

	return true;
}

const std::string &QueryParams::Get(const std::string &Name) const
{
	ParamMapType::const_iterator FindI=ParamMap.find(Name);
	if (FindI!=ParamMap.end())
		return FindI->second;
	else
		throw ParameterNotFound();
}

const std::string *QueryParams::GetPtr(const std::string &Name) const
{
	ParamMapType::const_iterator FindI=ParamMap.find(Name);
	if (FindI!=ParamMap.end())
		return &FindI->second;
	else
		return nullptr;
}

const std::string QueryParams::Get(const std::string &Name, const std::string &Default) const
{
	ParamMapType::const_iterator FindI=ParamMap.find(Name);
	if (FindI!=ParamMap.end())
		return FindI->second;
	else
		return Default;
}

QueryParams &QueryParams::operator=(QueryParams &&other)
{
	BoundaryParseCounter=other.BoundaryParseCounter; FMParseCounter=other.FMParseCounter;
	TempUploadHelper=std::move(other.TempUploadHelper);
	BoundaryStr=std::move(other.BoundaryStr);
	ParseTmp=std::move(other.ParseTmp); HeaderParseTmp=std::move(other.HeaderParseTmp);
	ParamMap=std::move(other.ParamMap);
	FileMap=std::move(other.FileMap);
	CurrFMPart=other.CurrFMPart;
	CurrPartMode=other.CurrPartMode;
	UploadHelper=other.UploadHelper==&other.TempUploadHelper ? &TempUploadHelper : other.UploadHelper;
	return *this;
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
		if (!CurrFile.second.Path.empty())
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
				{
					FMParseState=STATE_FINISHED;
					UploadHelper->OnFileEnd();
				}
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
			UploadHelper->OnFileEnd();
			return true;
		default:
			break;
		}

		Begin++;
	}

	if (FMParseState==STATE_READ)
	{
		if (IsCurrPartValid())
		{
			if (IsCurrPartFile())
				return UploadHelper->OnFileData(Begin, End);
			else
			{
				CurrFMPart.TargetParam->append(Begin,End);
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
	bool FileNameExists;
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
				CurrHeader.ParseContentDisposition(Name,FileName, FileNameExists);
			else if (CurrHeader.IntName==HN_CONTENT_TYPE)
				ContentTypeVal=CurrHeader.Value;

			HBegin=HeadersBegin+1;
		}

		HeadersBegin++;
	}

	if (!Name.empty())
	{
		if (CurrPartMode==PARTMODE_FILE)
			UploadHelper->OnFileEnd();

		if (FileNameExists)
		{
			CurrPartMode=PARTMODE_FILE;

			auto Res=UploadHelper->OnNewFile(Name, FileName, ContentTypeVal ? ContentTypeVal : UnknownFileContentType);
			if (std::get<1>(Res))
			{
				CurrFMPart.TargetFile=&FileMap[Name];
				if (!CurrFMPart.TargetFile->Path.empty())
				{
					//We've already read a file with this name. Delete the previous file's temp data.
					boost::system::error_code DelErr;
					boost::filesystem::remove(CurrFMPart.TargetFile->Path, DelErr);
				}

				CurrFMPart.TargetFile->MimeType=ContentTypeVal ? ContentTypeVal : UnknownFileContentType;
				CurrFMPart.TargetFile->OrigFileName=FileName;
				CurrFMPart.TargetFile->Path=std::get<0>(Res);
			}
			else
				CurrFMPart.TargetFile=NULL;
		}
		else
		{
			CurrPartMode=PARTMODE_STRING;
			CurrFMPart.TargetParam=&ParamMap[Name];
			if (!CurrFMPart.TargetParam->empty())
				CurrFMPart.TargetParam->clear();
		}

		return true;
	}
	else
	{
		CurrFMPart.TargetFile=NULL;
		return false;
	}
}

} //HTTP
