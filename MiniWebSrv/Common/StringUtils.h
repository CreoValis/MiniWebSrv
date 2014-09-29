#pragma once

#include <string>
#include <locale>

namespace UD
{

namespace StringUtils
{

template<class CharType>
bool IsWS(CharType TestVal) { return (TestVal=='\r') || (TestVal=='\n') || (TestVal==' ') || (TestVal=='\t'); }
template<class CharType>
bool IsSpace(CharType TestVal) { return (TestVal==' ') || (TestVal=='\t'); }
template<class CharType>
bool IsNewLine(CharType TestVal) { return (TestVal=='\r') || (TestVal=='\n'); }

inline bool IsEqual(const std::string &TestStr, const char *CmpBegin, const char *CmpEnd)
{
	return (TestStr.length()==(std::string::size_type)(CmpEnd-CmpBegin)) && (std::equal(TestStr.begin(),TestStr.end(),CmpBegin));
}

inline int CmpI(const char *Op1, const char *Op2Begin, const char *Op2End)
{
	char Op1Val, Op2Val;
	while ((Op1Val=tolower(*Op1++)) && (Op2Begin!=Op2End))
	{
		Op2Val=tolower(*Op2Begin++);
		if (Op1Val<Op2Val)
			return -1;
		if (Op1Val>Op2Val)
			return 1;
	}

	if (!Op1Val)
	{
		if (Op2Begin==Op2End)
			return 0;
		else
			return -1;
	}
	else
		return 1;
}

template<class ClassifierType, class CharType>
void Trim(const CharType **Begin, const CharType **End, ClassifierType Classifier)
{
	const CharType *TestPos=*Begin, *TestEnd=*End;

	while (TestPos!=TestEnd)
	{
		if (Classifier(*TestPos))
			++TestPos;
		else
			break;
	}

	while (TestPos!=TestEnd)
	{
		const CharType *CurrPos=TestEnd-1;
		if (Classifier(*CurrPos))
			TestEnd=CurrPos;
		else
			break;
	}

	*Begin=TestPos;
	*End=TestEnd;
}

template<class CharType>
inline void TrimWS(const CharType **Begin, const CharType **End)
{
	Trim(Begin,End,&IsWS<CharType>);
}

template<class TokenConsumerType, class CharType>
struct TrimWSTokenConsumer
{
	inline TrimWSTokenConsumer(TokenConsumerType NewCons) : MyCons(NewCons)
	{ }

	void operator()(const CharType *Begin, const CharType *End)
	{
		TrimWS(&Begin,&End);
		if (Begin!=End)
			MyCons(Begin,End);
	}

private:
	TokenConsumerType MyCons;
};

/**Extacts tokens from the given string. The tokens are expected to be separated with Separator.
The returned tokens are not trimmed automatically.
@param ParamConsumerType A functor with the following signature:
	Func(const CharType *Begin, const CharType *End)*/
template<class TokenConsumerType, class CharType>
void ExtractTokens(const CharType *Source, CharType Separator, TokenConsumerType Consumer)
{
	const CharType *Begin=Source;
	while (CharType CurrVal=*Source)
	{
		if (CurrVal==Separator)
		{
			if (Begin!=Source)
				Consumer(Begin,Source);

			++Source;
			Begin=Source;
		}
		else
			++Source;
	}

	if (Begin!=Source)
		Consumer(Begin,Source);
}

/**Extacts tokens from the given string.*/
template<class TokenConsumerType>
inline void ExtractTokens(const char *Source, char Separator, TokenConsumerType Consumer) { ExtractTokens<TokenConsumerType,char>(Source,Separator,Consumer); }

/**Extacts tokens from the given string, trimming them before they reach the given Consumer object.*/
template<class TokenConsumerType>
inline void ExtractTrimWSTokens(const char *Source, char Separator, TokenConsumerType Consumer)
{
	ExtractTokens(Source,Separator,TrimWSTokenConsumer<TokenConsumerType,char>(Consumer));
}

/**Parses a string containing key=value pair. Supports value quoting, without quote escape.
@param ParamConsumerType A functor with the following signature:
	Func(const CharType *KeyBegin, const CharType *KeyEnd, const CharType *ValueBegin, const CharType *ValueEnd)
*/
template<class ParamConsumerType, class CharType, CharType Separator>
void ParseKeyValue(const CharType *Source, ParamConsumerType Consumer)
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
			if (!IsWS(CurrVal))
			{
				KeyBegin=CurrPos;
				CurrState=PS_KEY;
			}
			break;
		case PS_KEY:
			if (IsWS(CurrVal))
			{
				KeyEnd=CurrPos;
				CurrState=PS_SEPSEARCH;
			}
			else if (CurrVal==Separator)
			{
				KeyEnd=CurrPos;
				CurrState=PS_VALSEARCH;
			}
			break;
		case PS_SEPSEARCH:
			if (CurrVal==Separator)
				CurrState=PS_VALSEARCH;
			else if (!IsWS(CurrVal))
			{
				//This is something weird, like: "Key1 Key2...". Let's skip Key1, and start again from Key2.
				KeyBegin=CurrPos;
				CurrState=PS_KEY;
			}
			break;
		case PS_VALSEARCH:
			if (!IsWS(CurrVal))
			{
				ValueBegin=CurrPos;
				if (CurrVal=='"')
					CurrState=PS_QUOTEDVAL;
				else
					CurrState=PS_VAL;
			}
			else if (IsNewLine(CurrVal))
			{
				//Empty value.
				Consumer(KeyBegin,KeyEnd,CurrPos,CurrPos);
				CurrState=PS_ZERO;
			}
			break;
		case PS_VAL:
			if (IsWS(CurrVal))
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
inline void ParseKeyValue(const char *Source, ParamConsumerType Consumer) { ParseKeyValue<ParamConsumerType,char,'='>(Source,Consumer); }

}; //StringUtils

}; //UD
