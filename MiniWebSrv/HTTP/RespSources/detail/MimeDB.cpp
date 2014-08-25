#include "MimeDB.h"

const char *HTTP::detail::MimeDB::UnknownMimeType="application/octet-stream";
boost::unordered_map<std::string,std::string> HTTP::detail::MimeDB::ExtMimeMap;
bool HTTP::detail::MimeDB::IsExtMimeMapInit=InitExtMimeMap();

const char *HTTP::detail::MimeDB::GetMimeType(const std::string &FileExt)
{
	boost::unordered_map<std::string,std::string>::const_iterator FindI=ExtMimeMap.find(FileExt);
	if (FindI!=ExtMimeMap.end())
		return FindI->second.data();
	else
		return UnknownMimeType;
}

bool HTTP::detail::MimeDB::InitExtMimeMap()
{
	ExtMimeMap[".gz"]="application/gzip";
	ExtMimeMap[".js"]="application/javascript";
	ExtMimeMap[".json"]="application/json";
	ExtMimeMap[".pdf"]="application/pdf";
	ExtMimeMap[".rtf"]="application/rtf";
	ExtMimeMap[".zip"]="application/zip";
	ExtMimeMap[".xml"]="application/xml";

	ExtMimeMap[".jpg"]="image/jpeg";
	ExtMimeMap[".jpeg"]="image/jpeg";
	ExtMimeMap[".png"]="image/png";
	ExtMimeMap[".gif"]="image/gif";
	ExtMimeMap[".svg"]="image/svg+xml";

	ExtMimeMap[".css"]="text/css";
	ExtMimeMap[".csv"]="text/csv";
	ExtMimeMap[".htm"]="text/html";
	ExtMimeMap[".html"]="text/html";
	ExtMimeMap[".frame"]="text/html";
	ExtMimeMap[".txt"]="text/plain";

	return true;
}
