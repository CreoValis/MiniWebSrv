#pragma once

#include <memory.h>
#include <list>

#include <boost/circular_buffer.hpp>

#include "BinUtils.h"

namespace UD
{

namespace Comm
{

/**Helper class to support fast async writes, with the goal of minimizing
memory allocation and write calls. Issued writes are always kept contiguous: no
issued write is split into two "write" system calls.
To accomplish this, objects of this class contain a fixed size buffer (with
size set by StaticWriteBuffSize), which is used as a FIFO byte queue. Writes
initially go into this buffer, which concatenates them into a continous range.
The contents of this range can then be written with one system call.
Writes that doesn't fit into a continous region at the end of the FIFO are
possibly written to the beginning of the FIFO.
If a write doesn't fit at either end of the FIFO buffer, then a new memory
buffer is allocated, and the write enqueued with it. Buffers allocated this way
are never released explicitly, only in the destructor. However, a buffer can
be reallocated internally, if a given write doesn't fit into any previously
allocated buffer.*/
template<unsigned int StaticWriteBuffSize, unsigned int InitQueueSize>
class WriteBuffQueue
{
public:
	inline WriteBuffQueue() : FIFOFreeBegin(WriteBuff), FIFOUsedBegin(WriteBuff),
		LastAllocLength(0), OutBuffA(InitQueueSize), FirstPendingI(0)
	{
	}
	inline ~WriteBuffQueue()
	{
		//Delete the free, dynamically allocated buffers.
		for (typename std::list<DynBuffer>::iterator NowI=FreeBuffList.begin(), EndI=FreeBuffList.end(); NowI!=EndI; ++NowI)
			delete[] NowI->Buff;

		//Delete the unsent, dynamically allocated buffers, too.
		for (typename boost::circular_buffer<Buffer>::iterator NowI=OutBuffA.begin(), EndI=OutBuffA.end(); NowI!=EndI; ++NowI)
		{
			if (!IsStaticBuffer(*NowI))
				delete[] NowI->Buff;
		}
	}

	/**Resets the contents and state of the buffer queue.
	@param ReleaseDynamicBuffers If true, any dynamically allocated buffers will
		be deallocated.*/
	void Reset(bool ReleaseDynamicBuffers=true)
	{
		FIFOFreeBegin=WriteBuff;
		FIFOUsedBegin=WriteBuff;
		LastAllocLength=0;

		if (ReleaseDynamicBuffers)
		{
			//Delete the free, dynamically allocated buffers.
			for (typename std::list<DynBuffer>::iterator NowI=FreeBuffList.begin(), EndI=FreeBuffList.end(); NowI!=EndI; ++NowI)
				delete[] NowI->Buff;

			FreeBuffList.clear();

			//Delete the unsent, dynamically allocated buffers, too.
			for (typename boost::circular_buffer<Buffer>::iterator NowI=OutBuffA.begin(), EndI=OutBuffA.end(); NowI!=EndI; ++NowI)
			{
				if (!IsStaticBuffer(*NowI))
					delete[] NowI->Buff;
			}
		}
		else
		{
			//Retain the previously allocated, dynamic buffers.
			for (typename boost::circular_buffer<Buffer>::iterator NowI=OutBuffA.begin(), EndI=OutBuffA.end(); NowI!=EndI; ++NowI)
			{
				if (!IsStaticBuffer(*NowI))
					FreeBuffList.push_back(DynBuffer(NowI->Buff,NowI->AllocLength));
			}
		}

		OutBuffA.clear();

		FirstPendingI=0;
	}

