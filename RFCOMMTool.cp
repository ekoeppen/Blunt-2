#define DEFAULT_LOGLEVEL 0

#include "RelocHack.h"
#include "CommToolImpl.h"
#include "RFCOMMTool.h"
#include "BluntServer.h"
#include "HCI.h"
#include "L2CAP.h"
#include <stdarg.h>
#include <stdio.h>
#include <NewtonScript.h>

extern "C" _sys_write (int fd, char* data, int len);

TRFCOMMTool::TRFCOMMTool (ULong serviceId): TCommTool (serviceId)
{
	TUNameServer nameServer;
	ULong id;
	ULong spec;
	TObjectId myPort;
	RefVar logLevel;

	logLevel = GetFrameSlot (GetVariable (GetFrameSlot (NSCallGlobalFn (SYM (GetGlobals)), SYM (blunt)), SYM (fLogLevel)), SYM (tool));
	fLogLevel = RefToInt (logLevel);

	if (nameServer.Lookup ("BluntServer", "TUPort", &id, &spec) == noErr) {
		fServerPort = id;
		fGetBuffer = NULL;
		myPort = * (TObjectId*) ( (Byte*) this + 0x8c);
		HLOG (0, "--------------------------------\nTRFCOMMTool::TRFCOMMTool\n  Port: %04x, Server port: %04x Log: %d\n", myPort, id, fLogLevel);
	} else {
		printf ("Blunt server not running!\n");
		fServerPort = 0;
	}

	RelocVTable (__VTABLE__11TRFCOMMTool);
}

TRFCOMMTool::~TRFCOMMTool (void)
{
	HLOG (0, "~TRFCOMMTool\n");
	delete fSavedData;
}

#pragma mark -

// ================================================================================
// ¥ Comm Tool Functions
// ================================================================================

NewtonErr TRFCOMMTool::HandleRequest (TUMsgToken& msgToken, ULong msgType)
{
	NewtonErr r;
	BluntEvent* event;
	int i;
	
	HLOG (1, "TRFCOMMTool::HandleRequest %08x %08x\n", &msgToken, msgType);
	if (msgType == BLUNT_MSG_TYPE) {
		event = (BluntEvent *) fMsgData;
		HLOG (1, "  Blunt event: %08x %08x %d\n", event, event->fOriginalEvent, event->fType);
		switch (event->fType) {
			case E_CONNECTION_COMPLETE: {
				BluntConnectionCompleteEvent *e = (BluntConnectionCompleteEvent *) event;
				if (e->fResult == noErr) {
					HLOG (1, "  Connection type %d complete: %04x, result: %d\n",
						e->fHandler->fHandlerType , fHCIHandle,
						e->fResult);
					fHCIHandle = e->fHCIHandle;
					if (e->fHandler->fHandlerType == H_RFCOMM) {
						ConnectComplete (e->fResult);
						fPeerRFCOMMPort = e->fRFCOMMPort;
						fDataHandler = e->fHandler;
						TCommToolGetEventReply event;
						event.fEventCode = 1;
						event.fEventData = kSerialEventDCDAssertedMask;
						event.fServiceId = 'rfcm';
						PostCommEvent (event, noErr);
					}
				} else {
					HLOG (1, "  Connection error: %d\n", e->fResult);
				}
				} break;
			case E_DATA: {
				BluntDataEvent *e = (BluntDataEvent *) event;
				Size n = 0;
				if (e->fResult == noErr) {
					if (fSavedDataAmount > 0 && fGetBuffer != NULL) {
						HLOG (1, "  First returning %d saved bytes", fSavedDataAmount);
						n = fGetBuffer->Putn (fSavedData, fSavedDataAmount);
						HLOG (1, ": Returned %d of %d (pos %d)\n", n, fSavedDataAmount, fGetBuffer->Position ());
						if (fSavedDataAmount > n) {
							HLOG (1, "  %d bytes still left\n", fSavedDataAmount - n);
							memcpy (fSavedData, fSavedData + n, fSavedDataAmount - n);
						}
						fSavedDataAmount -= n;
					}

					if (e->fLength > 0 && e->fData != NULL) {
						n = 0;
						if (fGetBuffer != NULL) {
							n = fGetBuffer->Putn (e->fData, e->fLength);
							HLOG (1, "  Data received: %d returned: %d pos: %d eof: %d space: %d\n", e->fLength, n, fGetBuffer->Position (), fGetBuffer->AtEOF (), fGetBuffer->GetSize());
							for (i = 0; i < e->fLength && i < 48; i++) {
								HLOG (2, "%02x ", e->fData[i]);
								if (((i + 1 ) % 16) == 2) HLOG (2, "\n");
							}
							HLOG (2, "\n");
						}
						if (e->fLength > n) {
							memcpy (fSavedData + fSavedDataAmount, e->fData + n, e->fLength - n);
							fSavedDataAmount += (e->fLength - n);
						}
						HLOG (1, "  %d bytes saved\n", fSavedDataAmount);
						if (e->fDelete) delete e->fData;
					}
					
					if (fGetBuffer != NULL) {
						GetComplete (noErr, true, fGetBuffer->Position ());
					}
				} else {
					HLOG (1, "  Error %d\n", e->fResult);
					GetComplete (e->fResult);
				}
				} break;
			case E_DATA_SENT: {
				BluntDataSentEvent *e = (BluntDataSentEvent *) event;
				fDataSent += e->fAmount;
				if (!fPutBuffer->AtEOF () > 0 && e->fResult == noErr && e->fAmount > 0) {
					HLOG (1, "  Data sent: %d Pos: %d\n", fDataSent, fPutBuffer->Position ());
					SendPendingData ();
				} else {
					HLOG (1, "  Data sent: %d : Result %d\n", fDataSent, e->fResult);
					PutComplete (e->fResult, fDataSent);
					HLOG (1, "  Put complete\n");
				}
				} break;
			case E_DISCONNECT_COMPLETE:
				TerminateComplete ();
				break;
		}
//		:KLUDGE: should be deleted
//		delete event->fOriginalEvent;
		r = noErr;
	} else {
		r = TCommTool::HandleRequest (msgToken, msgType);
	}
	return r;
}

