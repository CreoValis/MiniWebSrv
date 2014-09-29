#pragma once

#include <boost/detail/endian.hpp>

#define ROUNDUP_POWEROFTWO(Val,Pow) (((Val) + ((1 << Pow)-1)) & ~((1 << Pow)-1))

#define BYTESWAP2(x) ((unsigned short)( \
	((x) << 8) | \
	((x) >> 8) ))
#define BYTESWAP4(x) ((unsigned int)( \
	((x) << 24) | \
	(((x) << 8) & 0x00FF0000) | \
	(((x) >> 8) & 0x0000FF00) | \
	((x) >> 24) ))
#define BYTESWAP8(x) ((unsigned long long)( \
	((x) <<  56) | \
	(((x) << 40) & 0x00FF000000000000) | \
	(((x) << 24) & 0x0000FF0000000000) | \
	(((x) <<  8) & 0x000000FF00000000) | \
	(((x) >>  8) & 0x00000000FF000000) | \
	(((x) >> 24) & 0x0000000000FF0000) | \
	(((x) >> 40) & 0x000000000000FF00) | \
	((x) >>  56) ))

#ifdef BOOST_BIG_ENDIAN

	//ez bigendian, tehát a bigendianból olvasás -> noop
	#define FROM_BIGENDIAN2(x) (x)
	#define FROM_BIGENDIAN4(x) (x)
	#define FROM_BIGENDIAN8(x) (x)

	//ez bigendian, tehát a littleendianból olvasás -> swap
	#define FROM_LITTLEENDIAN2(x) ( BYTESWAP2(x) )
	#define FROM_LITTLEENDIAN4(x) ( BYTESWAP4(x) )
	#define FROM_LITTLEENDIAN8(x) ( BYTESWAP8(x) )

	/*A lenti makrók feltételezik, hogy Ptr egy valamilyen char típusú pointer.*/

	#ifdef UNALIGNED_MEMACCESS

	//ez bigendian, tehát bigendianból olvasás: nem kell swap.
	#define GET_BIGENDIAN_UNALIGNED2(Ptr) ( *((unsigned short *)(Ptr)) )
	#define GET_BIGENDIAN_UNALIGNED4(Ptr) ( *((unsigned int *)(Ptr)) )
	#define GET_BIGENDIAN_UNALIGNED8(Ptr) ( *((uint64_t *)(Ptr)) )

	#define SET_BIGENDIAN_UNALIGNED2(Ptr,Value) { *((unsigned short *)(Ptr))=Value; }
	#define SET_BIGENDIAN_UNALIGNED4(Ptr,Value) { *((unsigned int *)(Ptr))=Value; }
	#define SET_BIGENDIAN_UNALIGNED8(Ptr,Value) { *((uint64_t *)(Ptr))=Value; }

	#else //#ifdef UNALIGNED_MEMACCESS

	#define GET_BIGENDIAN_UNALIGNED2(Ptr) ( ((unsigned short)(*(Ptr)) << 8) | ((unsigned short)(*((Ptr)+1))) )
	#define GET_BIGENDIAN_UNALIGNED4(Ptr) ( ((unsigned int)(*(Ptr)) << 24) | ((unsigned int)(*((Ptr)+1)) << 16) | \
		((unsigned int)(*((Ptr)+2)) << 8) | ((unsigned int)(*((Ptr)+3))) )
	#define GET_BIGENDIAN_UNALIGNED8(Ptr) ( (((uint64_t)GET_BIGENDIAN_UNALIGNED4((Ptr))) << 32) | \
		((uint64_t)(GET_BIGENDIAN_UNALIGNED4((Ptr)+4))) )

	#define SET_BIGENDIAN_UNALIGNED2(Ptr,Value) { *(Ptr)=((Value) >> 8) & 0xFF; *((Ptr)+1)=(Value) & 0xFF; }
	#define SET_BIGENDIAN_UNALIGNED4(Ptr,Value) \
		{ *(Ptr)=((Value) >> 24) & 0xFF; *((Ptr)+1)=((Value) >> 16) & 0xFF; *((Ptr)+2)=((Value) >> 8) & 0xFF; *((Ptr)+3)=(Value) & 0xFF; }
	#define SET_BIGENDIAN_UNALIGNED8(Ptr,Value) \
		{ SET_BIGENDIAN_UNALIGNED4((Ptr),Value >> 32); SET_BIGENDIAN_UNALIGNED4(((Ptr)+4),Value & 0xFFFFFFFF); }

	#endif

