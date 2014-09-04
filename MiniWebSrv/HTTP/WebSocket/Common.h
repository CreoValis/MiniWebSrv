#pragma once

namespace HTTP
{

namespace WebSocket
{

enum CLOSEREASON
{
	CR_NONE         =    0,  //Must not be sent.
	CR_NORMAL       = 1000,
	CR_EXIT         = 1001,
	CR_PROT_ERROR   = 1002,
	CR_UNK_DATATYPE = 1003,
	CR_UNKNOWN_CODE = 1005, //Must not be sent.
	CR_CONN_ERROR   = 1006, //Must not be sent.
	CR_DATA_ERROR   = 1007,
	CR_POLICY_ERROR = 1008,
	CR_SIZE_LIMIT   = 1009,
	CR_EXT_ERROR    = 1010,
	CR_UNEXP_ERROR  = 1011,
};

enum MESSAGETYPE
{
	MSGTYPE_TEXT,
	MSGTYPE_BINARY,
};

}; //WebSocket

}; //namespace HTTP
