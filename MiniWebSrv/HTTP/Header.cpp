#include "Header.h"

#include "Common/StringUtils.h"
#include "Common/TimeUtils.h"

using namespace HTTP;

/**Parses a string containing key=value pairs in HTTP header format. Supports value quoting, without quote escape.
Sample input stuff; key1=value1; key2="value2"
@param ParamConsumerType A functor with the following signature:
	Func(const CharType *KeyBegin, const CharType *KeyEnd, const CharType *ValueBegin, const CharType *ValueEnd)
	Note that ValueBegin and ValueEnd can be nullptr, if the given key
*/
template<class ParamConsumerType, class CharType, CharType Separator>
void ParseDelimitedKeyValue(const CharType *Source, ParamConsumerType Consumer)
{
	const CharType *CurrPos=Source;
	const CharType *KeyBegin=NULL, *KeyEnd=NULL;
	const CharType *ValueBegin=NULL;
	
	enum PARSE_STATE
	{
		PS_ZERO, //Searching for key.
		PS_KEY, //Searching for key end.
		PS_SEPSEARCH, //Searching for Separator.
		PS_VALSEARCH, //Searching for value.
		PS_VAL, //Searching for value end.
		PS_QUOTEDVAL, //Searching for value end in quoted value.
	} CurrState=PS_ZERO;

	CharType CurrVal;
	while (true)
	{
		CurrVal=*CurrPos;
		if (!CurrVal)
			break;

		switch (CurrState)
		{
		case PS_ZERO:
			if ((!UD::StringUtils::IsWS(CurrVal)) && (CurrVal!=';'))
			{
				KeyBegin=CurrPos;
				CurrState=PS_KEY;
			}
			break;
		case PS_KEY:
			if (UD::StringUtils::IsWS(CurrVal))
			{
				KeyEnd=CurrPos;
				CurrState=PS_SEPSEARCH;
			}
			else if (CurrVal==Separator)
			{
				KeyEnd=CurrPos;
				CurrState=PS_VALSEARCH;
			}
			else if (CurrVal==';')
			{
				Consumer(KeyBegin,CurrPos,nullptr,nullptr);
				CurrState=PS_ZERO;
			}
			break;
		case PS_SEPSEARCH:
			if (CurrVal==Separator)
				CurrState=PS_VALSEARCH;
			else if (CurrVal==';')
			{
				Consumer(KeyBegin,KeyEnd,nullptr,nullptr);
				CurrState=PS_ZERO;
			}
			else if (!UD::StringUtils::IsWS(CurrVal))
			{
				//This is something weird, like: "Key1 Key2...". Let's skip Key1, and start again from Key2.
				KeyBegin=CurrPos;
				CurrState=PS_KEY;
			}
			break;
		case PS_VALSEARCH:
			if (!UD::StringUtils::IsWS(CurrVal))
			{
				ValueBegin=CurrPos;
				if (CurrVal=='"')
					CurrState=PS_QUOTEDVAL;
				else
					CurrState=PS_VAL;
			}
			else if (UD::StringUtils::IsNewLine(CurrVal))
			{
				//Empty value.
				Consumer(KeyBegin,KeyEnd,CurrPos,CurrPos);
				CurrState=PS_ZERO;
			}
			break;
		case PS_VAL:
			if ((UD::StringUtils::IsWS(CurrVal)) || (CurrVal==';'))
			{
				//End of value.
				Consumer(KeyBegin,KeyEnd,ValueBegin,CurrPos);
				CurrState=PS_ZERO;
			}
			break;
		case PS_QUOTEDVAL:
			if (CurrVal=='"')
			{
				//End of quoted value (ValueBegin points to the '"').
				Consumer(KeyBegin,KeyEnd,ValueBegin+1,CurrPos);
				CurrState=PS_ZERO;
			}
			break;
		default:
			break;
		}

		CurrPos++;
	}

	//We might have some work left.
	switch (CurrState)
	{
	case PS_VAL:
		//Output the last value.
		Consumer(KeyBegin,KeyEnd,ValueBegin,CurrPos);
		break;
	case PS_QUOTEDVAL:
		//Output the last value (quoted, but quote not closed).
		Consumer(KeyBegin,KeyEnd,ValueBegin+1,CurrPos);
		break;
	default:
		break;
	}
}

template<class ParamConsumerType>
inline void ParseDelimitedKeyValue(const char *Source, ParamConsumerType Consumer) { ParseDelimitedKeyValue<ParamConsumerType,char,'='>(Source,Consumer); }


struct SingleKeyExtractor
{
	SingleKeyExtractor(const std::string &KeyName, std::string &ValTargetStr) : Key(KeyName), Target(ValTargetStr)
	{ }

	void operator()(const char *KeyBegin, const char *KeyEnd, const char *ValueBegin, const char *ValueEnd)
	{
		if ((ValueBegin) && (UD::StringUtils::IsEqual(Key,KeyBegin,KeyEnd)))
			Target.assign(ValueBegin,ValueEnd);
	}

private:
	const std::string &Key;
	std::string &Target;
};