#else //#ifdef BOOST_BIG_ENDIAN

	//ez littleendian, tehát a bigendianból olvasás -> swap
	#define FROM_BIGENDIAN2(x) ( BYTESWAP2(x) )
	#define FROM_BIGENDIAN4(x) ( BYTESWAP4(x) )
	#define FROM_BIGENDIAN8(x) ( BYTESWAP8(x) )

	//ez littleendian, tehát a littleendianból olvasás -> noop
	#define FROM_LITTLEENDIAN2(x) (x)
	#define FROM_LITTLEENDIAN4(x) (x)
	#define FROM_LITTLEENDIAN8(x) (x)

	/*A lenti makrók feltételezik, hogy Ptr egy valamilyen char típusú pointer.
	FIGYELEM: Ezek már meg vannak javítva!!!*/

	#ifdef UNALIGNED_MEMACCESS

	#define GET_BIGENDIAN_UNALIGNED2(Ptr) ( FROM_BIGENDIAN2(*((unsigned short *)(Ptr))) )
	#define GET_BIGENDIAN_UNALIGNED4(Ptr) ( FROM_BIGENDIAN4(*((unsigned int *)(Ptr))) )
	#define GET_BIGENDIAN_UNALIGNED8(Ptr) ( FROM_BIGENDIAN8(*((uint64_t *)(Ptr))) )

	#define SET_BIGENDIAN_UNALIGNED2(Ptr,Value) { *((unsigned short *)(Ptr))=FROM_BIGENDIAN2(Value); }
	#define SET_BIGENDIAN_UNALIGNED4(Ptr,Value) { *((unsigned int *)(Ptr))=FROM_BIGENDIAN4(Value); }
	#define SET_BIGENDIAN_UNALIGNED8(Ptr,Value) { *((uint64_t *)(Ptr))=FROM_BIGENDIAN8(Value); }

	#else

	#define GET_BIGENDIAN_UNALIGNED2(Ptr) ( ((unsigned short)(*(Ptr)) << 8) | ((unsigned short)(*((Ptr)+1))) )
	#define GET_BIGENDIAN_UNALIGNED4(Ptr) ( ((unsigned int)(*(Ptr)) << 24) | ((unsigned int)(*((Ptr)+1)) << 16) | \
		((unsigned int)(*((Ptr)+2)) << 8) | ((unsigned int)(*((Ptr)+3))) )
	#define GET_BIGENDIAN_UNALIGNED8(Ptr) ( (((uint64_t)GET_BIGENDIAN_UNALIGNED4((Ptr))) << 32) | \
		((uint64_t)(GET_BIGENDIAN_UNALIGNED4((Ptr)+4))) )

	#define SET_BIGENDIAN_UNALIGNED2(Ptr,Value) { *(Ptr)=((Value) >> 8) & 0xFF; *((Ptr)+1)=(Value) & 0xFF; }
	#define SET_BIGENDIAN_UNALIGNED4(Ptr,Value) \
		{ *(Ptr)=((Value) >> 24) & 0xFF; *((Ptr)+1)=((Value) >> 16) & 0xFF; *((Ptr)+2)=((Value) >> 8) & 0xFF; *((Ptr)+3)=(Value) & 0xFF; }
	#define SET_BIGENDIAN_UNALIGNED8(Ptr,Value) \
		{ SET_BIGENDIAN_UNALIGNED4((Ptr),Value >> 32); SET_BIGENDIAN_UNALIGNED4(((Ptr)+4),Value & 0xFFFFFFFF); }

	#endif

#endif