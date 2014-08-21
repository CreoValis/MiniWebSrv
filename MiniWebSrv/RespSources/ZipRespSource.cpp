#include "ZipRespSource.h"

#include <stdexcept>

#include "CommonErrorRespSource.h"
#include "detail/MimeDB.h"

using namespace HTTP;
using namespace HTTP::RespSource;

const char *Zip::Response::IdentityEncoding="identity";
const char *Zip::Response::GZipEncoding="gzip";
unsigned char Zip::Response::GZipHeaderA[10]={
	31, 139, //ID1 ID2
	8, //CM; 8: deflate
	0, //FLG
	0, 0, 0, 0, //MTIME
	2, //XFL
	255 //OS
};

Zip::Response::Response(ZipArchive::Stream *ArchS, const char *MimeType, time_t IfModifiedSince) : MyMimeType(MimeType)
{
	time_t LastModTime=ArchS->GetInfo()->LastModTime;
	Header::FormatDateTime(LastModTime,LastModifiedStr);

	if (LastModTime>IfModifiedSince)
	{
		SourceS=ArchS;
		FileSize=ArchS->GetInfo()->CompressedSize;

		if (SourceS->GetInfo()->Compression==ZipArchive::CM_DEFLATE)
		{
			//Compose a Gzip footer.
			*(unsigned int *)GzipFooterA=SourceS->GetInfo()->CRC32;
			*((unsigned int *)GzipFooterA+1)=SourceS->GetInfo()->UncompressedSize;

			CState=CS_HEADER;
			CStatePos=0;

			FileSize+=sizeof(GZipHeaderA) + sizeof(GzipFooterA);
		}
		else
			CState=CS_BODYONLY;
	}
	else
	{
		SourceS=nullptr;
		delete ArchS;
		FileSize=NotModifiedSize;
	}
}

bool Zip::Response::GetExtraHeader(unsigned int Index,
	const char **OutHeader, const char **OutHeaderEnd,
	const char **OutHeaderVal, const char **OutHeaderValEnd)
{
	if (Index==0)
	{
		const std::string &HName=Header::GetHeaderName(HN_LAST_MODIFIED);
		*OutHeader=HName.data();
		*OutHeaderEnd=HName.data() + HName.size();
		*OutHeaderVal=LastModifiedStr;
		*OutHeaderValEnd=LastModifiedStr+sizeof(LastModifiedStr);

		return true;
	}
	else if (Index==1)
	{
		const std::string &HName=Header::GetHeaderName(HN_CONTENT_ENCODING);
		*OutHeader=HName.data();
		*OutHeaderEnd=HName.data() + HName.size();

		if ((!SourceS) || (SourceS->GetInfo()->Compression!=ZipArchive::CM_DEFLATE))
		{
			*OutHeaderVal=IdentityEncoding;
			*OutHeaderValEnd=IdentityEncoding + strlen(IdentityEncoding);
		}
		else
		{
			*OutHeaderVal=GZipEncoding;
			*OutHeaderValEnd=GZipEncoding + strlen(GZipEncoding);
		}

		return true;
	}
	else
		return false;
}

bool Zip::Response::Read(unsigned char *TargetBuff, unsigned int MaxLength, unsigned int &OutLength,
		boost::asio::yield_context &Ctx)
{
	if (FileSize!=NotModifiedSize)
	{
		unsigned int RemLength=MaxLength;
		while (RemLength)
		{
			switch (CState)
			{
			case CS_BODY:
			case CS_BODYONLY:
				{
					unsigned int ReadCount=SourceS->Read((char *)TargetBuff,RemLength);
					if (ReadCount<RemLength)
					{
						if (CState==CS_BODY)
						{
							CState=CS_FOOTER;
							CStatePos=0;
						}
						else
						{
							OutLength=MaxLength-RemLength;
							return true;
						}
					}

					RemLength-=ReadCount;
					TargetBuff+=ReadCount;
				}
				break;
			case CS_HEADER:
				{
					unsigned int CopyLen;
					if (RemLength>=sizeof(GZipHeaderA)-CStatePos)
					{
						CopyLen=sizeof(GZipHeaderA)-CStatePos;
						CState=CS_BODY;
						CStatePos=0;
					}
					else
					{
						CopyLen=RemLength;
						CStatePos+=CopyLen;
					}

					memcpy(TargetBuff,GZipHeaderA+CStatePos,CopyLen);
					RemLength-=CopyLen;
					TargetBuff+=CopyLen;
				}
				break;
			case CS_FOOTER:
				{
					unsigned int CopyLen;
					if (RemLength>=sizeof(GzipFooterA)-CStatePos)
						CopyLen=sizeof(GzipFooterA)-CStatePos;
					else
						CopyLen=RemLength;

					memcpy(TargetBuff,GzipFooterA+CStatePos,CopyLen);
					CStatePos+=CopyLen;
					RemLength-=CopyLen;
					TargetBuff+=CopyLen;

					if (CStatePos==sizeof(GzipFooterA))
					{
						OutLength=MaxLength-RemLength;
						return true;
					}
				}
				break;
			}
		}

		OutLength=MaxLength-RemLength;
		return false;
	}
	else
	{
		OutLength=0;
		return true;
	}
}

Zip::Zip(const boost::filesystem::path &ArchiveFN) : MyArch(ArchiveFN.string())
{
}

IResponse *Zip::Create(METHOD Method, const std::string &Resource, const QueryParams &Query, const std::vector<Header> &HeaderA,
		const unsigned char *ContentBuff, const unsigned char *ContentBuffEnd,
		boost::asio::io_service &ParentIOS)
{
	const ZipArchive::FileInfo *TargetFI=MyArch.Get(Resource.length() ? Resource.substr(1,Resource.length()-1) : Resource);
	if (!TargetFI)
		return new CommonError::Response(Resource,HeaderA,NULL,RC_NOTFOUND);

	time_t IfModSinceTime=0;
	try
	{
		for (const Header &CurrH : HeaderA)
		{
			if (CurrH.IntName==HN_IF_MOD_SINCE)
			{
				IfModSinceTime=CurrH.GetDateTime();
				break;
			}
		}
	}
	catch (...) { }

	try { return new Response(MyArch.GetStream(TargetFI,false),GetMimeType(Resource),IfModSinceTime); }
	catch (...) { return new CommonError::Response(Resource,HeaderA,NULL,RC_NOTFOUND); }
}

const char *Zip::GetMimeType(const std::string &FileName)
{
	std::string::size_type DotPos=FileName.rfind('.');

	if (DotPos!=std::string::npos)
		return MimeDB::GetMimeType(FileName.substr(DotPos));
	else
		return MimeDB::GetMimeType("");
}
