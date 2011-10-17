#include "EventsCommands.h"

BluntEvent::BluntEvent(BluntEventType type, NewtonErr result)
{
	fAEventID = kBluntEventId;
	fAEventClass = kBluntEventClass;
	Init();
	fOriginalEvent = this;
	fType = type;
	fResult = result;
	fDelete = true;
}

BluntEvent::~BluntEvent()
{
}

BluntInquiryResultEvent::BluntInquiryResultEvent (NewtonErr result):
	BluntEvent (E_INQUIRY_RESULT, result)
{
}

BluntLinkKeyNotificationEvent::BluntLinkKeyNotificationEvent(NewtonErr result, UByte *addr, UByte *key):
	BluntEvent (E_LINK_KEY_NOTIFICATION, result)
{
	if (result == noErr) {
		memcpy (fBdAddr, addr, 6);
		memcpy (fLinkKey, key, 16);
	}
}

BluntNameRequestResultEvent::BluntNameRequestResultEvent (NewtonErr result, UChar* name, UByte* addr):
	BluntEvent (E_NAME_REQUEST_RESULT, result)
{
	strncpy((Char*) fName, (Char*) name, 64);
	fName[63] = 0;
	memcpy(fBdAddr, addr, 6);
}

BluntCommandCompleteEvent::BluntCommandCompleteEvent (NewtonErr result, ULong s):
	BluntEvent (E_COMMAND_COMPLETE, result)
{
	fStatus = s;
}

BluntConnectionCompleteEvent::BluntConnectionCompleteEvent (NewtonErr s):
	BluntEvent (E_CONNECTION_COMPLETE, s)
{
}

BluntDisconnectCompleteEvent::BluntDisconnectCompleteEvent (NewtonErr s):
	BluntEvent (E_DISCONNECT_COMPLETE, s)
{
}

BluntCommand::BluntCommand(BluntCommandType type)
{
	fType = type;
	fOriginalCommand = this;
	fDelete = false;
}

BluntCommand::~BluntCommand()
{
}

BluntLogCommand::BluntLogCommand (UByte *data, ULong size):
	BluntCommand (C_LOG)
{
	memcpy (fData, data, size);
	fSize = size;
}