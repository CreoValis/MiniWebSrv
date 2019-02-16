#pragma once

#include <stdint.h>
#include <boost/filesystem.hpp>

namespace HTTP
{

namespace Config
{

/**Parameters for the default temp file upload handler.*/
struct FileUpload
{
	inline FileUpload() : MaxUploadSize(~(uintmax_t)0), MaxTotalUploadSize(~(uintmax_t)0)
	{ }

	inline FileUpload(uintmax_t MaxUploadSize, uintmax_t MaxTotalUploadSize) : MaxUploadSize(MaxUploadSize), MaxTotalUploadSize(MaxTotalUploadSize)
	{ }

	///Target directory to save uploaded files to. If not specified, the files will be saved to the temp directory.
	boost::filesystem::path Root;
	///Maximum uploaded file size. If a single file's size grows beyond this limit, the parsing will be aborted.
	uintmax_t MaxUploadSize;
	/**Maximum cumulative uploaded file size. If the total size of all uploaded files grow beyond this limit, the
	parsing will be aborted.*/
	uintmax_t MaxTotalUploadSize;
};

} //Config

} //HTTP
