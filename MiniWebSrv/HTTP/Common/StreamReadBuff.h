#pragma once

#include <memory.h>
#include <string.h>

#include "BinUtils.h"

namespace UD
{

namespace Comm
{

/**Helper class to support the async reader state machines in Connection
classes.
The class has a built-in static buffer, which is used until the required buffer
size is smaller than or equal to StaticReadBuffSize . Above this
size, a dynamically allocated buffer is used. This buffer is only released when
the object is destroyed. The buffer also never shrinks; it can only grow.*/
template<unsigned int StaticReadBuffSize>
class StreamReadBuff
{
public:
	StreamReadBuff() : ReadBuff(StaticBuff), ReadBuffEnd(StaticBuff+sizeof(StaticBuff)),
		ReadPos(0), RelevantLength(0), DataEndPos(0), ReqDataEndPos(0)
	{ }
	~StreamReadBuff()
	{
		if (ReadBuff!=StaticBuff)
			delete[] ReadBuff;
	}

	/**Discards the current content and state of the object. The internal
	buffer, if it's dinamically allocated, will be retained.*/
	void Reset()
	{
		ReadPos=0;
		RelevantLength=0;
		DataEndPos=0;
		ReqDataEndPos=0;
	}

	/**@return The number of bytes available to be consumed.*/
	inline unsigned int GetAvailableDataLength() const { return DataEndPos-ReadPos; }
	inline unsigned int GetRelevantDataLength() const { return RelevantLength; }
	/**Queries the buffer of currently kept, "relevant" data. This does not
	include data not yet consumed.*/
	inline const unsigned char *GetRelevantData(unsigned int &OutLength) const //From the start of relevant data to ReadPos.
	{
		OutLength=RelevantLength;
		return ReadBuff+ReadPos-RelevantLength;
	}

	/**Queries the buffer of bytes available to be consumed.*/
	inline const unsigned char *GetAvailableData(unsigned int &OutLength) const //From ReadPos to DataEndPos.
	{
		OutLength=DataEndPos-ReadPos;
		return ReadBuff+ReadPos;
	}

	/**Consumes a given number of bytes from the buffer, and optionally keeps
	them for further use.
	@param Length Number of bytes consumed.
	@param IsRelevant If true, Length number of bytes should be kept in the
		buffer.
	@return True, if the value of Length was correct (it was in bounds of the
		buffer). Otherwise, the method has consumed every available byte.*/
	inline bool Consume(unsigned int Length, bool IsRelevant=false)
	{
		if (Length<DataEndPos-ReadPos)
		{
			if (IsRelevant)
				RelevantLength+=Length;

			ReadPos+=Length;
			return true;
		}
		else
		{
			//TODO: do an assert, or throw an exception here!!!
			if (IsRelevant)
				RelevantLength+=DataEndPos-ReadPos;

			ReadPos=DataEndPos;
			return false;
		}
	}
	/**Clears the number of relevant bytes, releasing free space in the buffer.*/
	void ResetRelevant()
	{
		if ((RelevantLength) && (ReadPos))
		{
			//memmove(ReadBuff,ReadBuff+ReadPos,DataEndPos-ReadPos);
			//DataEndPos-=ReadPos;
			//ReqDataEndPos-=ReadPos;
			//ReadPos=0;
			RelevantLength=0;
		}
	}