struct ContentDisposExtractor
{
	ContentDisposExtractor(std::string &NameStr, std::string &FNStr) : NameTarget(NameStr), FNTarget(FNStr)
	{ }

	void operator()(const char *KeyBegin, const char *KeyEnd, const char *ValueBegin, const char *ValueEnd)
	{
		if (ValueBegin)
		{
			if (UD::StringUtils::IsEqual(NameKey,KeyBegin,KeyEnd))
				NameTarget.assign(ValueBegin,ValueEnd);
			else if (UD::StringUtils::IsEqual(FNKey,KeyBegin,KeyEnd))
				FNTarget.assign(ValueBegin,ValueEnd);
		}
	}

private:
	static const std::string NameKey, FNKey;
	std::string &NameTarget, &FNTarget;
};

const std::string ContentDisposExtractor::NameKey="name", ContentDisposExtractor::FNKey="filename";

const std::string Header::BoundaryParamName("boundary");

boost::unordered_map<std::string,HEADERNAME> Header::HeaderNameMap;
bool Header::IsHeaderNameInit=Header::InitHeaderNameMap();

Header::Header(char *LineBegin, char *LineEnd)
{
	if (ExtractHeader(LineBegin,LineEnd,(char **)&Name,(char **)&Value))
		IntName=ParseHeader(Name);
	else
		IntName=HN_UNKNOWN;
}

Header::Header(char *LineBegin, char *LineEnd, SkipParseOption) : IntName(HN_UNKNOWN)
{
	ExtractHeader(LineBegin,LineEnd,(char **)&Name,(char **)&Value);
}

CONTENTTYPE Header::GetContentType(std::string &OutBoundary) const
{
	if (IntName!=HN_CONTENT_TYPE)
		return CT_UNKNOWN;

	if (const char *SCPos=strchr(Value,';'))
	{
		if (UD::StringUtils::CmpI("multipart/form-data",Value,SCPos)==0)
		{
			OutBoundary.clear();
			ParseDelimitedKeyValue(SCPos,SingleKeyExtractor(BoundaryParamName,OutBoundary));
			return !OutBoundary.empty() ? CT_FORM_MULTIPART : CT_UNKNOWN;
		}
	}
	else
	{
		if (_stricmp(Value,"application/x-www-form-urlencoded")==0)
			return CT_URL_ENCODED;
	}

	return CT_UNKNOWN;
}

void Header::ParseContentDisposition(std::string &OutName, std::string &OutFileName) const
{
	if (IntName==HN_CONTENT_DISPOSITION)
		ParseDelimitedKeyValue(Value,ContentDisposExtractor(OutName,OutFileName));
	else
	{
		OutName.clear();
		OutFileName.clear();
	}
}

bool Header::IsConnectionClose() const
{
	return strcmp(Value,"close")==0;
}

time_t Header::GetDateTime() const
{
	//                       0         1         2
	//                       01234567890123456789012345678
	//Sample RFC 1123 date: "Sun, 06 Nov 1994 08:49:37 GMT", strlen: 29
	//WDay values: "Mon" | "Tue" | "Wed" | "Thu" | "Fri" | "Sat" | "Sun"
	//Month values: "Jan" | "Feb" | "Mar" | "Apr" | "May" | "Jun" | "Jul" | "Aug" | "Sep" | "Oct" | "Nov" | "Dec"

	if (strlen(Value)!=DateStringLength)
		throw FormatException("DateTime has invalid length");

	tm TM;
	TM.tm_isdst=0; TM.tm_wday=0; TM.tm_yday=0;
	TM.tm_mday=atoi(Value + 5);
	TM.tm_year=atoi(Value + 12) - 1900;
	TM.tm_hour=atoi(Value + 17);
	TM.tm_min=atoi(Value + 20);
	TM.tm_sec=atoi(Value + 23);

	TM.tm_mon=-1;
	switch (*(Value + 8))
	{
	case 'A': //Apr, Aug
		if (*(Value + 9)=='p')
			TM.tm_mon=3; //Apr
		else if (*(Value + 9)=='u')
			TM.tm_mon=7; //Aug
		break;
	case 'D': //Dec
		TM.tm_mon=11;
		break;
	case 'F': //Feb
		TM.tm_mon=2;
		break;
	case 'J': //Jan, Jun, Jul
		if (*(Value + 9)=='a')
			TM.tm_mon=0; //Jan
		else if (*(Value + 10)=='l')
			TM.tm_mon=6; //Jul
		else if (*(Value + 10)=='n')
			TM.tm_mon=5; //Jun
		break;
	case 'M': //Mar, May
		if (*(Value + 10)=='r')
			TM.tm_mon=2; //Mar
		else if (*(Value + 10)=='y')
			TM.tm_mon=4; //May
		break;
	case 'N': //Nov
		TM.tm_mon=10;
		break;
	case 'O': //Oct
		TM.tm_mon=9;
		break;
	case 'S': //Sep
		TM.tm_mon=8;
		break;
	}

	if ((TM.tm_year==0) || (TM.tm_mon==1))
		throw FormatException("DateTime is invalid");

	time_t RetTime=UD::TimeUtils::TimeGM(&TM);
	if (RetTime!=-1)
		return RetTime;
	else
		throw FormatException("DateTime is invalid");
}