NewtonErr TRFCOMMTool::ProcessOptionStart (TOption* theOption, ULong label, ULong opcode)
{
	NewtonErr r = noErr;
	Char s[5];
	
	memset (s, 0, 5);
	memcpy (s, &label, 4);
	HLOG (1, "TRFCOMMTool::ProcessOptionStart: %s\n", s);
	
	if (label == 'addr') {
		TRFCOMMAddressOption *o;
		o = (TRFCOMMAddressOption *) theOption;
		fPeerBdAddr[0] = o->fBdAddr[5];
		fPeerBdAddr[1] = o->fBdAddr[4]; 
		fPeerBdAddr[2] = o->fBdAddr[3];
		fPeerBdAddr[3] = o->fBdAddr[2]; 
		fPeerBdAddr[4] = o->fBdAddr[1];
		fPeerBdAddr[5] = o->fBdAddr[0]; 
		fPeerRFCOMMPort = o->fPort;
		HLOG (1, "  Peer Address: %02x:%02x:%02x:", o->fBdAddr[0], o->fBdAddr[1], o->fBdAddr[2]);
		HLOG (1, "%02x:%02x:%02x ", o->fBdAddr[3], o->fBdAddr[4], o->fBdAddr[5]);
		HLOG (1, "%d\n", o->fPort);
	} else if (label == 'pinc') {
		TRFCOMMPINCodeOption *o;
		o = (TRFCOMMPINCodeOption *) theOption;
		memcpy (fPINCode, o->fPINCode, sizeof (fPINCode));
		fPINCode[o->fPINCodeLength] = 0;
		HLOG (1, "  PIN Code: %s\n", fPINCode);
	} else if (label == 'lnkk') {
		TRFCOMMLinkKeyOption *o;
		o = (TRFCOMMLinkKeyOption *) theOption;
		memcpy (fLinkKey, o->fLinkKey, sizeof (fLinkKey));
		HLOG (1, "  Link key set\n");
	} else if (label == 'logl') {
		TRFCOMMLogLevelOption *o;
		o = (TRFCOMMLogLevelOption *) theOption;
		fLogLevel = o->fLogLevel;
		HLOG (1, "  Log level set: %d\n", fLogLevel);
	} else if (label == kCMOSerialEventEnables) {
		TCMOSerialEventEnables *o = (TCMOSerialEventEnables *) theOption;
		HLOG (1, "Serial events: %08x %d\n", o->serEventEnables, o->carrierDetectDownTime);
	} else {
		r = TCommTool::ProcessOptionStart (theOption, label, opcode);
	}
	return r;
}

void TRFCOMMTool::BindStart ()
{
	HLOG (1, "TRFCOMMTool::BindStart\n");

	BindComplete (noErr);
}

void TRFCOMMTool::ConnectStart ()
{
	HLOG (1, "TRFCOMMTool::ConnectStart\n");
	fSavedDataAmount = 0;
	fSavedData = new UByte[MAX_SAVE];
	fConnectionCommand.fToolPort = * (TObjectId*) ( (Byte*) this + 0x8c);
	memcpy (fConnectionCommand.fBdAddr, fPeerBdAddr, 6);
	fConnectionCommand.fTargetLayer = L_RFCOMM;
	fConnectionCommand.fL2CAPProtocol = PSM_RFCOMM;
	fConnectionCommand.fRFCOMMPort = fPeerRFCOMMPort;
	memcpy (fConnectionCommand.fLinkKey, fLinkKey, sizeof (fLinkKey));
	fServerPort.Send (&fConnectionCommand, sizeof (fConnectionCommand), kNoTimeout, M_COMMAND);
}

