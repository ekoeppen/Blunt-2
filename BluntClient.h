#ifndef __BLUNTCLIENT_H
#define __BLUNTCLIENT_H

#include <NewtonTime.h>
#include <AEvents.h>
#include <AEventHandler.h>
#include <UserTasks.h>
#include <NewtonScript.h>

class BluntClient: public TAEventHandler
{
public:
						BluntClient (RefArg blunt, TObjectId serverPort);
						~BluntClient ();
	
	void				AEHandlerProc (TUMsgToken* token, ULong* size, TAEvent* event);
	void				AECompletionProc (TUMsgToken* token, ULong* size, TAEvent* event);
	void				IdleProc (TUMsgToken* token, ULong* size, TAEvent* event);
	
	void				Reset (Char* name);
	void				Discover (UByte time, UByte amount);
	void				CancelDiscover ();
	void				NameRequest (UByte* bdAddr, ULong psRepMode, ULong psMode);
	void				Pair (UByte* bdAddr, Char *PIN, ULong psRepMode, ULong psMode);
	void				GetServices (UByte* bdAddr, ULong psRepMode, ULong psMode, UByte* linkKey);
	void				Stop ();
	void				Connect (UByte* bdAddr, ULong psRepMode, ULong psMode, UByte rfcommPort, UByte* linkKey);
	void				Disconnect (UByte* bdAddr);
	
	void				SendInquiryInfo (BluntInquiryResultEvent* event);
	void				SendNameRequestInfo (BluntNameRequestResultEvent* event);
	void				SendLinkKeyInfo (BluntLinkKeyNotificationEvent* event);
	void				SendServiceInfo (BluntServiceResultEvent* event);
    void                SendStatusInfo (BluntStatusEvent* event);
	
	void				SetLogLevel (UByte client, UByte server, UByte hci, UByte l2cap, UByte sdp, UByte rfcomm);
    void                Status ();
    void                Log (int logLevel, char* format, ...);
	
	ULong				fLogLevel;
	RefStruct*			fBlunt;
	TUPort				fServerPort;
};

#endif