long long Header::GetLongLong() const
{
	char *EndPtr;
#ifdef _MSC_VER
	long long RetVal=_strtoi64(Value,&EndPtr,10);
#else
	long long RetVal=strtoll(Value,&EndPtr,10);
#endif
	if (EndPtr!=Value)
		return RetVal;
	else
		throw FormatException("Cannot parse LongLong value");
}

unsigned long long Header::GetULongLong() const
{
	char *EndPtr;
#ifdef _MSC_VER
	unsigned long long RetVal=_strtoui64(Value,&EndPtr,10);
#else
	unsigned long long RetVal=strtoull(Value,&EndPtr,10);
#endif
	if (EndPtr!=Value)
		return RetVal;
	else
		throw FormatException("Cannot parse ULongLong value");
}

HEADERNAME Header::ParseHeader(const char *Name)
{
	boost::unordered_map<std::string,HEADERNAME>::const_iterator FindI=HeaderNameMap.find(Name);
	if (FindI!=HeaderNameMap.end())
		return FindI->second;
	else
		return HN_UNKNOWN;
}

const std::string &Header::GetHeaderName(HEADERNAME Name)
{
	switch (Name)
	{
	case HN_HOST: { static const std::string HeaderStr("host"); return HeaderStr; }
	case HN_CONNECTION: { static const std::string HeaderStr("connection"); return HeaderStr; }
	case HN_CONTENT_DISPOSITION: { static const std::string HeaderStr("content-disposition"); return HeaderStr; }
	case HN_CONTENT_TYPE: { static const std::string HeaderStr("content-type"); return HeaderStr; }
	case HN_CONTENT_LENGTH: { static const std::string HeaderStr("content-length"); return HeaderStr; }
	case HN_CONTENT_ENCODING: { static const std::string HeaderStr("content-encoding"); return HeaderStr; }
	case HN_USER_AGENT: { static const std::string HeaderStr("user-agent"); return HeaderStr; }
	case HN_IF_MOD_SINCE: { static const std::string HeaderStr("if-modified-since"); return HeaderStr; }
	case HH_ORIGIN: { static const std::string HeaderStr("origin"); return HeaderStr; }
	case HH_UPGRADE: { static const std::string HeaderStr("upgrade"); return HeaderStr; }
	case HH_SEC_WEBSOCKET_KEY: { static const std::string HeaderStr("sec-websocket-key"); return HeaderStr; }
	case HH_SEC_WEBSCOKET_VERSION: { static const std::string HeaderStr("sec-websocket-version"); return HeaderStr; }
	case HH_SEC_WEBSOCKET_PROTOCOL: { static const std::string HeaderStr("sec-websocket-protocol"); return HeaderStr; }
	case HN_LAST_MODIFIED: { static const std::string HeaderStr("last-modified"); return HeaderStr; }
	case HN_LOCATION: { static const std::string HeaderStr("location"); return HeaderStr; }
	case HH_SEC_WEBSOCKET_ACCEPT: { static const std::string HeaderStr("sec-websocket-accept"); return HeaderStr; }
	default: { static const std::string HeaderStr(""); return HeaderStr; }
	}
}

char *Header::ExtractHeader(char *LineBegin, char *LineEnd, char **OutName, char **OutValue)
{
	*OutName=LineBegin;
	*OutValue=nullptr;
	while (LineBegin!=LineEnd)
	{
		char CurrVal=*LineBegin;
		if ((CurrVal>='A') && (CurrVal<='Z'))
			*LineBegin=CurrVal-'A'+'a';
		else if (CurrVal==':')
		{
			*LineBegin='\0';
			LineBegin++;
			break;
		}

		LineBegin++;
	}

	while (LineBegin!=LineEnd)
	{
		if (*LineBegin!=' ')
		{
			*OutValue=LineBegin;
			break;
		}
		else
			LineBegin++;
	}

	while (LineBegin!=LineEnd)
	{
		if (*LineBegin=='\r')
		{
			*LineBegin++='\0';
			return LineBegin;
		}
		else
			LineBegin++;
	}

	return nullptr;
}

void Header::FormatDateTime(time_t Timestamp, char *TargetBuff)
{
	tm TM;
	if (!UD::TimeUtils::GMTime(&Timestamp,&TM))
		strftime(TargetBuff,30,"%a, %d %b %Y %H:%M:%S GMT",&TM);
	else
		*TargetBuff='\0';
}

bool Header::InitHeaderNameMap()
{
	for (unsigned int x=HN_UNKNOWN + 1; x!=HN_NOTUSED; ++x)
		AddToHeaderMap((HEADERNAME)x);

	return true;
}
