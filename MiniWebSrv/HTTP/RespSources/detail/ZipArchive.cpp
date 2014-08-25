#include "ZipArchive.h"

#include <time.h>

ZipArchive::Stream::Stream(const FileInfo *NewInfo, std::string &ArchiveName) : Info(NewInfo), CompPos(0)
{
	InS.open(ArchiveName.data(),std::ios_base::binary);
	InS.seekg(NewInfo->LocalHeaderOffset);
	if (InS.fail())
		throw Exception("Cannot seek to file");

	char ReadBuff[30];
	InS.read(ReadBuff,sizeof(ReadBuff));
	if ((InS.fail()) || (*(const unsigned int *)ReadBuff!=LFHMagic))
		throw Exception("Invalid zip file");

	unsigned int FNLength=*(const unsigned short *)(ReadBuff+26);
	unsigned int ExtraLength=*(const unsigned short *)(ReadBuff+28);

	InS.seekg(FNLength+ExtraLength,std::ios_base::cur);
	if (InS.fail())
		throw Exception("Cannot seek to file");
}

unsigned int ZipArchive::Stream::Read(char *Target, unsigned int Size)
{
	unsigned int CompSize=Info->CompressedSize;
	if (CompPos<CompSize)
	{
		unsigned int RemSize=CompSize-CompPos;
		if (Size>RemSize)
			Size=RemSize;

		InS.read(Target,Size);
		CompPos+=Size;
		return Size;
	}
	else
		return 0;
}

ZipArchive::ZipArchive(const std::string &FileName) : ZipFN(FileName)
{
	ZipS.open(FileName.data(),std::ios_base::binary);
	if (!ZipS.is_open())
		throw Exception("Cannot open archive");

	Load(ZipS,FileInfoMap);
}

const ZipArchive::FileInfo *ZipArchive::Get(const std::string &FileName)
{
	FIMapType::const_iterator FindI=FileInfoMap.find(FileName);
	if (FindI!=FileInfoMap.end())
		return &FindI->second;
	else
		return nullptr;
}

ZipArchive::Stream *ZipArchive::GetStream(const FileInfo *Info, bool Decompress)
{
	if (!Info)
		throw Exception("Invalid FileInfo");

	if (Decompress)
		throw Exception("Decompress is not implemented");

	return new Stream(Info,ZipFN);
}

void ZipArchive::Load(std::ifstream &Source, FIMapType &Target)
{
	Source.seekg(-22,std::ios_base::end);

	char ReadBuff[256];
	Source.read(ReadBuff,22);
	//TODO: handle non-zero length comments.
	if ((Source.fail()) || (*(const unsigned int *)ReadBuff!=EOCDMagic))
		throw Exception("Not a zip archive");

	unsigned short CDCount=*(const unsigned short *)(ReadBuff+10);
	unsigned int CDOffset=*(const unsigned int *)(ReadBuff+16);

	Source.seekg(CDOffset,std::ios_base::beg);
	if (Source.fail())
		throw Exception("Not a zip archive");

	std::string CurrFN;
	while (CDCount)
	{
		Source.read(ReadBuff,46);
		if ((*(const unsigned int *)ReadBuff!=CDMagic))
			throw Exception("Error in archive central directory");

		FileInfo CurrFile;
		{
			unsigned short ModTime=*(const unsigned short *)(ReadBuff+12);
			unsigned short ModDate=*(const unsigned short *)(ReadBuff+14);

			tm FileModTime;
			FileModTime.tm_year=(ModDate >> 9)+1980-1900;
			FileModTime.tm_mon=((ModDate >> 5) & 0xF)-1;
			FileModTime.tm_mday=((ModDate >> 0) & 0x1F);
			FileModTime.tm_hour=(ModTime >> 11);
			FileModTime.tm_min=((ModTime >> 5) & 0x3F);
			FileModTime.tm_sec=((ModTime >> 0) & 0x1F)*2;
			CurrFile.LastModTime=mktime(&FileModTime);
		}

		CurrFile.Compression=GetCompressionMethod(*(const unsigned short *)(ReadBuff+10));
		CurrFile.CRC32=*(const unsigned int *)(ReadBuff+16);
		CurrFile.CompressedSize=*(const unsigned int *)(ReadBuff+20);
		CurrFile.UncompressedSize=*(const unsigned int *)(ReadBuff+24);
		CurrFile.LocalHeaderOffset=*(const unsigned int *)(ReadBuff+42);

		unsigned short FNLength=*(const unsigned short *)(ReadBuff+28);
		unsigned int ExtraLength=*(const unsigned short *)(ReadBuff+30);
		unsigned int CommentLength=*(const unsigned short *)(ReadBuff+32);

		{
			CurrFN.resize(0);
			CurrFN.reserve(FNLength);

			while (FNLength)
			{
				if (FNLength<sizeof(ReadBuff))
				{
					Source.read(ReadBuff,FNLength);
					CurrFN.append(ReadBuff,FNLength);
					FNLength=0;
				}
				else
				{
					Source.read(ReadBuff,sizeof(ReadBuff));
					CurrFN.append(ReadBuff,sizeof(ReadBuff));
					FNLength-=sizeof(ReadBuff);
				}
			}
		}

		if ((!CurrFN.empty()) && (CurrFN.at(CurrFN.length()-1)!='/'))
			Target[CurrFN]=CurrFile;

		ExtraLength+=CommentLength;
		if (ExtraLength)
			Source.seekg(ExtraLength,std::ios_base::cur);

		CDCount--;
	}
}

ZipArchive::COMPRESSIONMETHOD ZipArchive::GetCompressionMethod(unsigned short Code)
{
	switch (Code)
	{
	case CM_STORE:
	case CM_DEFLATE:
		return (COMPRESSIONMETHOD)Code;
	default:
		throw Exception("Unsupported compression method");
	}
}
