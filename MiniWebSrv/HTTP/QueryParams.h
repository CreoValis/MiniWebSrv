#pragma once

#include <tuple>
#include <string>
#include <map>
#include <stdexcept>
#include <fstream>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

namespace HTTP
{

class QueryParams
{
public:
	/**Interface to handle uploaded files. It's methods used in the following order:
	* - OnNewFile() //Called when a new file is about to be received.
	* - OnFileData() //Called with the current file's data.
	* - ... //multiple OnFileData() calls.
	* - OnFileEnd() //Called after the last OnFileData() call.
	*/
	class IFileUpdloadHelper
	{
	public:
		/**Called when a new file's data is about to be received.
		@return A path identifying the new file, and the result code. The path will be used to fill the Path member
			of the File structure.*/
		virtual std::tuple<boost::filesystem::path, bool> OnNewFile(const std::string &Name, const std::string &OrigFileName, const std::string &MimeType)=0;
		virtual bool OnFileData(const char *Begin, const char *End)=0;
		virtual void OnFileEnd()=0;
	};

	/**Parameters for the default temp file upload handler.*/
	struct FileUploadParams
	{
		inline FileUploadParams() : MaxUploadSize(~(uintmax_t)0), MaxTotalUploadSize(~(uintmax_t)0)
		{ }

		inline FileUploadParams(uintmax_t MaxUploadSize, uintmax_t MaxTotalUploadSize) : MaxUploadSize(MaxUploadSize), MaxTotalUploadSize(MaxTotalUploadSize)
		{ }

		///Target directory to save uploaded files to. If not specified, the files will be saved to the temp directory.
		boost::filesystem::path Root;
		///Maximum uploaded file size. If a single file's size grows beyond this limit, the parsing will be aborted.
		uintmax_t MaxUploadSize;
		/**Maximum cumulative uploaded file size. If the total size of all uploaded files grow beyond this limit, the
		parsing will be aborted.*/
		uintmax_t MaxTotalUploadSize;
	};

	QueryParams();
	QueryParams(QueryParams &&other);
	QueryParams(const std::tuple<const char *, const char *> &URLEncodedRange);
	/**Constructs the object with the default file upload handler. It will save the uploaded files into the system's
	temp directory, or the one specified by UploadParams.Root .*/
	QueryParams(const FileUploadParams &UploadParams);
	/**Constructs the object with a custom file upload handler. The handler object will not be owned by this object.*/
	QueryParams(IFileUpdloadHelper *UploadHelper);

	class ParameterNotFound : public std::runtime_error { public: ParameterNotFound() : std::runtime_error("ParameterNotFound") { } };

	struct File
	{
		inline File() { }
		inline File(const boost::filesystem::path &FN) : Path(FN) { }

		inline operator const void *() const
		{
			try { return boost::filesystem::is_empty(Path) ? nullptr : this; }
			catch (...) { return nullptr; }
		}

		std::string MimeType, OrigFileName;
		boost::filesystem::path Path;
	};

	typedef std::map<std::string,std::string> ParamMapType;
	typedef std::map<std::string,File> FileMapType;

	inline bool AddURLEncoded(const std::tuple<const char *, const char *> &Range) { return AddURLEncoded(std::get<0>(Range), std::get<1>(Range)); }
	bool AddURLEncoded(const char *Begin, const char *End);
	/**Can be called multiple times. The boundary string must be set beforehand.*/
	bool AppendFormMultipart(const char *Begin, const char *End);

	inline const ParamMapType &Params() const { return ParamMap; }
	inline const FileMapType &Files() const { return FileMap; }

	inline std::string &GetBoundaryStr() { return BoundaryStr; }
	void OnBoundaryParsed();

	void DeleteUploadedFiles();

	/**Throws ParameterNotFound if the given parameter is not present.*/
	const std::string &Get(const std::string &Name) const;
	const std::string *GetPtr(const std::string &Name) const;
	const std::string Get(const std::string &Name, const std::string &Default="") const;

	QueryParams &operator=(QueryParams &&other);

	static void DecodeURLEncoded(std::string &Target, const char *Begin, const char *End);

protected:
	class TempFileUploadHelper : public IFileUpdloadHelper
	{
	public:
		inline TempFileUploadHelper() : CurrFileS(nullptr), CurrFileSize(0), TotalFileSize(0)
		{ }
		inline TempFileUploadHelper(const TempFileUploadHelper &other) : Params(other.Params), CurrFileS(nullptr), CurrFileSize(0), TotalFileSize(0)
		{ }
		inline TempFileUploadHelper(FileUploadParams Params) : Params(Params), CurrFileS(nullptr), CurrFileSize(0), TotalFileSize(0)
		{ }

		virtual std::tuple<boost::filesystem::path, bool> OnNewFile(const std::string &Name, const std::string &OrigFileName, const std::string &MimeType) override;
		virtual bool OnFileData(const char *Begin, const char *End) override;
		virtual void OnFileEnd() override;

	private:
		FileUploadParams Params;

		boost::filesystem::path CurrFN;
		boost::filesystem::ofstream *CurrFileS;

		uintmax_t CurrFileSize, TotalFileSize;
	};

	enum
	{
		STATE_HEADERSTART, //Searching for "\r\n" after the boundary.
		STATE_HEADERS_END, //Searching for headers end ("\r\n\r\n").
		STATE_READ, //Reading part.
		STATE_FINISHED, //Multipart stream finished (final part found).
	} FMParseState;
	unsigned int BoundaryParseCounter, FMParseCounter;

	TempFileUploadHelper TempUploadHelper;

	std::string BoundaryStr;

	std::string ParseTmp, HeaderParseTmp;
	ParamMapType ParamMap;
	FileMapType FileMap;

	union
	{
		std::string *TargetParam;
		File *TargetFile;
	} CurrFMPart;
	enum
	{
		PARTMODE_STRING,
		PARTMODE_FILE,
	} CurrPartMode;

	IFileUpdloadHelper *UploadHelper;

	static const std::string UnknownFileContentType;

	void StartNewPart();
	bool AppendToCurrentPart(const char *Begin, const char *End);
	bool ParseFMHeaders(const char *HeadersBegin, const char *HeadersEnd);

	inline bool IsCurrPartFile() const { return CurrPartMode==PARTMODE_FILE; }
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

} //HTTP