void TRFCOMMTool::ListenStart (void)
{
	HLOG (1, "TRFCOMMTool::ListenStart\n");
	ListenComplete (noErr);
}

void TRFCOMMTool::AcceptStart (void)
{
	HLOG (1, "TRFCOMMTool::AcceptStart\n");
	AcceptComplete (noErr);
}

void TRFCOMMTool::TerminateConnection (void)
{
	HLOG (1, "TRFCOMMTool::TerminateConnection\n");
	fDisconnectCommand.fToolPort = * (TObjectId*) ( (Byte*) this + 0x8c);
	memcpy (fDisconnectCommand.fBdAddr, fPeerBdAddr, 6);
	fDisconnectCommand.fHCIHandle = fHCIHandle;
	fServerPort.Send (&fDisconnectCommand, sizeof (fDisconnectCommand), kNoTimeout, M_COMMAND);
}

void TRFCOMMTool::TerminateComplete (void)
{
	HLOG (1, "TRFCOMMTool::TerminateComplete\n");
	TCommTool::TerminateComplete ();
}

#pragma mark -

NewtonErr TRFCOMMTool::PutBytes (CBufferList *list)
{
	HLOG (1, "TRFCOMMTool::PutBytes %d\n", list->GetSize ());
	Size pos;
	
	fPutBuffer = list;
	fDataSent = 0;
	pos = fPutBuffer->Seek (0, -1);
	HLOG (1, "  Pos: %d\n", pos);
	SendPendingData ();
	return noErr;
}

NewtonErr TRFCOMMTool::PutFramedBytes (CBufferList *, Boolean)
{
	HLOG (1, "TRFCOMMTool::PutFramedBytes\n");
	return noErr;
}

void TRFCOMMTool::KillPut (void)
{
	Size pos;
	
	HLOG (1, "TRFCOMMTool::KillPut\n");
	pos = fPutBuffer->Seek (0, 1);
	HLOG (1, "  Pos: %d\n", pos);
	KillPutComplete (noErr);
}

NewtonErr TRFCOMMTool::GetBytes (CBufferList *list)
{
	HLOG (0, "TRFCOMMTool::GetBytes %d (pos %d), saved %d\n", list->GetSize (), list->Position (), fSavedDataAmount);
	HLOG (0, "    %x %x %d %d\n", list, fGetBufferList, filler_01c6, filler_01c7);
	Long n;

	fGetBuffer = list;
	if (fSavedDataAmount > 0) {
		HLOG (1, "  Returning %d saved bytes", fSavedDataAmount);
		n = fGetBuffer->Putn (fSavedData, fSavedDataAmount);
		HLOG (1, ": Returned %d of %d (pos %d)\n", n, fSavedDataAmount, fGetBuffer->Position ());
		if (fSavedDataAmount > n) {
			HLOG (1, "  %d bytes still left\n", fSavedDataAmount - n);
			memcpy (fSavedData, fSavedData + n, fSavedDataAmount - n);
		}
		fSavedDataAmount -= n;
		if (n > 0 ) {
			GetComplete (noErr, true, fGetBuffer->Position ());
		}
	} else {
		if (fGetBuffer->AtEOF ())  {
			HLOG (1, "  Get complete\n");
			GetComplete (noErr);
		}
	}
	return noErr;
}

NewtonErr TRFCOMMTool::GetFramedBytes (CBufferList *)
{
	HLOG (1, "TRFCOMMTool::GetFramedBytes\n");
	return noErr;
}

void TRFCOMMTool::GetOptionsComplete (NewtonErr result)
{
	HLOG (0, "TRFCOMMTool::GetOptionsComplete: %d\n", result);
	TCommTool::GetOptionsComplete (result);
}


void TRFCOMMTool::HandleReply (ULong userRefCon, ULong msgType)
{
	HLOG (1, "TRFCOMMTool::HandleReply\n");
	TCommTool::HandleReply (userRefCon, msgType);
}

void TRFCOMMTool::DoControl (ULong opCode, ULong msgType)
{
	HLOG (1, "TRFCOMMTool::DoControl 0x%x 0x%x %c\n", opCode, msgType, msgType);
	TCommTool::DoControl (opCode, msgType);
}

