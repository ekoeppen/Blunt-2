#ifndef __COMMTOOLIMPL_H
#define __COMMTOOLIMPL_H

#include <BufferSegment.h>
#include <CommManagerInterface.h>
#include <CommTool.h>
#include <CommErrors.h>
#include <SerialOptions.h>
#include <CommOptions.h>
#include <CommToolOptions.h>
#include <Endpoint.h>
#include <UserTasks.h>
#include "Buffer.h"
#include "CircleBuf.h"

// ================================================================================
// � TCommTool
// ================================================================================

class TCommTool: public TUTaskWorld					/* 0018 - 026b */
{

public:
	Byte 					filler_0030[0x30 - 0x18];

	TCMOCTConnectInfo 		fConnectInfo;			/* 0030 - 0044 */

	ULong					fMsgSize;				/* 0044 */
	UByte					fMsgData[0x40];			/* 0048 */
	
	Byte					filler_0088[0xbc];  	/* 0090 */
	

#if 0
	Byte					filler_0044[0x48];  	/* 0044 */
	
	TUPort					fPort;					/* 008c */
	
	Byte					filler_0094[0xb4];  	/* 0094 - 0148 */
#endif

public:
	TCMOTransportInfo		fTransportInfo;			/* 0148 */
	ULong					filler_0174;
	TCommToolOptionInfo 	fOpenOptionInfo;		/* 0178 */
	TCommToolOptionInfo 	fGetOptionInfo;			/* 0190 */
	TCommToolOptionInfo 	fPutOptionInfo;			/* 01a8 */
	
	ULong					filler_01c0;

	UByte					filler_01c4;
	UByte					filler_01c5;
	UByte					filler_01c6;
	UByte					filler_01c7;
	
	CBufferList*			fGetBufferList;
	
	Size					fTCommToolInputBufferSize;
	
	UByte					filler_01d0;
	UByte					filler_01d1;
	
	UByte					flag_01d2;
	
	UByte					filler_01d3;
	ULong					filler_01d4;
	
	TCommToolGetEventReply	fGetEventReply;			/* 01d8 */
	
	ULong					filler_01fc;
	ULong					filler_0200;
	ULong					filler_0204;
	ULong					filler_0208;
	ULong					filler_020c;
	ULong					filler_0210;
	ULong					filler_0214;
	ULong					filler_0218;
	ULong					filler_021c;
	
	CShadowBufferSegment	fBufferSegment_1;		/* 0220 */
	CShadowBufferSegment	fBufferSegment_2;  		/* 023c */
													/* 0248 */
												
	Byte 					filler_0248[0x14];

public:
							TCommTool (unsigned long);
							TCommTool (unsigned long, long);
	virtual					~TCommTool (void);
	
	virtual	ULong			GetSizeOf (void) = 0;

	virtual	NewtonErr		TaskConstructor();
	virtual	void			TaskDestructor();
	virtual	void			TaskMain();

	virtual UChar*			GetToolName() = 0;

	virtual void			HandleInternalEvent(void);
	virtual NewtonErr		HandleRequest(TUMsgToken& msgToken, ULong msgType);
	virtual void			HandleReply(ULong userRefCon, ULong msgType);
	virtual void			HandleTimerTick(void);

	virtual void			DoControl(ULong opCode, ULong msgType);
	virtual void			DoKillControl(ULong msgType);
	virtual void			DoStatus(unsigned long, unsigned long);
	
	virtual UByte*			GetCommEvent();
	virtual void			DoKillGetCommEvent();
	virtual NewtonErr		PostCommEvent(TCommToolGetEventReply& theEvent, NewtonErr result);

	virtual NewtonErr		OpenStart(TOptionArray* options);
	virtual NewtonErr		OpenComplete();
	virtual int				Close (void);
	virtual void			CloseComplete(NewtonErr result);

	virtual void			ConnectStart();
	virtual void			ConnectComplete(NewtonErr result);

	virtual void			ListenStart();
	virtual void			ListenComplete(NewtonErr result);

	virtual void			AcceptStart();
	virtual void			AcceptComplete(NewtonErr result);

			void			Disconnect();
	virtual void			DisconnectComplete(NewtonErr result);

	virtual void			ReleaseStart();
	virtual void			ReleaseComplete(NewtonErr result);

	virtual void			BindStart();
	virtual void			BindComplete(NewtonErr result);

	virtual void			UnbindStart();
	virtual void			UnbindComplete(NewtonErr result);

	virtual void			GetProtAddr();
	
	virtual void		 	OptionMgmt(TCommToolOptionMgmtRequest *);
	virtual void		 	OptionMgmtComplete(NewtonErr result);

	virtual ULong			ProcessOptions(TCommToolOptionInfo*);
	virtual ULong			ProcessOptionsContinue(TCommToolOptionInfo*);
	virtual ULong			ProcessOptionsComplete(NewtonErr, TCommToolOptionInfo*);
	virtual ULong			ProcessOptionsCleanUp(NewtonErr, TCommToolOptionInfo*);

	virtual void			ProcessCommOptionComplete(unsigned long, TCommToolOptionInfo* theOption);
	virtual NewtonErr		ProcessOptionStart(TOption* theOption, ULong label, ULong opcode);
	virtual void			ProcessOptionComplete(unsigned long);
	virtual void			ProcessOption(TOption* theOption, ULong label, ULong opcode);
	
	virtual void			ForwardOptions();
	virtual NewtonErr		AddDefaultOptions(TOptionArray* options);
	virtual NewtonErr		AddCurrentOptions(TOptionArray* options);


	virtual void			ProcessPutBytesOptionStart(TOption* theOption, ULong label, ULong opcode);
	virtual void			ProcessPutBytesOptionComplete(unsigned long);
	virtual void			ProcessGetBytesOptionStart(TOption* theOption, ULong label, ULong opcode);
	virtual void			ProcessGetBytesOptionComplete(unsigned long);
	
	virtual	NewtonErr		PutBytes (CBufferList *) = 0;
	virtual	NewtonErr		PutFramedBytes (CBufferList *, Boolean) = 0;

	virtual void			PutComplete(NewtonErr result, ULong putBytesCount);
	virtual	void			KillPut (void) = 0;
	virtual void			KillPutComplete(NewtonErr result);
	virtual	NewtonErr		GetBytes (CBufferList *) = 0;
	virtual	NewtonErr		GetFramedBytes (CBufferList *) = 0;

	virtual void			GetBytesImmediate(CBufferList* clientBuffer, Size threshold);
	virtual void			GetComplete(NewtonErr result, Boolean endOfFrame = false, ULong getBytesCount = 0);
	virtual	void			KillGet (void) = 0;
	virtual void			KillGetComplete(NewtonErr result);

	virtual void			PrepGetRequest();
	virtual void			GetOptionsComplete(NewtonErr);

	virtual void			PrepPutRequest();
	virtual void			PutOptionsComplete(NewtonErr);
	
	virtual void			ResArbRelease(unsigned char*, unsigned char*);
	virtual void			ResArbReleaseStart(unsigned char*, unsigned char*);
	virtual void			ResArbReleaseComplete(NewtonErr);
	virtual void			ResArbClaimNotification(unsigned char*, unsigned char*);
	
	virtual void			TerminateConnection();
	virtual void			TerminateComplete();
	virtual void			GetNextTermProc(ULong terminationPhase,ULong& terminationFlag,TerminateProcPtr& TerminationProc);
	virtual void			SetChannelFilter(CommToolRequestType, Boolean);
};

#endif
