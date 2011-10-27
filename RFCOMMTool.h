#ifndef __RFCOMMTOOL_H
#define __RFCOMMTOOL_H

#include "Definitions.h"
#include "CommToolImpl.h"
#include "EventsCommands.h"

class SCCChannelInts;
class TCMOSerialIOParms;
class TCMOSerialHardware;
class TCMOSerialHWParms;
class TCMOSerialHWChipLoc;
class TCMOSerialChipSpec;
class TCMService;
class TSerialChip;
class TOption;
class Handler;

#define TOOL_GET_STATUS		0x10000000
#define TOOL_SENDING 		0x20000000
#define TOOL_RECEIVING 		0x40000000
#define TOOL_XYZ 			0x80000000

#define kRFCOMMToolErrBase				-19000
#define kRFCOMMToolErrBind				(-10078)
#define kRFCOMMToolErrConnectStatus		(kRFCOMMToolErrBase - 1)
#define kRFCOMMToolErrConnect			(kRFCOMMToolErrBase - 2)
#define kRFCOMMToolErrAcceptStatus		(kRFCOMMToolErrBase - 3)
#define kRFCOMMToolErrAccept			(kRFCOMMToolErrBase - 4)

#define MAX_SAVE 2096

// ================================================================================
// ¥ TRFCOMMTool option classes
// ================================================================================

class TRFCOMMAddressOption: public TOption
{
public:
	UByte					fBdAddr[6];
	Byte					fPort;
};

class TRFCOMMPINCodeOption: public TOption
{
public:
	Byte					fPINCodeLength;
	UByte					fPINCode[16];
};

class TRFCOMMNameOption: public TOption
{
public:
	UByte					fNameLength;
	UChar					fName[64];
};

class TRFCOMMLinkKeyOption: public TOption
{
public:
	UChar					fLinkKey[16];
};

class TRFCOMMLogLevelOption: public TOption
{
public:
	Byte					fLogLevel;
};

// ================================================================================
// ¥ TRFCOMMTool
// ================================================================================

class TRFCOMMTool: public TCommTool					/* 0690 */
{
public:
	TUPort					fServerPort;
	int						fLogLevel;
	UByte					fPeerBdAddr[6];
	Byte					fPeerRFCOMMPort;
	Short					fHCIHandle;
	Char					fPINCode[16];
	CBufferList				*fGetBuffer;
	CBufferList				*fPutBuffer;
	Handler					*fDataHandler;
	UByte*					fSavedData;
	Long					fSavedDataAmount;
	Long					fDataSent;
	UByte					fLinkKey[16];
	BluntDataCommand		fDataCommand;
	BluntConnectionCommand	fConnectionCommand;
	BluntDisconnectCommand	fDisconnectCommand;
	BluntLogCommand			fLogCommand;
	
	Byte					fPadding[12];
	
							TRFCOMMTool (ULong serviceId);
	virtual					~TRFCOMMTool (void);
	virtual	ULong			GetSizeOf (void) { return sizeof (TRFCOMMTool); }
	virtual UChar*			GetToolName() { return (UChar*) "TRFCommTool"; }

	virtual NewtonErr		HandleRequest (TUMsgToken& msgToken, ULong msgType);
	virtual void			HandleReply (ULong userRefCon, ULong msgType);
	virtual void			DoControl (ULong opCode, ULong msgType);
	virtual void			DoStatus (unsigned long, unsigned long);
	virtual UByte*			GetCommEvent ();

	virtual void			BindStart (void);
	virtual void			ConnectStart (void);
	virtual void			ListenStart (void);
	virtual void			AcceptStart (void);

	virtual void		 	OptionMgmt (TCommToolOptionMgmtRequest *);
	virtual ULong			ProcessOptions (TCommToolOptionInfo* option);
	virtual NewtonErr		ProcessOptionStart (TOption* theOption, ULong label, ULong opcode);
	virtual void			ProcessGetBytesOptionStart (TOption* theOption, ULong label, ULong opcode);
	virtual void			ProcessOption (TOption* theOption, ULong label, ULong opcode);

	virtual	NewtonErr		PutBytes (CBufferList *);
	virtual	NewtonErr		PutFramedBytes (CBufferList *, Boolean);
	virtual	void			KillPut (void);

	virtual	NewtonErr		GetBytes (CBufferList *);
	virtual	NewtonErr		GetFramedBytes (CBufferList *);
//	virtual void			GetBytesImmediate (CBufferList* clientBuffer, Size threshold);
	virtual void			GetComplete (NewtonErr result, Boolean endOfFrame = false, ULong getBytesCount = 0);
	virtual	void			KillGet (void);

	virtual void			TerminateConnection (void);
	virtual void			TerminateComplete (void);

	void					SendPendingData ();

    void                    Log (int logLevel, char* format, ...);
};

extern StartCommTool (TCommTool *, ULong, TServiceInfo *);
extern OpenCommTool (ULong, TOptionArray *, TCMService *);

#endif
