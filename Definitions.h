#ifndef __DEFINITONS_H
#define __DEFINITONS_H

#define MAX_HANDLERS 32
#define MAX_PACKETS 64
#define MAX_MESSAGE 512
#define MAX_LOG 100

#define BLUNT_MSG_TYPE 0x0000bbbb

#define kErrNoHW			-100000	
#define kErrSendFailed		-100001
#define kErrNoPeer			-100002
#define kErrInputUnderflow	-100003

#define kBluntEventClass 'Blnt'
#define kBluntEventId 'Blnt'

#define kResultOk							0
#define kResultComplete						1
#define kResultPairConnectFailed			2
#define kResultPairAuthReqFailed			3
#define kResultStatusTimeout              255 

#define LOGGING

#ifdef LOGGING

#define LOG if (fLogLevel > 0) printf
#define LOG_ERROR printf
#define LOGX printf
#define LOG2 if (fLogLevel > 1) printf
#define LOG3 if (fLogLevel > 2) printf
#define HLOG Log
#define HAMMER_LOG hammer_log

#else

#define LOG
#define LOG_ERROR
#define LOGX
#define LOG2
#define LOG3
#define HLOG
#define HAMMER_LOG

#endif

#define SET_SHORT(addr, offset, value) addr[offset] = value & 0x00ff; addr[offset + 1] = (value & 0xff00) >> 8
#define GET_SHORT(addr, offset) (addr[(offset) + 1] * 256 + addr[(offset)])

#define SET_LONG(addr, offset, value) addr[offset] = value & 0x000000ff;             addr[offset + 1] = (value & 0x0000ff00) >> 8; 	addr[offset + 2] = (value & 0x00ff0000) >> 16; addr[offset + 3] = (value & 0xff000000) >> 24;
#define GET_LONG(addr, offset)  ((addr[(offset) + 3] << 24) + (addr[(offset) + 2] << 16) + (addr[(offset) + 1] << 8) + addr[(offset)])

//#define SET_USHORT_N(addr, offset, value) \
//	* (UShort *) (&addr[offset]) = value
//#define SET_SHORT_N(addr, offset, value) \
//	* (Short *) (&addr[offset]) = value
// #define SET_ULONG_N(addr, offset, value) \
//	* (ULong *) (&addr[offset]) = value
#define SET_SHORT_N(addr, offset, value) addr[offset + 1] =  value & 0x000000ff;       addr[offset] = (value & 0x0000ff00) >> 8;
#define SET_USHORT_N(addr, offset, value) addr[offset + 1] =  value & 0x000000ff;       addr[offset] = (value & 0x0000ff00) >> 8;
#define SET_ULONG_N(addr, offset, value) addr[offset + 3] =  value & 0x000000ff;       addr[offset + 2] = (value & 0x0000ff00) >> 8; 	addr[offset + 1] = (value & 0x00ff0000) >> 16;addr[offset] = (value & 0xff000000) >> 24;

#define GET_USHORT_N(addr, offset) * (UShort *) (&addr[offset])
#define GET_SHORT_N(addr, offset) * (Short *) (&addr[offset])
#define GET_ULONG_N(addr, offset) * (ULong *) (&addr[offset])
	
#define NUM_ELEMENTS(x) (sizeof (x)/sizeof (x[0]))	

#endif
