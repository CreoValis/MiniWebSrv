#pragma once

#include <fstream>
#include <boost/unordered_map.hpp>

class ZipArchive
{
public:
	ZipArchive(const std::string &FileName);

	class Exception : public std::runtime_error
	{
	public:
		inline Exception(const char *Msg) : std::runtime_error(Msg) { }
	};

	enum COMPRESSIONMETHOD
	{
		CM_STORE     = 0,
		CM_DEFLATE   = 8,
	};

	struct FileInfo
	{
		COMPRESSIONMETHOD Compression;
		unsigned int CRC32;
		std::size_t LocalHeaderOffset;
		std::size_t CompressedSize, UncompressedSize;
		time_t LastModTime;
	};

	class Stream
	{
	public:
		Stream(const FileInfo *NewInfo, std::string &ArchiveName);
		Stream() { }

		inline const FileInfo *GetInfo() const { return Info; }
		unsigned int Read(char *Target, unsigned int Size);

	private:
		static const unsigned int LFHMagic = 0x04034b50;

		const FileInfo *Info;
		std::ifstream InS;
		unsigned int CompPos;
	};

	const FileInfo *Get(const std::string &FileName);
	Stream *GetStream(const FileInfo *Info, bool Decompress=true);

protected:
	static const unsigned int CDMagic = 0x02014b50;
	static const unsigned int EOCDMagic = 0x06054b50;

	//FileName->FileInfo map.
	typedef boost::unordered_map<std::string,FileInfo> FIMapType;
	boost::unordered_map<std::string,FileInfo> FileInfoMap;

	std::string ZipFN;
	std::ifstream ZipS;

	void Load(std::ifstream &Source, FIMapType &Target);

	static COMPRESSIONMETHOD GetCompressionMethod(unsigned short Code);
};
