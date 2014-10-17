#include "FSRespSource.h"

#include <type_traits>
#include <stdexcept>

#include <boost/locale.hpp>

#include "CommonErrorRespSource.h"
#include "detail/MimeDB.h"

using namespace HTTP;
using namespace HTTP::RespSource;

FS::Response::Response(const boost::filesystem::path &FileName, const char *MimeType, time_t IfModifiedSince) : MyMimeType(MimeType)
{
	time_t LastModTime=boost::filesystem::last_write_time(FileName);
	Header::FormatDateTime(LastModTime,LastModifiedStr);

	if ((!IfModifiedSince) || (LastModTime>IfModifiedSince))
	{
		FileSize=boost::filesystem::file_size(FileName);
		FilePos=0;

		InS.open(FileName.string().data(),std::ios_base::binary);
		if (!InS.is_open())
			throw std::runtime_error("Cannot open target file");
	}
	else
	{
		FileSize=NotModifiedSize;
		FilePos=0;
	}
}

bool FS::Response::GetExtraHeader(unsigned int Index,
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
	else
		return false;
}

bool FS::Response::Read(unsigned char *TargetBuff, unsigned int MaxLength, unsigned int &OutLength,
		boost::asio::yield_context &Ctx)
{
	if (FileSize!=NotModifiedSize)
	{
		unsigned long long RemLength=FileSize-FilePos;
		if (MaxLength>RemLength)
			MaxLength=(unsigned int)RemLength;

		InS.read((char *)TargetBuff,MaxLength);
	
		OutLength=MaxLength;
		FilePos+=MaxLength;

		return (FilePos==FileSize) || (InS.fail());
	}
	else
	{
		OutLength=0;
		return true;
	}
}

FS::FS(const boost::filesystem::path &NewRoot) : Root(boost::filesystem::canonical(NewRoot))
{
}

IResponse *FS::Create(METHOD Method, const std::string &Resource, const QueryParams &Query, const std::vector<Header> &HeaderA,
		const unsigned char *ContentBuff, const unsigned char *ContentBuffEnd,
		boost::asio::io_service &ParentIOS, void *ParentConn)
{
	boost::filesystem::path Target;
	try
	{
		if (std::is_same<boost::filesystem::path::value_type,wchar_t>())
			Target=boost::filesystem::canonical(Root / boost::locale::conv::utf_to_utf<wchar_t,char>(Resource));
		else
			Target=boost::filesystem::canonical(Root / Resource);
	}
	catch (...)
	{ return new CommonError::Response(Resource,HeaderA,NULL,RC_NOTFOUND); }

	if ( (std::distance(Root.begin(), Root.end())<=std::distance(Target.begin(), Target.end())) &&
	  (std::equal(Root.begin(), Root.end(), Target.begin())) &&
	  (boost::filesystem::exists(Target)) )
	{
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

		if (!boost::filesystem::is_directory(Target))
			return new Response(Target,GetMimeType(Target),IfModSinceTime);
		else
			return new CommonError::Response(Resource,HeaderA,NULL,RC_FORBIDDEN);
	}
	else
		return new CommonError::Response(Resource,HeaderA,NULL,RC_NOTFOUND);
}

const char *FS::GetMimeType(const boost::filesystem::path &FileName)
{
	return detail::MimeDB::GetMimeType(FileName.extension().string());
}