	/**Allocates a buffer on the write queue. No other Push() or Allocate()
	call should be made until the results of this allocation is Commit()-ed.
	@return The write buffer, into which the caller can copy it's own data.
		The contents of this buffer will not be returned by Pop() until Commit()
		has been called.*/
	unsigned char *Allocate(unsigned int Length)
	{
		if (!Length)
			return NULL;

		LastAllocLength=Length;

		unsigned char *RetBuff;
		unsigned int RetAllocLength;
		if (Length<sizeof(WriteBuff))
		{
			//We can hope to allocate some space from the FIFO.
			unsigned int BeginLen, FIFOBeginLen;
			GetContinousFreeFIFOLength(BeginLen,FIFOBeginLen);

			if (Length<=BeginLen)
			{
				RetBuff=FIFOFreeBegin;

				FIFOFreeBegin+=Length;
				if (FIFOFreeBegin>=WriteBuff+sizeof(WriteBuff))
					FIFOFreeBegin=WriteBuff;
			}
			else if (Length<=FIFOBeginLen)
			{
				RetBuff=WriteBuff;
				FIFOFreeBegin=WriteBuff + Length;
			}
			else
				RetBuff=NULL;

			if (RetBuff)
			{
				RetAllocLength=Length;

				/*We have successfully allocated our space from the FIFO. Let's try to
				append it to the last inserted "pending" buffer (if there's any).*/
				if (OutBuffA.size())
				{
					Buffer &LastBuff=OutBuffA.back();
					if ((LastBuff.State==BS_PENDING) && (IsStaticBuffer(LastBuff)) && (LastBuff.Buff+LastBuff.Length==RetBuff))
					{
						//We can safely "append" our newly allocated buffer.
						LastBuff.Length+=Length;
						LastBuff.State=BS_ALLOCATED;
						return RetBuff;
					}
				}
			}
		}
		else
			RetBuff=NULL;

		if (!RetBuff)
		{
			/*We have to dynamically allocate a buffer. Let's see if we have one in
			store.*/
			unsigned int CloseLength=~(unsigned int)0;
			typename std::list<DynBuffer>::iterator CloseI=FreeBuffList.end(), EndI=CloseI;
			for (typename std::list<DynBuffer>::iterator NowI=FreeBuffList.begin(); NowI!=EndI; ++NowI)
			{
				unsigned int CurrLength=NowI->Length;
				if ((CurrLength>=Length) && (CurrLength<CloseLength))
				{
					CloseI=NowI;
					CloseLength=CurrLength;
				}
			}

			if (CloseI!=EndI)
			{
				//We have found some dynamic buffer to use.
				RetBuff=CloseI->Buff;
				RetAllocLength=CloseLength;

				FreeBuffList.erase(CloseI);
			}
			else
			{
				//We have to allocate a new buffer. Round it up to at least 256.
				RetAllocLength=ROUNDUP_POWEROFTWO(Length,8);
				RetBuff=new unsigned char[RetAllocLength];
			}
		}

		/*Now we surely have something in RetBuff and RetAllocLength. Push the new
		buffer to the queue.*/
		if (!OutBuffA.full())
			OutBuffA.push_back(Buffer(RetBuff,Length,RetAllocLength,typename Buffer::AllocatedStateOption()));
		else
			OutBuffA.resize(OutBuffA.size()+1,Buffer(RetBuff,Length,RetAllocLength,typename Buffer::AllocatedStateOption()));

		return RetBuff;
	}
	/**Commits the previously allocated buffer into the write queue.
	If there is no allocated buffer, the method does nothing.
	@param Length Number of bytes in the allocated buffer to commit.
		If ~0, then the whole buffer will be commited to the write queue.*/
	void Commit(unsigned int Length=~0)
	{
		if ((!OutBuffA.empty()) && (OutBuffA.back().State==BS_ALLOCATED))
		{
			Buffer &LastBuff=OutBuffA.back();
			if (Length<LastAllocLength)
			{
				//We have to shrink the allocated buffer.
				if (IsStaticBuffer(LastBuff))
				{
					unsigned int ShrinkLen=LastAllocLength-Length;
					LastBuff.Length-=ShrinkLen;

					if (FIFOFreeBegin!=WriteBuff)
						FIFOFreeBegin-=ShrinkLen;
					else
						FIFOFreeBegin=WriteBuff + sizeof(WriteBuff)-ShrinkLen;

					if (!LastBuff.Length)
					{
						OutBuffA.pop_back();
						return;
					}
				}
				else if (Length)
					//Dynamic buffer.
					LastBuff.Length=Length;
				else
				{
					/*Remove the whole buffer, but preserve the dynamically allocated
					memory.*/
					FreeBuffList.push_back(DynBuffer(LastBuff.Buff,LastBuff.AllocLength));
					OutBuffA.pop_back();
					return;
				}
			}

			LastBuff.State=BS_PENDING;
		}
	}
	/**Pushes the contents of a buffer to the write queue. Essentially an
	Allocate, memcpy and Commit call.*/
	inline void Push(const unsigned char *Src, unsigned int Length)
	{
		unsigned char *NewBuff=Allocate(Length);
		memcpy(NewBuff,Src,Length);
		Commit();
	}

	/**Gets the next buffer to write.
	@return The buffer to write.*/
	const unsigned char *Pop(unsigned int &OutLength)
	{
		if (FirstPendingI<OutBuffA.size())
		{
			Buffer &RetBuff=OutBuffA[FirstPendingI];
			OutLength=RetBuff.Length;
			RetBuff.State=BS_WRITING;

			FirstPendingI++;

			return RetBuff.Buff;
		}
		else
		{
			OutLength=0;
			return NULL;
		}
	}
	/**Releases the first buffer returned by Pop(). After this call, the storage
	space used by this buffer can be reused.*/
	void Release()
	{
		if (OutBuffA.empty())
			return;

		Buffer CurrBuff=OutBuffA.front();
		if (CurrBuff.State!=BS_WRITING)
			return;

		OutBuffA.pop_front();
		FirstPendingI--; //Corrigate for skew introduced above.

		//We have to release CurrBuff somehow.
		if (IsStaticBuffer(CurrBuff))
		{
			//Release the previously allocate space from the FIFO.
			FIFOUsedBegin=CurrBuff.Buff + CurrBuff.Length;
			if (FIFOUsedBegin>=WriteBuff + sizeof(WriteBuff))
				FIFOUsedBegin-=sizeof(WriteBuff);

			if (FIFOUsedBegin==FIFOFreeBegin)
			{
				/*The FIFO is now empty. Reset it's pointers to the begining of it's
				buffer, so we can allocate the biggest possible contignous chunk from
				it.*/
				FIFOUsedBegin=WriteBuff;
				FIFOFreeBegin=WriteBuff;
			}
		}
		else
			//This is a dynamic buffer: keep it separately.
			FreeBuffList.push_back(DynBuffer(CurrBuff.Buff,CurrBuff.AllocLength));
	}

protected:
	enum BUFFERSTATE
	{
		BS_ALLOCATED, //Waiting for Commit().
		BS_PENDING, //Waiting for Pop().
		BS_WRITING, //Waiting for Release().