	/**Ensures that the required number of bytes are available in the buffer
	after the relevant (kept) data.
	@param ReqDataLength Minimum number of bytes that should be available to
		consume.
	@return True, if there are enough bytes available to consume. Otherwise,
		it sets the number of requested bytes, and, if needed, resizes the buffer
		so enough space will be available for a future read.*/
	bool RequestData(unsigned int ReqDataLength)
	{
		unsigned int AvailableLength=GetAvailableDataLength();
		if (AvailableLength>=ReqDataLength)
			return true;

		ReqDataLength-=AvailableLength;

		ReqDataEndPos+=ReqDataLength;
		if (ReqDataEndPos>(unsigned int)(ReadBuffEnd-ReadBuff))
		{
			/*We might have to resize the read buffer. We don't need to, if we can
			make room by moving data in back in ReadBuff. This requires that the
			space before the relevant data (ReadPos-RelevantLength) plus any
			available space after DataEndPos ((ReadBuffEnd-ReadBuff)-DataEndPos) is
			greater than, or equal to the required number of free bytes.*/
			unsigned int
				BuffLength=(unsigned int)(ReadBuffEnd-ReadBuff),
				FrontSpace=ReadPos-RelevantLength,
				EndSpace=BuffLength-DataEndPos,
				ReqNewLength=ReqDataEndPos-DataEndPos;
			if (ReqNewLength<=FrontSpace+EndSpace)
			{
				//We only have to move our data by FrontSpace bytes.
				memmove(ReadBuff,ReadBuff+FrontSpace,DataEndPos-FrontSpace);
			}
			else
			{
				//We have to re-allocate the buffer.
				unsigned int NewBuffSize=BuffLength-FrontSpace-EndSpace+ReqNewLength;
				NewBuffSize=ROUNDUP_POWEROFTWO(NewBuffSize,BUFFER_ROUNDUP_TARGET);
				unsigned char *NewBuff=new unsigned char[NewBuffSize];

				memcpy(NewBuff,ReadBuff+FrontSpace,DataEndPos-FrontSpace);

				if (ReadBuff!=StaticBuff)
					delete[] ReadBuff;

				ReadBuff=NewBuff;
				ReadBuffEnd=NewBuff+NewBuffSize;
			}

			ReadPos-=FrontSpace;
			DataEndPos-=FrontSpace;
			ReqDataEndPos-=FrontSpace;
		}

		return false;
	}
	/**Requests information about the number of bytes that must be / should be
	read into the buffer.
	@param OutReqLength Number of bytes that _must_ be read into the returned
		buffer.
	@param OutFreeLength Number of bytes that _can_ be read into the returned
		buffer. (OutFreeLength>=OutReqLength) is always true.
	@return Pointer to the buffer to read to.*/
	unsigned char *GetReadInfo(unsigned int &OutReqLength, unsigned int &OutFreeLength)
	{
		if (ReqDataEndPos>DataEndPos)
			OutReqLength=ReqDataEndPos-DataEndPos;
		else
			OutReqLength=0;

		OutFreeLength=(unsigned int)((ReadBuffEnd-ReadBuff)-DataEndPos);

		return ReadBuff + DataEndPos;
	}
	/**Requests information about the number of bytes that should be read into
	the buffer.
	@param OutFreeLength Number of bytes that _can_ be read into the returned
		buffer.
	@return Pointer to the buffer to read to.*/
	inline unsigned char *GetReadInfo(unsigned int &OutFreeLength)
	{
		OutFreeLength=(unsigned int)((ReadBuffEnd-ReadBuff)-DataEndPos);
		return ReadBuff + DataEndPos;
	}
	/**Should be called after data has been read into the buffer returned by
	RequestData().
	@param NewDataLength Number of bytes read.
	@return True, if the buffer now contains enough bytes to statisfy the
		requirement set by the previous RequestData() call.
		Otherwise, one should call GetReadInfo() to find out the parameters of
		the next read.*/
	bool OnNewData(unsigned int NewDataLength)
	{
		unsigned int ReadBuffSize=(unsigned int)(ReadBuffEnd-ReadBuff);
		if (NewDataLength>ReadBuffSize - DataEndPos)
			NewDataLength=ReadBuffSize;

		DataEndPos+=NewDataLength;
		if (DataEndPos>=ReqDataEndPos)
		{
			ReqDataEndPos=DataEndPos;
			return true;
		}
		else
			return false;
	}

protected:
	enum { BUFFER_ROUNDUP_TARGET = 10 };

	unsigned char StaticBuff[StaticReadBuffSize];
	unsigned char *ReadBuff, *ReadBuffEnd;

	unsigned int
		ReadPos, //Next data position for the parser.
		RelevantLength, //Number of bytes that are relevant _before_ ReadPos. (ReadPos-RelevantLength>=0)
		DataEndPos, //End position of the actual data.
		ReqDataEndPos; //Required end position of actual data.
};

}; //Comm

}; //UD
