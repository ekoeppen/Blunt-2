#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "BluntServer.h"
#include "BluntClient.h"
#include "L2CAP.h"

extern "C" _sys_write (int fd, char* data, int len);

#define DEFAULT_LOGLEVEL 0

BluntClient::BluntClient (RefArg blunt, TObjectId server)
{
	fLogLevel = DEFAULT_LOGLEVEL;
	HLOG (1, "BluntClient::BluntClient\n");
	Init (kBluntEventId, kBluntEventClass);
	fBlunt = new RefStruct (blunt);
	fServerPort = server;
}

BluntClient::~BluntClient ()
{
	delete fBlunt;
}

void BluntClient::AEHandlerProc (TUMsgToken* token, ULong* size, TAEvent* event)
{
	HLOG (1, "BluntClient::AEHandlerProc %08x\n", event);
	switch (((BluntEvent*) event)->fType) {
		case E_RESET_COMPLETE:
			HLOG (1, "  Reset complete\n");
			NSSendIfDefined (*fBlunt, SYM (MResetCallback));
			break;
		case E_INQUIRY_RESULT:
			SendInquiryInfo ((BluntInquiryResultEvent *) event);
			break;
		case E_NAME_REQUEST_RESULT:
			SendNameRequestInfo ((BluntNameRequestResultEvent*) event);
			break;
		case E_LINK_KEY_NOTIFICATION:
			SendLinkKeyInfo ((BluntLinkKeyNotificationEvent *) event);
			break;
		case E_SERVICE_RESULT:
			SendServiceInfo ((BluntServiceResultEvent *) event);
			break;
		case E_STATUS:
			SendStatusInfo ((BluntStatusEvent *) event);
			break;
	}
	delete ((BluntEvent*) event)->fOriginalEvent;
}

void BluntClient::AECompletionProc (TUMsgToken* token, ULong* size, TAEvent* event)
{
	HLOG (1, "BluntClient::AECompletionProc %08x\n", event);
}

void BluntClient::IdleProc (TUMsgToken* token, ULong* size, TAEvent* event)
{
	HLOG (1, "BluntClient::AEIdlerProc\n");
}

void BluntClient::Stop ()
{
	fServerPort.Send (NULL, 0, kNoTimeout, M_QUIT);
}

void BluntClient::Reset (Char* name)
{
	BluntResetCommand command;
	strcpy (command.fName, name);
	fServerPort.Send (&command, sizeof (command), kNoTimeout, M_COMMAND);
}

void BluntClient::Discover (UByte time, UByte amount)
{
	BluntInquiryCommand command;
	command.fTime = time;
	command.fAmount = amount;
	fServerPort.Send (&command, sizeof (command), kNoTimeout, M_COMMAND);
}

void BluntClient::CancelDiscover ()
{
	BluntInquiryCancelCommand command;
	fServerPort.Send (&command, sizeof (command), kNoTimeout, M_COMMAND);
}

void BluntClient::NameRequest (UByte* bdAddr, ULong psRepMode, ULong psMode)
{
	BluntNameRequestCommand command;
	memcpy (command.fBdAddr, bdAddr, 6);
	command.fPSRepMode = psRepMode;
	command.fPSMode = psMode;
	fServerPort.Send (&command, sizeof (command), kNoTimeout, M_COMMAND);
}

void BluntClient::Pair (UByte* bdAddr, Char *PIN, ULong psRepMode, ULong psMode)
{
	BluntInitiatePairingCommand command;
	memcpy (command.fBdAddr, bdAddr, 6);
	strcpy (command.fPIN, PIN);
	command.fPSRepMode = psRepMode;
	command.fPSMode = psMode;
	fServerPort.Send (&command, sizeof (command), kNoTimeout, M_COMMAND);
}

void BluntClient::GetServices (UByte* bdAddr, ULong psRepMode, ULong psMode, UByte* linkKey)
{
	BluntServiceRequestCommand command;
	memcpy (command.fBdAddr, bdAddr, 6);
	command.fPSRepMode = psRepMode;
	command.fPSMode = psMode;
	memcpy (command.fLinkKey, linkKey, sizeof (command.fLinkKey));
	fServerPort.Send (&command, sizeof (command), kNoTimeout, M_COMMAND);
}

void BluntClient::Connect (UByte* bdAddr, ULong psRepMode, ULong psMode, UByte rfcommPort, UByte* linkKey)
{
	BluntConnectionCommand command;
	command.fToolPort = 0;
	memcpy (command.fBdAddr, bdAddr, 6);
	command.fPSRepMode = psRepMode;
	command.fPSMode = psMode;
	command.fTargetLayer = L_RFCOMM;
	command.fL2CAPProtocol = PSM_RFCOMM;
	command.fRFCOMMPort = rfcommPort;
	memcpy (command.fLinkKey, linkKey, sizeof (command.fLinkKey));
	fServerPort.Send (&command, sizeof (command), kNoTimeout, M_COMMAND);
}