void TRFCOMMTool::DoStatus (unsigned long a, unsigned long b)
{
	HLOG (1, "TRFCOMMTool::DoStatus\n");
	TCommTool::DoStatus (a, b);
}

UByte* TRFCOMMTool::GetCommEvent ()
{
	HLOG (1, "TRFCOMMTool::GetCommEvent\n");
	return TCommTool::GetCommEvent ();
}

void TRFCOMMTool::OptionMgmt (TCommToolOptionMgmtRequest *request)
{
	HLOG (1, "TRFCOMMTool::OptionMgmt\n");
	TCommTool::OptionMgmt (request);
}

ULong TRFCOMMTool::ProcessOptions (TCommToolOptionInfo* option)
{
	ArrayIndex i;
	Char s[5];
	ULong label;
	
<<<<<<< HEAD
	HLOG (0, "TRFCOMMTool::ProcessOptions %x\n", option->fOptionsState);
=======
	memset (s, 0, 5);
	HLOG (1, "TRFCOMMTool::ProcessOptions %x\n", option->fOptionsState);
>>>>>>> e1db479a8d8d6fd6b0d7941de4a7958cb19abe4e
 	if (option->fOptions) {
		HLOG (1, "  %d: ", option->fOptions->GetArrayCount());
		for (i = 0; i < option->fOptions->GetArrayCount(); i++) {
			label = option->fOptions->OptionAt (i)->Label ();
			memcpy (s, &label, 4);
			HLOG (1, "%s ", s);
		}
		HLOG (1, "\n");
	}
	if (option->fOptionsIterator) {
		HLOG (1, "  it: %d\n", option->fOptionsIterator->More ());
	}
	if (option->fCurOptPtr) {
		label = option->fCurOptPtr->Label ();
		memcpy (s, &label, 4);
		HLOG (1, "  %s\n", s);
	}
	TCommTool::ProcessOptions (option);
}

void TRFCOMMTool::ProcessGetBytesOptionStart (TOption* theOption, ULong label, ULong opcode)
{
	Char s[5];

	memcpy (s, &label, 4);
	HLOG (1, "TRFCOMMTool::ProcessGetBytesOptionStart %s\n", s);
	TCommTool::ProcessGetBytesOptionStart (theOption, label, opcode);
}

void TRFCOMMTool::ProcessOption (TOption* theOption, ULong label, ULong opcode)
{
	Char s[5];

	memcpy (s, &label, 4);
	HLOG (1, "TRFCOMMTool::ProcessOption %s\n", s);
	TCommTool::ProcessOption (theOption, label, opcode);
}

void TRFCOMMTool::GetBytesImmediate (CBufferList* clientBuffer, Size threshold)
{
	Long n;

	HLOG (0, "TRFCOMMTool::GetBytesImmediate (%d %d %d)\n", clientBuffer->GetSize(), threshold, fSavedDataAmount);
	if (fSavedDataAmount > threshold) {
		n = clientBuffer->Putn (fSavedData, threshold);
		memcpy (fSavedData, fSavedData + n, fSavedDataAmount - n);
		fSavedDataAmount -= n;
		GetComplete (noErr);
	} else {
		GetComplete (noErr, false, 0);
	}
}

void TRFCOMMTool::GetComplete (NewtonErr result, Boolean endOfFrame, ULong getBytesCount)
{
	HLOG (0, "TRFCOMMTool::GetComplete %d %d %d\n", result, endOfFrame, getBytesCount);
	fGetBuffer = NULL;
	TCommTool::GetComplete (result, endOfFrame, getBytesCount);
}

void TRFCOMMTool::KillGet (void)
{
	HLOG (1, "TRFCOMMTool::KillGet\n");
	fSavedDataAmount = 0;
	KillGetComplete (noErr);
}

void TRFCOMMTool::SendPendingData ()
{
	HLOG (1, "TRFCOMMTool::SendPendingData %d\n", fPutBuffer->GetSize ());
	fDataCommand.fData = fPutBuffer;
	fDataCommand.fHCIHandle = fHCIHandle;
	fDataCommand.fRFCOMMPort = fPeerRFCOMMPort;
	fDataCommand.fHandler = fDataHandler;
	fServerPort.Send (&fDataCommand, sizeof (fDataCommand), kNoTimeout, M_COMMAND);
}

void TRFCOMMTool::Log (int logLevel, char *format, ...)
{
    va_list args;
    
    if (fLogLevel >= logLevel) {
        va_start (args, format);
        vsprintf ((char *) fLogCommand.fData, format, args);
        va_end (args);
		fLogCommand.fSize = strlen ((char *) fLogCommand.fData);
		fServerPort.Send (&fLogCommand, sizeof (fLogCommand), kNoTimeout, M_COMMAND);
    }
}