		BS_NOTUSED,
	};

	struct DynBuffer
	{
		inline DynBuffer() { }
		inline DynBuffer(unsigned char *NewBuff, unsigned int NewLen) : Buff(NewBuff), Length(NewLen)
		{ }

		unsigned char *Buff;
		unsigned int Length;
	};

	struct Buffer
	{
		struct AllocatedStateOption { };

		inline Buffer() { }
		inline Buffer(unsigned char *NewBuff, unsigned int NewLen, unsigned int NewAllocLen, AllocatedStateOption) :
			Buff(NewBuff), Length(NewLen), AllocLength(NewAllocLen), State(BS_ALLOCATED)
		{ }

		unsigned char *Buff;
		unsigned int Length, AllocLength;
		BUFFERSTATE State;
	};

	unsigned char WriteBuff[StaticWriteBuffSize]; //Static FIFO buffer.
	unsigned char *FIFOFreeBegin, //First free byte in the FIFO.
		*FIFOUsedBegin; //First used byte in the FIFO.
	unsigned int LastAllocLength;

	boost::circular_buffer<Buffer> OutBuffA; //Enqueued buffers.
	unsigned int FirstPendingI; //Index of the first pending buffer.
	std::list<DynBuffer> FreeBuffList; //Free, dynamically allocated buffers.

	bool IsStaticBuffer(const Buffer &Src) const
	{
		return (Src.Buff>=WriteBuff) && (Src.Buff<(WriteBuff + sizeof(WriteBuff)));
	}

	void GetContinousFreeFIFOLength(unsigned int &OutFromBegin, unsigned int &OutFromFIFOBegin) const
	{
		if (FIFOFreeBegin>=FIFOUsedBegin)
		{
			if (FIFOUsedBegin!=WriteBuff)
				OutFromBegin=(unsigned int)((WriteBuff + sizeof(WriteBuff)) - FIFOFreeBegin);
			else
				OutFromBegin=(unsigned int)((WriteBuff + sizeof(WriteBuff)) - FIFOFreeBegin - 1); //-1 for the reserved "guard" byte (region).

			if (FIFOUsedBegin>WriteBuff)
				OutFromFIFOBegin=(unsigned int)(FIFOUsedBegin - WriteBuff - 1); //-1 for the reserved "guard" byte (region).
			else
				OutFromFIFOBegin=0;
		}
		else
		{
			if (FIFOUsedBegin>FIFOFreeBegin)
				OutFromBegin=(unsigned int)(FIFOUsedBegin - FIFOFreeBegin - 1);
			else
				OutFromBegin=0;

			OutFromFIFOBegin=0;
		}
	}
};

/**Internal WriteBuffQueue documentation:
-Buffer states:
	-possible states:
		-allocated (waiting for user data)
		-pending (user data written, wating for write)
		-writing (waiting for release)
	-a buffer can only change states in the following order, expect for
		concatenation, where a pending->allocated transition occurs (see below)
	-free buffers are kept in a separate storage
	-only one buffer can be in allocated state: no new allocation is performed
		until the previously allocated buffer is put into pending state
	-multiple buffers can be in pending state
	-multiple buffers can be in writing state
-Allocate internals:
	-allocation in static buffer
		-allocation is performed by using the static buffer as a FIFO
		-if area to be allocated is too large to fit at the end of the buffer,
			the method tries to place it in the beginning, essentially marking the
			end of the buffer as "allocated"
		-a released buffer that has data in the static buffer will always set the
			beginning of the FIFO to "datapos + datalen", thus eventually
			releasing any data were lost due to allocations not fitting at the end
	-dynamic allocation
		-if the static buffer is full, or the buffer to be allocated is too
			large, it will be allocated from the heap
		-a dynamic buffer cannot be extended by new allocations
-Commit internals:
	-this method puts the perviously allocated buffer into the buffer queue
	-the buffer queue is implemented as boost::circular_buffer ?
-Pop internals:
	-
-Release internals:
-Static buffer handling:
 -the class tries to concatenate multiple buffers, that were allocated in the
	internal (continous) buffer
	-concatenation happens on allocation if the last pending buffer is in the
		static buffer, AND the new buffer to be allocated fits in the static
		buffer, too
	-concatenation is performed by extending the last pending buffer by the
		required number of bytes, and setting it to allocated state
	-the pointer returned by the Allocate() call, however, will point to the
		end of this last buffer, minus the newly added length
*/

}; //Comm

}; //UD
