#pragma once

#include <string>
#include <map>
#include <stdexcept>
#include <fstream>

#include <boost/filesystem.hpp>

namespace HTTP
{

class QueryParams
{
public:
	struct FileUploadParams
	{
		inline FileUploadParams() : MaxUploadSize(~(uintmax_t)0)
		{ }

		boost::filesystem::path Root;
		uintmax_t MaxUploadSize;
	};

	QueryParams();
	QueryParams(const FileUploadParams &UploadParams);

	class ParameterNotFound : public std::runtime_error { public: ParameterNotFound() : std::runtime_error("ParameterNotFound") { } };

	struct Param
	{
		inline Param() { }
		inline Param(const std::string &Val) : Value(Val){ }

		inline operator const std::string &() const { return Value; }

		std::string Value;
	};

	struct File
	{
		inline File() { }
		inline File(const boost::filesystem::path &FN) : Path(FN) { }

		std::string MimeType, OrigFileName;
		boost::filesystem::path Path;
	};

	typedef std::map<std::string,Param> ParamMapType;
	typedef std::map<std::string,File> FileMapType;

	bool AddURLEncoded(const char *Begin, const char *End);
	/**Can be called multiple times.*/
	bool AppendFormMultipart(const char *Begin, const char *End);

	inline const ParamMapType &Params() const { return ParamMap; }
	inline const FileMapType &Files() const { return FileMap; }

	inline std::string &GetBoundaryStr() { return BoundaryStr; }
	void OnBoundaryParsed();

	void DeleteUploadedFiles();

	/**Throws ParameterNotFound if the given parameter is not present.*/
	const Param &Get(const std::string &Name) const;
	const Param Get(const std::string &Name, const std::string &Default="") const;

protected:
	enum
	{
		STATE_HEADERSTART, //Searching for "\r\n" after the boundary.
		STATE_HEADERS_END, //Searching for headers end ("\r\n\r\n").
		STATE_READ, //Reading part.
		STATE_FINISHED, //Multipart stream finished (final part found).
	} FMParseState;
	unsigned int BoundaryParseCounter, FMParseCounter;

	FileUploadParams FUParams;
	std::string BoundaryStr;

	std::string ParseTmp, HeaderParseTmp;
	ParamMapType ParamMap;
	FileMapType FileMap;

	union
	{
		Param *TargetParam;
		File *TargetFile;
	} CurrFMPart;
	std::ofstream *CurrFileS;

	void StartNewPart();
	bool AppendToCurrentPart(const char *Begin, const char *End);
	bool ParseFMHeaders(const char *HeadersBegin, const char *HeadersEnd);

	inline bool IsCurrPartFile() const { return CurrFileS!=nullptr; }
	inline bool IsCurrPartValid() const { return CurrFMPart.TargetFile!=nullptr; }

	static unsigned char GetHexVal(char Val)
	{
		if ((Val>='0') && (Val<='9'))
			return Val-'0';
		if ((Val>='A') && (Val<='F'))
			return Val-'A'+10;
		if ((Val>='a') && (Val<='f'))
			return Val-'a'+10;
		else
			return 0;
	}
};

};