void BluntClient::Disconnect (UByte* bdAddr)
{
	BluntDisconnectCommand command;
	command.fToolPort = 0;
	memcpy (command.fBdAddr, bdAddr, 6);
	fServerPort.Send (&command, sizeof (command), kNoTimeout, M_COMMAND);
}

void BluntClient::SendInquiryInfo (BluntInquiryResultEvent* event)
{
	RefVar device;
	RefVar addr;
	
	HLOG (1, "BluntClient::SendInquiryInfo %d\n", event->fResult);
	if (event->fResult == noErr) {
		device = AllocateFrame ();
		addr = AllocateBinary (SYM (binary), 6);
		WITH_LOCKED_BINARY(addr, a)
		memcpy (a, event->fBdAddr, 6);
		END_WITH_LOCKED_BINARY(addr);
		SetFrameSlot (device, SYM (fBdAddr), addr);
		SetFrameSlot (device, SYM (fClass), MakeInt ((event->fClass[0] << 16) + (event->fClass[1] << 8) + event->fClass[2]));
		SetFrameSlot (device, SYM (fClockOffset), MakeInt ((event->fClockOffset[0] << 8) + event->fClockOffset[1]));
		SetFrameSlot (device, SYM (fPSRepMode), MakeInt (event->fPSRepMode));
		SetFrameSlot (device, SYM (fPSPeriodMode), MakeInt (event->fPSPeriodMode));
		SetFrameSlot (device, SYM (fPSMode), MakeInt (event->fPSMode));
		NSSendIfDefined (*fBlunt, SYM (MInquiryCallback), device);
	} else {
		NSSendIfDefined (*fBlunt, SYM (MInquiryCallback), NILREF);
	}
}

void BluntClient::SendNameRequestInfo (BluntNameRequestResultEvent* event)
{
	RefVar addr;
	
	HLOG (1, "BluntClient::SendNameRequestInfo (%s)\n", event->fName);
	addr = AllocateBinary (SYM (binary), 6);
	WITH_LOCKED_BINARY(addr, a)
	memcpy (a, event->fBdAddr, 6);
	END_WITH_LOCKED_BINARY(addr);
	NSSendIfDefined (*fBlunt, SYM (MNameRequestCallback), addr, MakeString ((char*) event->fName));
}

void BluntClient::SendLinkKeyInfo (BluntLinkKeyNotificationEvent* event)
{
	RefVar addr;
	RefVar key;
	
	HLOG (1, "BluntClient::SendLinkKeyInfo\n");
	addr = AllocateBinary (SYM (binary), 6);
	key = AllocateBinary (SYM (binary), 16);
	WITH_LOCKED_BINARY(addr, a)
	memcpy (a, event->fBdAddr, 6);
	END_WITH_LOCKED_BINARY(addr);
	WITH_LOCKED_BINARY(key, k)
	memcpy (k, event->fLinkKey, 16);
	END_WITH_LOCKED_BINARY(key);
	NSSendIfDefined (*fBlunt, SYM (MLinkKeyCallback), addr, key);
}

void BluntClient::SendServiceInfo (BluntServiceResultEvent* event)
{
	RefVar service;
	RefVar addr;
	
	HLOG (1, "BluntClient::SendServiceInfo %d\n", event->fResult);
	service = AllocateFrame ();
	SetFrameSlot (service, SYM (fResult), MakeInt (event->fResult));
	if (event->fResult == noErr) {
		addr = AllocateBinary (SYM (binary), 6);
		WITH_LOCKED_BINARY(addr, a)
		memcpy (a, event->fBdAddr, 6);
		END_WITH_LOCKED_BINARY(addr);
		SetFrameSlot (service, SYM (fBdAddr), addr);
		SetFrameSlot (service, SYM (fService), MakeInt (event->fServiceUUID));
		SetFrameSlot (service, SYM (fPort), MakeInt (event->fServicePort));
	}
	NSSendIfDefined (*fBlunt, SYM (MServicesCallback), service);
}

void BluntClient::SetLogLevel (UByte level[5])
{
	BluntSetLogLevelCommand command;
	memcpy (command.fLevel, level, sizeof (command.fLevel));
	fServerPort.Send (&command, sizeof (command), kNoTimeout, M_COMMAND);
}

void BluntClient::Status ()
{
	BluntStatusCommand command;
	fServerPort.Send (&command, sizeof (command), kNoTimeout, M_COMMAND);
}

void BluntClient::SendStatusInfo (BluntStatusEvent* event)
{
	HLOG (0, "BluntClient::SendStatusInfo %d\n", event->fResult);
	NSSendIfDefined (*fBlunt, SYM (MStatusCallback), MakeInt (event->fResult));
}

void BluntClient::Log (int logLevel, char *format, ...)
{
    va_list args;
    char buffer[128];
    
    if (fLogLevel >= logLevel) {
        va_start (args, format);
        vsprintf (buffer, format, args);
        va_end (args);
#ifdef USE_HAMMER_LOG
        _sys_write (1, buffer, strlen (buffer));
#else
        BluntLogCommand command ((UByte *) buffer, strlen (buffer));
        fServerPort.Send (&command, sizeof (command), kNoTimeout, M_COMMAND);
#endif
    }
}

