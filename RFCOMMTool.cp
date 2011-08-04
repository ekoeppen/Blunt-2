#define DEFAULT_LOGLEVEL 0

#include "RelocHack.h"
#include "CommToolImpl.h"
#include "RFCOMMTool.h"
#include "BluntServer.h"
#include "HCI.h"
#include "L2CAP.h"
#include <stdarg.h>
#include <stdio.h>

extern "C" _sys_write (int fd, char* data, int len);

TRFCOMMTool::TRFCOMMTool (unsigned long serviceId): TCommTool (serviceId)
{
	fLogLevel = DEFAULT_LOGLEVEL;
	HLOG (1, "-------------------------\nTRFCOMMTool 2\n");
	
	RelocVTable (__VTABLE__11TRFCOMMTool);
}

TRFCOMMTool::~TRFCOMMTool (void)
{
	HLOG (1, "~TRFCOMMTool\n");
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
					HLOG (1, "  Connection type %d complete: %04x\n", e->fHandler->fHandlerType , fHCIHandle);
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
					if (e->fLength > 0 && e->fData != NULL) {
						if (fGetBuffer != NULL) {
							n = fGetBuffer->Putn (e->fData, e->fLength);
							if (fGetBuffer->AtEOF ()) GetComplete (noErr);
							HLOG (1, "  Data received: %d returned: %d pos: %d eof: %d space: %d\n", e->fLength, n, fGetBuffer->Position (), fGetBuffer->AtEOF (), fGetBuffer->GetSize());
							for (i = 0; i < e->fLength && i < 256; i++) {
								HLOG (2, "%02x ", e->fData[i]);
								if (((i + 1 ) % 16) == 0) HLOG (2, "\n");
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
	} else if (label == kCMOSerialEventEnables) {
		TCMOSerialEventEnables *o = (TCMOSerialEventEnables *) theOption;
		HLOG (1, "Serial events: %08x %d\n", o->serEventEnables, o->carrierDetectDownTime);
	}
	return TCommTool::ProcessOptionStart (theOption, label, opcode);
}

void TRFCOMMTool::BindStart ()
{
	TUNameServer nameServer;
	ULong id;
	ULong spec;
	HLOG (1, "TRFCOMMTool::BindStart\n");
	TObjectId myPort;
	NewtonErr r;
	
	r = nameServer.Lookup ("BluntServer", "TUPort", &id, &spec);
	if (r == noErr) {
		fServerPort = id;
		fGetBuffer = NULL;
		myPort = * (TObjectId*) ( (Byte*) this + 0x8c);
		HLOG (1, "  Port: %04x, Server port: %04x\n", myPort, id);
	} else {
		printf ("Blunt server not running!\n");
		r = kCommErrResourceNotAvailable;
	}
	
	BindComplete (r);
}

void TRFCOMMTool::ConnectStart ()
{
	BluntConnectionCommand* command = new BluntConnectionCommand ();
	
	HLOG (1, "TRFCOMMTool::ConnectStart\n");
	fSavedDataAmount = 0;
	fSavedData = new UByte[MAX_SAVE];
	command->fToolPort = * (TObjectId*) ( (Byte*) this + 0x8c);
	memcpy (command->fBdAddr, fPeerBdAddr, 6);
	command->fTargetLayer = L_RFCOMM;
	command->fL2CAPProtocol = PSM_RFCOMM;
	command->fRFCOMMPort = fPeerRFCOMMPort;
	memcpy (command->fLinkKey, fLinkKey, sizeof (fLinkKey));
	fServerPort.Send (command, sizeof (*command), kNoTimeout, M_COMMAND);
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
	BluntDisconnectCommand* command = new BluntDisconnectCommand ();
	
	command->fToolPort = * (TObjectId*) ( (Byte*) this + 0x8c);
	memcpy (command->fBdAddr, fPeerBdAddr, 6);
	command->fHCIHandle = fHCIHandle;
	fServerPort.Send (command, sizeof (*command), kNoTimeout, M_COMMAND);
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
	HLOG (1, "TRFCOMMTool::GetBytes %d (pos %d), saved %d\n", list->GetSize (), list->Position (), fSavedDataAmount);
	Long n;

	fGetBuffer = list;
	if (fSavedDataAmount > 0) {
		HLOG (1, "  Returning %d saved bytes\n", fSavedDataAmount);
		n = fGetBuffer->Putn (fSavedData, fSavedDataAmount);
		HLOG (1, "  Returned %d of %d (pos %d)\n", n, fSavedDataAmount, fGetBuffer->Position ());
		if (fSavedDataAmount > n) {
			HLOG (1, "  %d bytes still left\n", fSavedDataAmount - n);
			memcpy (fSavedData, fSavedData + n, fSavedDataAmount - n);
		}
		fSavedDataAmount -= n;
	}
	if (fGetBuffer->AtEOF ())  {
		HLOG (1, "  Get complete\n");
		GetComplete (noErr);
	}
	return noErr;
}

NewtonErr TRFCOMMTool::GetFramedBytes (CBufferList *)
{
	HLOG (1, "TRFCOMMTool::GetFramedBytes\n");
	return noErr;
}

/*
void TRFCOMMTool::GetBytesImmediate (CBufferList* clientBuffer, Size threshold)
{
	Long n;

	HLOG (1, "TRFCOMMTool::GetBytesImmediate (%d %d %d)\n", clientBuffer->GetSize(), threshold, fSavedDataAmount);
	if (fSavedDataAmount > threshold) {
		n = clientBuffer->Putn (fSavedData, threshold);
		memcpy (fSavedData, fSavedData + n, fSavedDataAmount - n);
		fSavedDataAmount -= n;
		GetComplete (noErr);
	} else {
		GetComplete (noErr, false, 0);
	}
}
*/

void TRFCOMMTool::GetComplete (NewtonErr result, Boolean endOfFrame, ULong getBytesCount)
{
	HLOG (1, "TRFCOMMTool::GetComplete %d %d %d\n", result, endOfFrame, getBytesCount);
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
	BluntDataCommand* command = new BluntDataCommand ();

	HLOG (1, "TRFCOMMTool::SendPendingData %d\n", fPutBuffer->GetSize ());
	command->fData = fPutBuffer;
	command->fHCIHandle = fHCIHandle;
	command->fRFCOMMPort = fPeerRFCOMMPort;
	command->fHandler = fDataHandler;
	fServerPort.Send (command, sizeof (*command), kNoTimeout, M_COMMAND);
}
void TRFCOMMTool::Log (int logLevel, char *format, ...)
{
    va_list args;
    char buffer[128];
    
    if (fLogLevel >= logLevel) {
        va_start (args, format);
        vsprintf (buffer, format, args);
        va_end (args);
//		printf (buffer);
		BluntLogCommand* command = new BluntLogCommand ((UByte *) buffer, strlen (buffer));
		fServerPort.Send (command, sizeof (*command), kNoTimeout, M_COMMAND);
    }
}


/*
UByte* TRFCOMMTool::GetCommEvent()
{
	HLOG (1, "TRFCOMMTool::GetCommEvent\n");
	return TCommTool::GetCommEvent ();
}

void TRFCOMMTool::DoControl(ULong opCode, ULong msgType)
{
	HLOG (1, "TRFCOMMTool::DoControl %d %d\n", opCode, msgType);
	TCommTool::DoControl (opCode, msgType);
}

void TRFCOMMTool::DoStatus(unsigned long a, unsigned long b)
{
	HLOG (1, "TRFCOMMTool::DoStatus %d %d\n", a, b);
	TCommTool::DoStatus (a, b);
}

void TRFCOMMTool::HandleReply (ULong userRefCon, ULong msgType)
{
	HLOG (1, "TRFCOMMTool::HandleReply %d %d\n", userRefCon, msgType);
	TCommTool::DoControl (userRefCon, msgType);
}

ULong TRFCOMMTool::ProcessOptions (TCommToolOptionInfo* option)
{
	HLOG (1, "TRFCOMMTool::ProcessOptions %d\n", option->fOptionCount);
	return TCommTool::ProcessOptions (option);
}

void TRFCOMMTool::ProcessOption (TOption* theOption, ULong label, ULong opcode)
{
	HLOG (1, "TRFCOMMTool::ProcessOption %c %d\n", label, opcode);
	TCommTool::ProcessOption (theOption, label, opcode);
}

void TRFCOMMTool::ProcessGetBytesOptionStart (TOption* theOption, ULong label, ULong opcode)
{
	HLOG (1, "TRFCOMMTool::ProcessGetBytesOptionStart %c %d\n", label, opcode);
	TCommTool::ProcessGetBytesOptionStart (theOption, label, opcode);
}

void TRFCOMMTool::OptionMgmt (TCommToolOptionMgmtRequest *request)
{
	HLOG (1, "TRFCOMMTool::OptionMgmt\n");
	TCommTool::OptionMgmt (request);
}
*/
