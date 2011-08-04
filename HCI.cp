#define DEFAULT_LOGLEVEL 0

#include <AEvents.h>
#include <AEventHandler.h>
#include <BufferSegment.h>
#include <CommManagerInterface.h>
#include <CommTool.h>
#include <SerialOptions.h>
#include <SerialChipV2.h>
#include <CommOptions.h>

#include "BluntServer.h"
#include "HCI.h"
#include "L2CAP.h"
#include "EventsCommands.h"

#define EVT_HCI_INQUIRY_COMPLETE							0x01
#define EVT_HCI_INQUIRY_RESULT								0x02
#define EVT_HCI_CONNECTION_COMPLETE							0x03
#define EVT_HCI_CONNECTION_REQUEST							0x04
#define EVT_HCI_DISCONNECTION_COMPLETE						0x05
#define EVT_HCI_AUTHENTICATION_COMPLETE						0x06
#define EVT_HCI_REMOTE_NAME_REQUEST_COMPLETE				0x07
#define EVT_HCI_ENCRYPTION_CHANGE							0x08
#define EVT_HCI_CHANGE_CONNECTION_LINK_KEY_COMPLETE			0x09
#define EVT_HCI_MASTER_LINK_KEY_COMPLETE					0x0a
#define EVT_HCI_READ_REMOTE_SUPPORTED_FEATURES_COMPLETE		0x0b
#define EVT_HCI_READ_REMOTE_VERSION_INFORMATION_COMPLETE	0x0c
#define EVT_HCI_QOS_SETUP_COMPLETE							0x0d
#define EVT_HCI_COMMAND_COMPLETE							0x0e
#define EVT_HCI_COMMAND_STATUS								0x0f
#define EVT_HCI_HARDWARE_ERROR								0x10
#define EVT_HCI_FLUSH_OCCURRED								0x11
#define EVT_HCI_ROLE_CHANGE									0x12
#define EVT_HCI_NUMBER_OF_COMPLETED_PACKETS 				0x13
#define EVT_HCI_MODE_CHANGE									0x14
#define EVT_HCI_RETURN_LINK_KEYS							0x15
#define EVT_HCI_PIN_CODE_REQUEST							0x16
#define EVT_HCI_LINK_KEY_REQUEST							0x17
#define EVT_HCI_LINK_KEY_NOTIFICATION 						0x18
#define EVT_HCI_LOOPBACK_COMMAND							0x19
#define EVT_HCI_DATA_BUFFER_OVERFLOW 						0x1a
#define EVT_HCI_MAX_SLOTS_CHANGE							0x1b
#define EVT_HCI_READ_CLOCK_OFFSET_COMPLETE 					0x1c
#define EVT_HCI_CONNECTION_PACKET_TYPE_CHANGED				0x1d
#define EVT_HCI_QO_S_VIOLATION								0x1e
#define EVT_HCI_PAGE_SCAN_MODE_CHANGE 						0x1f
#define EVT_HCI_PAGE_SCAN_REPETITION_MODE_CHANGE			0x20
#define EVT_HCI_TIMER                                       0xff

enum {
	HCI_ANY =								 0,
	HCI_IDLE =								 1,
	HCI_RESET =								 2,
	HCI_INQUIRY =							 3,
	HCI_INQUIRY_CANCEL =					 4,
	HCI_CHANGE_LOCAL_NAME =					 5,
	HCI_READ_BDADDR =						 6,
	HCI_WRITE_SCAN_ENABLE =					 7,
	HCI_WRITE_INQ_SCAN =					 8,
	HCI_WRITE_PAGE_SCAN =					 9,
	HCI_ACCEPT_CONNECTION_REQUEST =			10,
	HCI_CREATE_CONNECTION =					11,
	HCI_DISCONNECT =						12,
	HCI_LINK_KEY_REQUEST_REPLY =			13,
	HCI_LINK_KEY_REQUEST_NEGATIVE_REPLY =	14,
	HCI_PIN_CODE_REQUEST_REPLY =			15,
	HCI_HOST_BUFFER_SIZE =					16,
	HCI_READ_BUFFER_SIZE =					17,
	HCI_GET_LINK_QUALITY =					18,
	HCI_WRITE_CLASS_OF_DEVICE =				19,
	HCI_IPAIR_CONNECT =						20,
	HCI_IPAIR_AUTH =						21,
    HCI_STATUS_BD_ADDR =                    22
};

enum {
	A_READ_BUFFER_SIZE =					 0,
	A_HOST_BUFFER_SIZE =					 1,
	A_WRITE_SCAN_ENABLE =					 2,
	A_WRITE_CLASS_OF_DEVICE =				 3,
	A_CHANGE_LOCAL_NAME =					 4,
	A_RESET_COMPLETE =						 5,
	A_CONNECT_STATUS =						 6,
	A_CONNECTED =							 7,
	A_HCI_PIN_CODE_REPLY =					 8,
	A_SET_LINK_KEY =						 9,
	A_HCI_LINK_KEY_REPLY =					10,
	A_DISCONNECTED =						11,
	A_ACCEPT =								12,
	A_ACCEPTED =							13,
	A_INQUIRY_RESULT =						14,
	A_INQUIRY_COMPLETE =					15,
	A_NAME_RESULT =							16,
	A_IPAIR_CONNECT_STATUS =				17,
	A_IPAIR_AUTH_REQUEST =					18,
	A_IPAIR_KEY_REQUEST =					19,
	A_IPAIR_PIN_REQUEST =					20,
	A_IPAIR_AUTH_COMPLETE =					21,
	A_COMPLETED_PACKETS =					22,
    A_STATUS_BD_ADDR =                      23,
    A_STATUS_TIMEOUT =                      24,
	A_NONE =								25
};

static const int kLinkEvents[][2] = {
	{EVT_HCI_DISCONNECTION_COMPLETE, 3},
	{EVT_HCI_AUTHENTICATION_COMPLETE, 3},
	{EVT_HCI_ENCRYPTION_CHANGE, 3},
	{EVT_HCI_CHANGE_CONNECTION_LINK_KEY_COMPLETE, 3},
	{EVT_HCI_MASTER_LINK_KEY_COMPLETE, 3},
	{EVT_HCI_READ_REMOTE_SUPPORTED_FEATURES_COMPLETE, 3},
	{EVT_HCI_READ_REMOTE_VERSION_INFORMATION_COMPLETE, 3},
	{EVT_HCI_QOS_SETUP_COMPLETE, 3},
	{EVT_HCI_FLUSH_OCCURRED, 2},
	{EVT_HCI_MODE_CHANGE, 3},
	{EVT_HCI_MAX_SLOTS_CHANGE, 2},
	{EVT_HCI_READ_CLOCK_OFFSET_COMPLETE, 3},
	{EVT_HCI_CONNECTION_PACKET_TYPE_CHANGED, 3},
	{EVT_HCI_QO_S_VIOLATION, 2},
};

static const int kGeneralEvents[] = {
	EVT_HCI_CONNECTION_COMPLETE,
	EVT_HCI_DISCONNECTION_COMPLETE,
	EVT_HCI_INQUIRY_COMPLETE,
	EVT_HCI_INQUIRY_RESULT,
	EVT_HCI_CONNECTION_REQUEST,
	EVT_HCI_REMOTE_NAME_REQUEST_COMPLETE,
	EVT_HCI_COMMAND_COMPLETE,
	EVT_HCI_COMMAND_STATUS,
	EVT_HCI_HARDWARE_ERROR,
	EVT_HCI_ROLE_CHANGE,
	EVT_HCI_NUMBER_OF_COMPLETED_PACKETS,
	EVT_HCI_RETURN_LINK_KEYS,
	EVT_HCI_PIN_CODE_REQUEST,
	EVT_HCI_LINK_KEY_REQUEST,
	EVT_HCI_LINK_KEY_NOTIFICATION,
	EVT_HCI_LOOPBACK_COMMAND,
	EVT_HCI_DATA_BUFFER_OVERFLOW,
	EVT_HCI_PAGE_SCAN_MODE_CHANGE,
	EVT_HCI_PAGE_SCAN_REPETITION_MODE_CHANGE,
};

static const int kStates[][4] = {
	/* Reset states */
	{HCI_RESET,					EVT_HCI_COMMAND_COMPLETE,		A_READ_BUFFER_SIZE,			HCI_READ_BUFFER_SIZE},
	{HCI_READ_BUFFER_SIZE,		EVT_HCI_COMMAND_COMPLETE,		A_HOST_BUFFER_SIZE,			HCI_HOST_BUFFER_SIZE},
	{HCI_HOST_BUFFER_SIZE,		EVT_HCI_COMMAND_COMPLETE,		A_WRITE_SCAN_ENABLE,		HCI_WRITE_SCAN_ENABLE},
	{HCI_WRITE_SCAN_ENABLE,		EVT_HCI_COMMAND_COMPLETE,		A_WRITE_CLASS_OF_DEVICE,	HCI_WRITE_CLASS_OF_DEVICE},
	{HCI_WRITE_CLASS_OF_DEVICE,	EVT_HCI_COMMAND_COMPLETE,		A_CHANGE_LOCAL_NAME,		HCI_CHANGE_LOCAL_NAME},
	{HCI_CHANGE_LOCAL_NAME,		EVT_HCI_COMMAND_COMPLETE,		A_RESET_COMPLETE,			HCI_IDLE},
	
	/* Connect states */
	{HCI_CREATE_CONNECTION,		EVT_HCI_COMMAND_STATUS,			A_CONNECT_STATUS,			HCI_CREATE_CONNECTION},
	{HCI_CREATE_CONNECTION,		EVT_HCI_CONNECTION_COMPLETE,	A_CONNECTED,				HCI_IDLE},
	
	/* Disconnect states */
	{HCI_ANY,					EVT_HCI_DISCONNECTION_COMPLETE,	A_DISCONNECTED,				HCI_IDLE},
	
	/* Accept states */
	{HCI_ANY,					EVT_HCI_CONNECTION_REQUEST,			A_ACCEPT,				HCI_ACCEPT_CONNECTION_REQUEST},
	{HCI_ACCEPT_CONNECTION_REQUEST,		EVT_HCI_COMMAND_STATUS,		A_CONNECT_STATUS,		HCI_ACCEPT_CONNECTION_REQUEST},
	{HCI_ACCEPT_CONNECTION_REQUEST,		EVT_HCI_CONNECTION_COMPLETE,	A_ACCEPTED,			HCI_IDLE},
	
	/* Discover states */
	{HCI_ANY,					EVT_HCI_INQUIRY_RESULT,			A_INQUIRY_RESULT,			HCI_INQUIRY},
	{HCI_INQUIRY,				EVT_HCI_INQUIRY_COMPLETE,		A_INQUIRY_COMPLETE,			HCI_IDLE},
	{HCI_ANY,					EVT_HCI_REMOTE_NAME_REQUEST_COMPLETE,	A_NAME_RESULT,		HCI_IDLE},
	{HCI_INQUIRY_CANCEL,		EVT_HCI_COMMAND_COMPLETE,		A_NONE,						HCI_IDLE},
	
	/* Pairing states (initiating) */
	{HCI_IPAIR_CONNECT,			EVT_HCI_COMMAND_STATUS,			A_IPAIR_CONNECT_STATUS,		HCI_IPAIR_CONNECT},
	{HCI_IPAIR_CONNECT,			EVT_HCI_CONNECTION_COMPLETE,	A_IPAIR_AUTH_REQUEST,		HCI_IPAIR_AUTH},
	{HCI_IPAIR_AUTH,			EVT_HCI_LINK_KEY_REQUEST,		A_IPAIR_KEY_REQUEST,		HCI_IPAIR_AUTH},
	{HCI_IPAIR_AUTH,			EVT_HCI_PIN_CODE_REQUEST,		A_IPAIR_PIN_REQUEST,		HCI_IPAIR_AUTH},
	{HCI_IPAIR_AUTH,			EVT_HCI_LINK_KEY_NOTIFICATION,	A_SET_LINK_KEY,				HCI_IPAIR_AUTH},
	{HCI_IPAIR_AUTH,			EVT_HCI_AUTHENTICATION_COMPLETE,	A_IPAIR_AUTH_COMPLETE,	HCI_IDLE},
    
    /* Status states */
    {HCI_STATUS_BD_ADDR,        EVT_HCI_COMMAND_COMPLETE,       A_STATUS_BD_ADDR,           HCI_IDLE},
    {HCI_STATUS_BD_ADDR,        EVT_HCI_TIMER,                  A_STATUS_TIMEOUT,           HCI_IDLE},
	
	/* Other states */
	{HCI_ANY,					EVT_HCI_PIN_CODE_REQUEST,		A_HCI_PIN_CODE_REPLY,		HCI_ANY},
	{HCI_ANY,					EVT_HCI_LINK_KEY_NOTIFICATION,	A_SET_LINK_KEY,				HCI_ANY},
	{HCI_ANY,					EVT_HCI_LINK_KEY_REQUEST,		A_HCI_LINK_KEY_REPLY,		HCI_ANY},
	{HCI_ANY,					EVT_HCI_MAX_SLOTS_CHANGE,		A_NONE,						HCI_ANY},
	{HCI_ANY,					EVT_HCI_NUMBER_OF_COMPLETED_PACKETS,	A_COMPLETED_PACKETS,	HCI_ANY},
};
	
HCI::HCI (void)
{
	fServer = NULL;
	fLogLevel = DEFAULT_LOGLEVEL;
	fOutstandingPackets = 0;
	fHCIWindowSize = 0;
	fPacketLength = 0;
	fConnectionHandle = -1;
	fHandlerType = H_HCI;
	fState = HCI_IDLE;
	strcpy (fPairPINCode, "1234");
	strcpy (fName, "Newton");
	
	HLOG (1, "HCI::HCI %08x\n", this);
}

HCI::~HCI (void)
{
	int i;
	
	HLOG (1, "HCI::~HCI\n");
	for (i = 0; i < fNumHandlers; i++) {
		delete (L2CAP *) fHandlers[i];
	}
}

int HCI::Action (int action, void *eventData)
{
	int newState;
	int i;
	HCI *hci;
	L2CAP* controlL2cap;
	
	newState = HCI_ANY;
	HLOG (1, "HCI::Action (%08x) %d %d\n", this, fState, action);
	switch (action) {
		case A_READ_BUFFER_SIZE:
			ReadBufferSize ();
			break;
		case A_HOST_BUFFER_SIZE:
			fHCIBufferSize = fPacket[7] + (fPacket[8] << 8);
			fHCIWindowSize = fPacket[10];
			HLOG (1, "  Buffers: %d Size: %d\n", fHCIWindowSize, fHCIBufferSize);
			HostBufferSize ();
			break;
		case A_WRITE_SCAN_ENABLE:
			WriteScanEnable (0x03);
			break;
		case A_WRITE_CLASS_OF_DEVICE:
			WriteClassofDevice ((UByte *) "\020\001\024");
			break;
		case A_CHANGE_LOCAL_NAME:
			ChangeLocalName ((UByte *) fName);
			break;
		case A_RESET_COMPLETE:
			fConnectionState = CONN_IDLE;
			HLOG (0, "Reset complete\n");
			fServer->SendEvent (new BluntResetCompleteEvent (kResultOk));
			break;
		case A_CONNECT_STATUS:
			HLOG (1, "  Connect status: %d\n", fPacket[3]);
			if (fPacket[3] != 0x00) {
				fConnectionState = CONN_IDLE;
				newState = HCI_IDLE;
			}
			break;
		case A_CONNECTED:
			HLOG (1, "  Connection complete: %d\n", fPacket[3]);
			if (fPacket[3] == 0x00) {
				fOutstandingPackets = 0;
				CompleteConnection (noErr);
			} else {
				CompleteConnection (kCommErrConnectionAborted);
			}
			break;
		case A_ACCEPT:
			AcceptConnectionRequest (&fPacket[3], 0x01);
			break;
		case A_ACCEPTED:
			hci = new HCI ();
			controlL2cap = new L2CAP ();
			hci->fConnectionHandle = GET_SHORT (fPacket, 4);
			HLOG (1, "Accepted\n  HCI: %08x (%04x) L2CAP: %08x\n", hci, hci->fConnectionHandle, controlL2cap);
			hci->fTool = fTool;
			hci->fConnectionState = CONN_CONNECT;
			memcpy (hci->fPeerAddress, &fPacket[6], 6);
			strcpy (hci->fPairPINCode, fPairPINCode);
			hci->fControlChannel = this;
			hci->fHCIWindowSize = fHCIWindowSize;
			fServer->AddHandler (hci);
			controlL2cap->fLocalCID = CID_SIGNAL;
			controlL2cap->fRemoteCID = CID_SIGNAL;
			controlL2cap->fTool = fTool;
			hci->AddHandler ((Handler *) controlL2cap, fServer);
			fServer->Print ();
			break;
		case A_HCI_PIN_CODE_REPLY:
			PINCodeRequestReply (&fPacket[3], fPairPINCode);
			break;
		case A_SET_LINK_KEY:
			HLOG (1, "  Got link key for %02x:%02x:%02x...\n", fPacket[3], fPacket[4], fPacket[5]);
			fServer->SendEvent (new BluntLinkKeyNotificationEvent (kResultOk, &fPacket[3], &fPacket[9]));
			break;
		case A_HCI_LINK_KEY_REPLY:
			LinkKeyRequestReply (fPeerAddress, fLinkKey);
			break;
		case A_DISCONNECTED:
			CompleteDisconnect (GET_SHORT (fPacket, 4));
			break;
		case A_INQUIRY_RESULT: {
			for (i = 0; i < fPacket[3]; i++) {
				BluntInquiryResultEvent* event = new BluntInquiryResultEvent (kResultOk);
				memcpy (event->fBdAddr,			&fPacket[4 + i          * 6], 6);
				event->fPSRepMode =				 fPacket[4 + fPacket[3] * 6 + i];
				event->fPSPeriodMode =			 fPacket[4 + fPacket[3] * 6 + i + 1];
				event->fPSMode =				 fPacket[4 + fPacket[3] * 6 + i + 2];
				memcpy (event->fClass,			&fPacket[4 + fPacket[3] * (6 + 3) + i * 3], 3);
				memcpy (event->fClockOffset,	&fPacket[4 + fPacket[3] * (6 + 3 + 3) + i * 2], 2);
				fServer->SendEvent (event);
			}
			} break;
		case A_INQUIRY_COMPLETE:
			fServer->SendEvent (new BluntInquiryResultEvent (kResultComplete));
			break;
		case A_NAME_RESULT:
			if (fPacket[3] == 0) {
				fServer->SendEvent (new BluntNameRequestResultEvent (kResultOk, &fPacket[10], &fPacket[4]));
			} break;
		case A_IPAIR_CONNECT_STATUS:
			if (fPacket[3] != 0x00) {
				HLOG (1, "  Pair connect status: %d\n", fPacket[3]);
				fServer->SendEvent (new BluntLinkKeyNotificationEvent (kResultPairConnectFailed, NULL, NULL));
				CompleteDisconnect (fPairAddress);
			}
			break;
		case A_IPAIR_AUTH_REQUEST:
			if (fPacket[3] == 0x00 && (hci = GetHandler (&fPacket[6])) != NULL) {
				hci->fConnectionHandle = GET_SHORT (fPacket, 4);
				hci->fState = HCI_IPAIR_AUTH;
				AuthenticationRequested (hci->fConnectionHandle);
			} else {
				fServer->SendEvent (new BluntLinkKeyNotificationEvent (kResultPairConnectFailed, NULL, NULL));
				CompleteDisconnect (fPairAddress);
			}
			break;
		case A_IPAIR_KEY_REQUEST:
			LinkKeyRequestNegativeReply (&fPacket[3]);
			break;
		case A_IPAIR_PIN_REQUEST:
			PINCodeRequestReply (&fPacket[3], fPairPINCode);
			break;
		case A_IPAIR_AUTH_COMPLETE:
			HLOG (1, "  Pairing complete\n");
			Disconnect (0x13);
			break;
		case A_COMPLETED_PACKETS:
			TrackCompletedPackets ();
            break;
        case A_STATUS_BD_ADDR:
            HLOG (1, "  Read BD Address\n");
            if (fPacket[6] == 0) {
                fServer->SendEvent (new BluntStatusEvent (kResultOk));
            } else {
                fServer->SendEvent (new BluntStatusEvent (fPacket[6]));
            }
			HLOG (0, "HCI Status: %d\n", fPacket[6]);
			break;
        case A_STATUS_TIMEOUT:
            HLOG (0, "*** Timeout getting status\n");
			fServer->SendEvent (new BluntStatusEvent (kResultStatusTimeout));
			break;
	}
	return newState;
}

void HCI::Transition (ULong event, void *eventData)
{
	int i;
	int newState;
	
	HLOG (1, "HCI::Transition (%08x) %d %02x -> ", this, fState, event);
	for (i = 0; i < NUM_ELEMENTS(kStates); i++) {
		if ((fState == kStates[i][0] || kStates[i][0] == HCI_ANY) && event == kStates[i][1]) {
			HLOG (1, "%d\n", i);
			newState = Action (kStates[i][2], eventData);
			if (newState != HCI_ANY) {
				fState = newState;
			} else if (kStates[i][3] != HCI_ANY) {
				fState = kStates[i][3];
			}
			break;
		}
	}
	HLOG (1, "New state: %d\n", fState);
}

void HCI::HandleTimer (void *userData)
{
	HLOG (1, "HCI::HandleTimer\n");
	Transition (EVT_HCI_TIMER, userData);
}

Boolean	HCI::IsMyPacket (TExtendedCircleBuf *buffer)
{
	Boolean r = false;
	Short handle;
	UByte type;
	UByte event;
	int i;
	
	type = buffer->PeekByte (0);
	LOG2 ("HCI::IsMyPacket %08x %02x (%04x)\n", this, type, fConnectionHandle);
	if (type == 0x02) {
		handle = buffer->PeekByte (1) + (buffer->PeekByte (2) << 8);
		LOG2 ("  Handle: %04x\n", handle & 0x0fff);
		if ((handle & 0x0fff) == fConnectionHandle) r = true;
	} else if (type == 0x04) {
		event = buffer->PeekByte (1);
		LOG2 ("  Event: %02x\n", event);
		if (fConnectionHandle == -1) {
			for (i = 0; r == false && i < NUM_ELEMENTS(kGeneralEvents); i++) {
				if (event == kGeneralEvents[i]) r = true;
			}
		} else {
			for (i = 0; r == false && i < NUM_ELEMENTS(kLinkEvents); i++) {
				if (event == kLinkEvents[i][0]) {
					handle = buffer->PeekByte (kLinkEvents[i][1] + 1) + (buffer->PeekByte (kLinkEvents[i][1] + 2) << 8);
					LOG2 ("  Handle: %04x", handle);
					if (handle == fConnectionHandle) {
						r = true;
					}
				}
			}
		}
	}
	if (r) {
		LOG2 (" -> mine \n");
	} else {
		LOG2 ("\n");
	}
	return r;
}

Boolean	HCI::IsMyPacket ()
{
	Boolean r = false;
	Short handle;
	UByte type, hi, lo;
	UByte event;
	int i;
	
	fServer->PeekInputBuffer (&type, 0, sizeof (type));
	LOG2 ("HCI::IsMyPacket %08x %02x (%04x)\n", this, type, fConnectionHandle);
	if (type == 0x02) {
		fServer->PeekInputBuffer (&lo, 1, sizeof (lo));
		fServer->PeekInputBuffer (&hi, 2, sizeof (hi));
		handle = lo + (hi << 8);
		LOG2 ("  Handle: %04x\n", handle & 0x0fff);
		if ((handle & 0x0fff) == fConnectionHandle) r = true;
	} else if (type == 0x04) {
		fServer->PeekInputBuffer (&event, 1, sizeof (event));
		LOG2 ("  Event: %02x\n", event);
		if (fConnectionHandle == -1) {
			for (i = 0; r == false && i < NUM_ELEMENTS(kGeneralEvents); i++) {
				if (event == kGeneralEvents[i]) r = true;
			}
		} else {
			for (i = 0; r == false && i < NUM_ELEMENTS(kLinkEvents); i++) {
				if (event == kLinkEvents[i][0]) {
					fServer->PeekInputBuffer (&lo, kLinkEvents[i][1] + 1, sizeof (lo));
					fServer->PeekInputBuffer (&hi, kLinkEvents[i][1] + 2, sizeof (hi));
					handle = lo + (hi << 8);
					LOG2 ("  Handle: %04x", handle);
					if (handle == fConnectionHandle) {
						r = true;
					}
				}
			}
		}
	}
	if (r) {
		LOG2 (" -> mine \n");
	} else {
		LOG2 ("\n");
	}
	return r;
}

Boolean HCI::CheckPacketComplete (TExtendedCircleBuf *buffer, Boolean alwaysConsume)
{
	ULong needed;
	ULong total;
	Boolean complete;
	ULong len;
	UByte b;
	
	complete = false;
	needed = 0;
	
	len = buffer->BufferCount ();
	b = buffer->PeekByte (0);
	LOG2 ("HCI::CheckPacketComplete %02x (%d)\n", b, len);
	if (len > 0) {
		switch (b) {
			case 0x02:
				needed = 5;
				if (len >= 5) {
					needed += buffer->PeekByte (3) + (buffer->PeekByte (4) << 8);
				} else {
					LOG2 ("  Incomplete data packet header\n");
				}
				break;
			case 0x04:
				needed = 3;
				if (len >= 3) {
					needed += buffer->PeekByte (2);
				} else {
					LOG2 ("  Incomplete event packet header\n");
				}
				break;
			default:
				break;
		}
		
		LOG2 ("  Got: %d, needed: %d\n", len, needed);
		if (needed <= len && (alwaysConsume || IsMyPacket (buffer))) {
			complete = true;
			fPacketLength = needed;
			if (alwaysConsume) {
				HLOG (1, "HCI: Discarded %d bytes\n", needed);
				buffer->CopyOut (fPacket, &needed, &len);
				fPacketLength = 0;
			}
			if (fPacketLength <= sizeof (fPacket)) {
				buffer->CopyOut (fPacket, &needed, &len);
			} else {
				HLOG (1, "*** HCI Overflow: %d\n", fPacketLength);
				buffer->Reset ();
			}
		}
	} else {
		LOG2 ("  Incomplete header\n");
	}
	
	return complete;
}

Boolean HCI::CheckPacketComplete (Boolean alwaysConsume)
{
	ULong needed;
	ULong total;
	Boolean complete;
	ULong len;
	UByte type, hi, lo;
	
	complete = false;
	needed = 0;
	
	len = fServer->InputBufferCount ();
	fServer->PeekInputBuffer (&type, 0, sizeof (type));
	HLOG (2, "HCI::CheckPacketComplete %02x (%d)\n", type, len);
	if (len > 0) {
		switch (type) {
			case 0x02:
				needed = 5;
				if (len >= 5) {
					fServer->PeekInputBuffer (&lo, 3, sizeof (lo));
					fServer->PeekInputBuffer (&hi, 4, sizeof (hi));
					needed += (hi << 8) + lo;
				} else {
					HLOG (2, "  Incomplete data packet header\n");
				}
				break;
			case 0x04:
				needed = 3;
				if (len >= 3) {
					fServer->PeekInputBuffer (&lo, 2, sizeof (lo));
					needed += lo;
				} else {
					HLOG (2, "  Incomplete event packet header\n");
				}
				break;
			default:
				break;
		}
		
		HLOG (2, "  Got: %d, needed: %d\n", len, needed);
		if (needed <= len && (alwaysConsume || IsMyPacket ())) {
			complete = true;
			fPacketLength = needed;
			if (alwaysConsume) {
				HLOG (1, "HCI: Discarded %d bytes\n", fPacketLength);
				fServer->ConsumeInputBuffer (fPacketLength);
				fPacketLength = 0;
			}
			if (fPacketLength <= sizeof (fPacket)) {
				fServer->ReadInputBuffer (fPacket, fPacketLength);
			} else {
				HLOG (0, "*** HCI Overflow: %d\n", fPacketLength);
				fServer->ResetInputBuffer ();
			}
		}
	} else {
		HLOG (2, "  Incomplete header\n");
	}
	
	return complete;
}

Boolean HCI::HandleData (TExtendedCircleBuf *buffer)
{
	NewtonErr r;
	Boolean s = false;
	
	HLOG (2, "HCI::HandleData %08x\n", this);
	if (CheckPacketComplete (buffer, false)) {
		HLOG (1, ">>> HCI %08x handling, state %d Event code: 0x%02x\n", this, fConnectionState, fPacket[1]);
		s = true;
		if (fPacket[0] == 0x04) {
			Transition (fPacket[1], NULL);
		} else if (fPacket[0] == 0x02) {
			ProcessACLData ();
		} else {
			s = false;
		}
	}
	
	return s;
}

Boolean HCI::HandleData ()
{
	NewtonErr r;
	Boolean s = false;
	
	HLOG (2, "HCI::HandleData %08x\n", this);
	if (CheckPacketComplete (false)) {
		HLOG (1, ">>> HCI %08x handling, state %d Event code: 0x%02x\n", this, fConnectionState, fPacket[1]);
		s = true;
		if (fPacket[0] == 0x04) {
			Transition (fPacket[1], NULL);
		} else if (fPacket[0] == 0x02) {
			ProcessACLData ();
		} else {
			s = false;
		}
	}
	
	return s;
}

void HCI::RemoveUnhandledPackets (TExtendedCircleBuf *buffer)
{
	int i;
	
	HLOG (1, "HCI::RemoveUnhandledPackets)\n");
	/*
	for (i = 0; i < length && i < 256; i++) {
		HLOG (1, "%02x ", data[i]);
		if ((i + 1) % 16 == 0) HLOG (1, "\n  ");
	}
	HLOG (1, "\n"); */

	CheckPacketComplete (buffer, true);
}

void HCI::RemoveUnhandledPackets ()
{
	int i;
	
	HLOG (1, "HCI::RemoveUnhandledPackets)\n");
	/*
	for (i = 0; i < length && i < 256; i++) {
		HLOG (1, "%02x ", data[i]);
		if ((i + 1) % 16 == 0) HLOG (1, "\n  ");
	}
	HLOG (1, "\n"); */

	CheckPacketComplete (true);
}

void HCI::SetConnectionData ()
{
	HCI *h;
	BluntConnectionCompleteEvent *e;
	int i;
	Short handle;
	
	handle = GET_SHORT (fPacket, 4);
	HLOG (1, "HCI::SetConnectionData %04x\n", handle);
	for (i = 1; i < fServer->fNumHandlers; i++) {
		h = (HCI *) fServer->fHandlers[i];
		HLOG (2, " %d %d ", i, h->fConnectionState);
		HLOG (2, " %02x %02x %02x", i, h->fPeerAddress[0], h->fPeerAddress[1], h->fPeerAddress[2]);
		HLOG (2, " %02x %02x %02x\n", h->fPeerAddress[3], h->fPeerAddress[4], h->fPeerAddress[5]);
		if (memcmp (h->fPeerAddress, &fPacket[6], 6) == 0 && h->fConnectionState == CONN_CONNECT) {
			HLOG (2, "HCI Handler: %08x\n", h);
			h->fConnectionHandle = handle;
			break;
		}
	}
}

void HCI::CompleteConnection (NewtonErr err)
{
	HCI *h, *hci;
	L2CAP *l, *l2cap;
	BluntConnectionCompleteEvent *e;
	int i;
	Short handle;
	
	handle = GET_SHORT (fPacket, 4);
	HLOG (1, "HCI::CompleteConnection %04x\n", handle);
	for (hci = NULL, i = 1; hci == NULL && i < fServer->fNumHandlers; i++) {
		if (fServer->fHandlers[i]->fHandlerType == H_HCI) {
			h = (HCI *) fServer->fHandlers[i];
			HLOG (1, "%d %08x %08x\n", h->fConnectionState, h->fPeerAddress, fPacket + 6);
			if (memcmp (h->fPeerAddress, &fPacket[6], 6) == 0 && h->fConnectionState == CONN_CONNECT) {
				hci = h;
			}
		}
	}
	
	if (hci != NULL) {
		TUPort p (hci->fTool);
		HLOG (1, "HCI Handler: %08x\n", hci);
		
		if (err == noErr) {
			if (fTool) {
/*			
				BluntConnectionCompleteEvent *e;
				TUPort p (fTool);

				e = new BluntConnectionCompleteEvent (noErr);
				e->fHCIHandle = ((HCI *) fParentHandler)->fConnectionHandle;
				e->fHandler = this;
				p.Send ((TUAsyncMessage *) e, e, sizeof (BluntConnectionCompleteEvent), kNoTimeout, nil, BLUNT_MSG_TYPE);
*/				
			}

			hci->fConnectionHandle = handle;
			hci->fConnectionState = CONN_CONNECTED;
			hci->fState = HCI_IDLE;
			
			l2cap = NULL;
			if (hci->fNumHandlers > 1) {
				for (i = 1; l2cap == NULL && hci->fNumHandlers; i++) {
					l = (L2CAP *) hci->fHandlers[i];
					if (l->fConnectionState == CONN_CONNECT) {
						l2cap = l;
					}
				}
			}
			if (l2cap != NULL) {
				((L2CAP *) hci->fHandlers[0])->SndConnectionRequest (l2cap->fProtocol);
			}
		} else {
			e = new BluntConnectionCompleteEvent (err);
			p.Send ((TUAsyncMessage *) e, e, sizeof (BluntConnectionCompleteEvent),
				kNoTimeout, nil, BLUNT_MSG_TYPE);
		}
	}
}

void HCI::CompleteDisconnect (Short connectionHandle)
{
	BluntDisconnectCompleteEvent *e;
	HCI *hci = NULL;
	int i;
	
	HLOG (1, "HCI::CompleteDisconnect %04x\n", connectionHandle);
	hci = GetHandler (connectionHandle);
	if (hci != NULL) {
		TUPort p (hci->fTool);
		hci->fConnectionHandle = -1;
		fServer->DeferredRemoveHandler (hci);
		e = new BluntDisconnectCompleteEvent (noErr);
		p.Send ((TUAsyncMessage *) e, e, sizeof (*e), kNoTimeout, nil, BLUNT_MSG_TYPE);
	} else {
		HLOG (1, "HCI: Handle %04x for disconnect not found!\n", connectionHandle);
	}
}

void HCI::CompleteDisconnect (UByte *addr)
{
	BluntDisconnectCompleteEvent *e;
	HCI *hci = NULL;
	int i;
	
	HLOG (1, "HCI::CompleteDisconnect\n");
	hci = GetHandler (addr);
	if (hci != NULL) {
		TUPort p (hci->fTool);
		hci->fConnectionHandle = -1;
		fServer->DeferredRemoveHandler (hci);
		e = new BluntDisconnectCompleteEvent (noErr);
		p.Send ((TUAsyncMessage *) e, e, sizeof (*e), kNoTimeout, nil, BLUNT_MSG_TYPE);
	} else {
		HLOG (1, "HCI: Address for disconnect not found!\n");
	}
}

void HCI::ProcessACLData (void)
{
	int i;
	int len;
	Boolean r = false;
	
	HLOG (1, "HCI::ProcessACLData\n");
	
	fConnectionHandle = GET_SHORT (fPacket, 1) & 0x0fff;
	len = GET_SHORT (fPacket, 3);
	HLOG (2, "  Handle: 0x%04x, flags: %d, length: %d\n", fConnectionHandle, (fPacket[2] & 0xf0) >> 4, len);

	/*
	for (i = 0; i < len + 5 && i < 96; i++) {
		if (i % 16 == 0) printf ("\n");
		printf ("%02x ", fPacket[i]);
	}
	printf ("\n");
	*/
	
	for (i = 0; r == false && i < fNumHandlers; i++) {
		HLOG (1, "L2CAP Handler: %i %08x\n", i, fHandlers[i]);
		r = ((L2CAP *) fHandlers[i])->HandleData (&fPacket[1], len);
	}
}

HCI* HCI::GetHandler (Short handle)
{
	int i;
	HCI* hci;
	
	HLOG (1, "HCI::GetHandler %04x\n", handle);
	for (i = 1, hci = NULL; hci == NULL && i < fServer->fNumHandlers; i++) {
		if (((HCI *) fServer->fHandlers[i])->fConnectionHandle == handle) {
			hci = (HCI *) fServer->fHandlers[i];
		}
	}
	return hci;
}	
	
HCI* HCI::GetHandler (UByte *addr)
{
	int i;
	HCI* hci;
	
	HLOG (1, "HCI::GetHandler %02x:%02x:%02x...\n", addr[0], addr[1], addr[2]);
	for (i = 1, hci = NULL; hci == NULL && i < fServer->fNumHandlers; i++) {
		if (memcmp (((HCI *) fServer->fHandlers[i])->fPeerAddress, addr, 6) == 0) {
			hci = (HCI *) fServer->fHandlers[i];
		}
	}
	return hci;
}

void HCI::TrackCompletedPackets ()
{
	int i, j;
	HCI* hci;
	Short handle;
	
	HLOG (1, "HCI::TrackCompletedPackets\n");
	for (i = 1; i < fServer->fNumHandlers; i++) {
		hci = (HCI *) fServer->fHandlers[i];
		for (j = 0; j < fPacket[3]; j++) {
			handle = GET_SHORT (fPacket, 4 + j * 2);
			if (handle == hci->fConnectionHandle) {
				hci->fOutstandingPackets -= fPacket[4 + fPacket[3] * 2 + j];
				HLOG (1, "  Handler %d (%04x): %d outstanding\n", i, hci->fConnectionHandle, hci->fOutstandingPackets);
				if (hci->fOutstandingPackets < hci->fHCIWindowSize / 2 && fServer->fBufferOutput) {
					fServer->StartOutput ();
				}
			}
		}
	}
}	
	
#pragma mark -

void HCI::Pair (UByte *addr, Char *PIN)
{
	HLOG (1, "HCI::Pair %02x:%02x:%02x...\n", addr[0], addr[1], addr[2]);
	fLinkKeyValid = false;
	strcpy (fPairPINCode, PIN);
	memcpy (fPairAddress, addr, 6);
	fControlChannel->fState = fState = HCI_IPAIR_CONNECT;
	fControlChannel->CreateConnection (fPairAddress, fPSRepMode, fPSMode);
}

void HCI::Connect ()
{
	fControlChannel->fState = fState = HCI_CREATE_CONNECTION;
	fControlChannel->CreateConnection (fPeerAddress, fPSRepMode, fPSMode);
}

void HCI::Status ()
{
	fState = HCI_STATUS_BD_ADDR;
	ReadBdAddr ();
}

#pragma mark -

//-----------------------------------------------------------------------------
// Data sending
//-----------------------------------------------------------------------------

void HCI::SndPacketHeader (Byte flags, Size length)
{
	UByte header[5];

	fOutstandingPackets++;
	HLOG (1, "HCI::SndPacketHeader\n %04x %d (%d)\n", fConnectionHandle, length, fOutstandingPackets);
	if (fOutstandingPackets > fHCIWindowSize) {
		HLOG (0, "*** Too many outstanding packets in HCI::SndPacketHeader (%d > %d)\n",
			fOutstandingPackets, fHCIWindowSize);
		fServer->BufferOutput (true);
	}

	header[0] = 0x02;
	header[1] = fConnectionHandle & 0x00ff;
	header[2] = ((fConnectionHandle & 0xff00) >> 8) | (flags << 4);
	header[3] = length & 0x00ff;
	header[4] = (length & 0xff00) >> 8;
	
	fServer->Output (header, 5);
}

//-----------------------------------------------------------------------------
// Link Control Commands, OGF = 0x01
//-----------------------------------------------------------------------------

void HCI::Inquiry (UByte time, UByte amount)
{
	UByte packet[9];
	
	HLOG (1, "HCI::Inquiry\n");
	fState = HCI_INQUIRY;
	memcpy (packet, "\001\001\004\005\063\213\236", 7);
	packet[7] = time;
	packet[8] = amount;
	fServer->Output (packet, sizeof (packet));
}

void HCI::InquiryCancel (void)
{
	HLOG (1, "HCI::InquiryCancel\n");
	if (fState == HCI_INQUIRY) {
		fState = HCI_INQUIRY_CANCEL;
		fServer->Output ((UByte *) "\001\002\004\000", 4);
	}
}

void HCI::PeriodicInquiryMode (void)
{
}

void HCI::ExitPeriodicInquiryMode (void)
{
}

void HCI::CreateConnection (UByte *bd_addr, Byte psRep, Byte psMode)
{
	UByte packet[17];
	
	HLOG (1, "HCI::CreateConnection %02x:%02x:%02x:%02x:%02x:%02x\n",
        bd_addr[0], bd_addr[1], bd_addr[2], bd_addr[3], bd_addr[5], bd_addr[6]);
	fConnectionState = CONN_CONNECT;
	memset (packet, 0, sizeof (packet));
	memcpy (packet, "\001\005\004\015", 4);
	memcpy (&packet[4], bd_addr, 6);
	packet[10] = 0x18;
	packet[11] = 0xcc;
	packet[12] = psRep;
	packet[13] = psMode;
	packet[14] = 0x00;
	packet[15] = 0x00;
	packet[16] = 0x00;
	fServer->Output (packet, sizeof (packet));
}

void HCI::Disconnect (Byte reason)
{
	UByte packet[7];
	
	HLOG (1, "HCI::Disconnect (%08x) %04x\n", this, fConnectionHandle);
	fConnectionState = CONN_DISCONNECT;
	memset (packet, 0, sizeof (packet));
	memcpy (packet, "\001\006\004\003", 4);
	SET_SHORT (packet, 4, fConnectionHandle);
	packet[6] = reason;
	fServer->Output (packet, sizeof (packet));
}

void HCI::AddSCOConnection (void)
{
}

void HCI::AcceptConnectionRequest (UByte *bd_addr, Byte role)
{
	UByte packet[11];
	
	HLOG (1, "HCI::AcceptConnectionRequest\n");
	memset (packet, 0, sizeof (packet));
	memcpy (packet, "\001\011\004\007", 4);
	memcpy (&packet[4], bd_addr, 6);
	packet[10] = role;
	fServer->Output (packet, sizeof (packet));
}

void HCI::RejectConnectionRequest (void)
{
}

void HCI::LinkKeyRequestReply (UByte *bd_addr, UByte *linkKey)
{
	UByte packet[26];
	
	HLOG (1, "HCI::LinkKeyRequestReply\n");
	memset (packet, 0, sizeof (packet));
	memcpy (packet, "\001\013\004\026", 4);
	memcpy (&packet[4], bd_addr, 6);
	memcpy (&packet[10], linkKey, 16);
	for (int i = 0; i < 16; i++) HLOG (1, "%02x ", linkKey[i]); HLOG (1, "\n");
	fServer->Output (packet, sizeof (packet));
}

void HCI::LinkKeyRequestNegativeReply (UByte *bd_addr)
{
	UByte packet[10];
	
	HLOG (1, "HCI::LinkKeyRequestNegativeReply\n");
	memset (packet, 0, sizeof (packet));
	memcpy (packet, "\001\014\004\006", 4);
	memcpy (&packet[4], bd_addr, 6);
	fServer->Output (packet, sizeof (packet));
}

void HCI::PINCodeRequestReply (UByte *bd_addr, Char *PIN)
{
	UByte packet[27];
	
	HLOG (1, "HCI::PINCodeRequestReply\n");
	memset (packet, 0, sizeof (packet));
	memcpy (packet, "\001\015\004\027", 4);
	memcpy (&packet[4], bd_addr, 6);
	packet[10] = strlen (PIN);
	memcpy (&packet[11], PIN, strlen (PIN));
	fServer->Output (packet, sizeof (packet));
}

void HCI::PINCodeRequestNegativeReply (void)
{
}

void HCI::ChangeConnectionPacketType (void)
{
}

void HCI::AuthenticationRequested (Short handle)
{
	UByte packet[6];
	
	HLOG (1, "HCI::AuthenticationRequested\n");
	memset (packet, 0, sizeof (packet));
	memcpy (packet, "\001\021\004\002", 4);
	SET_SHORT (packet, 4, handle);
	fServer->Output (packet, sizeof (packet));
}

void HCI::SetConnectionEncryption (void)
{
}

void HCI::ChangeConnectionLinkKey (void)
{
}

void HCI::MasterLinkKey (void)
{
}

void HCI::RemoteNameRequest (UByte *bdAddr, Byte psRep, Byte psMode)
{
	UByte packet[14];
	
	HLOG (1, "HCI::RemoteNameRequest (%d %d)\n", psRep, psMode);
	memset (packet, 0, sizeof (packet));
	memcpy (packet, "\001\031\004\012", 4);
	memcpy (&packet[4], bdAddr, 6);
	packet[10] = psRep;
	packet[11] = psMode;
	packet[12] = 0;
	packet[13] = 0;
	fServer->Output (packet, sizeof (packet));
}

void HCI::ReadRemoteSupportedFeatures (void)
{
}

void HCI::ReadRemoteVersionInformation (void)
{
}

void HCI::ReadClockOffset (void)
{
}

#pragma mark -

//-----------------------------------------------------------------------------
// Link Policy Commands, OGF = 0x02 
//-----------------------------------------------------------------------------

void HCI::HoldMode (void)
{
}

void HCI::SniffMode (void)
{
}

void HCI::ExitSniffMode (void)
{
}

void HCI::ParkMode (void)
{
}

void HCI::ExitParkMode (void)
{
}

void HCI::QoSSetup (void)
{
}

void HCI::RoleDiscovery (void)
{
}

void HCI::SwitchRole (void)
{
}

void HCI::ReadLinkPolicySettings (void)
{
}

void HCI::WriteLinkPolicySettings (void)
{
}

#pragma mark -

//-----------------------------------------------------------------------------
// Host Controller & Baseband Commands, OGF = 0x03
//-----------------------------------------------------------------------------

void HCI::SetEventMask (void)
{
}

void HCI::Reset (void)
{
	HLOG (1, "HCI::Reset\n");
	fState = HCI_RESET;
	fConnectionState = CONN_RESET;
	fServer->Output ((UByte *) "\001\003\014\000", 4);
}

void HCI::SetEventFilter (void)
{
}

void HCI::Flush (void)
{
}

void HCI::ReadPINType (void)
{
}

void HCI::WritePINType (void)
{
}

void HCI::CreateNewUnitKey (void)
{
}

void HCI::ReadStoredLinkKey (void)
{
}

void HCI::WriteStoredLinkKey (void)
{
}

void HCI::DeleteStoredLinkKey (void)
{
}

void HCI::ChangeLocalName (UByte *name)
{
	UByte packet[4 + 248];
	
	HLOG (1, "HCI::ChangeLocalName\n");
	memset (packet, 0, sizeof (packet));
	memcpy (packet, (UByte *) "\001\023\014\370", 4);
	memcpy (&packet[4], name, strlen ((char *) name));
	fServer->Output (packet, sizeof (packet));
}

void HCI::ReadLocalName (void)
{
}

void HCI::ReadConnectionAcceptTimeout (void)
{
}

void HCI::WriteConnectionAcceptTimeout (void)
{
}

void HCI::ReadPageTimeout (void)
{
}

void HCI::WritePageTimeout (void)
{
}

void HCI::ReadScanEnable (void)
{
}

void HCI::WriteScanEnable (UByte enable)
{
	HLOG (1, "HCI::WriteScanEnable\n");
	fServer->Output ((UByte *) "\001\032\014\001\003", 5);
}

void HCI::ReadPageScanActivity (void)
{
}

void HCI::WritePageScanActivity (UShort interval, UShort window)
{
}

void HCI::ReadInquiryScanActivity (void)
{
}

void HCI::WriteInquiryScanActivity (UShort interval, UShort window)
{
	HLOG (1, "HCI::WriteInquiryScanActivity\n");
	fServer->Output ((UByte *) "\001\036\014\004\000\004\022\000", 8);
}

void HCI::ReadAuthenticationEnable (void)
{
}

void HCI::WriteAuthenticationEnable (void)
{
}

void HCI::ReadEncryptionMode (void)
{
}

void HCI::WriteEncryptionMode (void)
{
}

void HCI::ReadClassofDevice (void)
{
}

void HCI::WriteClassofDevice (UByte *deviceClass)
{
	UByte packet[7];

	HLOG (1, "HCI::WriteClassofDevice\n");
	memcpy (packet, (UByte *) (UByte *) "\001\044\014\003", 4);
	memcpy (&packet[4], deviceClass, 3);
	fServer->Output (packet, sizeof (packet));
}

void HCI::ReadVoiceSetting (void)
{
}

void HCI::WriteVoiceSetting (void)
{
}

void HCI::ReadAutomaticFlushTimeout (void)
{
}

void HCI::WriteAutomaticFlushTimeout (void)
{
}

void HCI::ReadNumBroadcastRetransmissions (void)
{
}

void HCI::WriteNumBroadcastRetransmissions (void)
{
}

void HCI::ReadHoldModeActivity (void)
{
}

void HCI::WriteHoldModeActivity (void)
{
}

void HCI::ReadTransmitPowerLevel (void)
{
}

void HCI::ReadSCOFlowControlEnable (void)
{
}

void HCI::WriteSCOFlowControlEnable (void)
{
}

void HCI::SetHostControllerToHostFlowControl (void)
{
}

void HCI::HostBufferSize (void)
{
	HLOG (1, "HCI::HostBufferSize\n");
	fServer->Output ((UByte *) "\001\063\014\007\000\002\000\001\000\000\000", 11);
}

void HCI::HostNumberOfCompletedPackets (void)
{
}

void HCI::ReadLinkSupervisionTimeout (void)
{
}

void HCI::WriteLinkSupervisionTimeout (void)
{
}

void HCI::ReadNumberOfSupportedIAC (void)
{
}

void HCI::ReadCurrentIACLAP (void)
{
}

void HCI::WriteCurrentIACLAP (void)
{
}

void HCI::ReadPageScanPeriodMode (void)
{
}

void HCI::WritePageScanPeriodMode (void)
{
}

void HCI::ReadPageScanMode (void)
{
}

void HCI::WritePageScanMode (void)
{
}

#pragma mark -

//-----------------------------------------------------------------------------
// Informational Parameters
//-----------------------------------------------------------------------------

void HCI::ReadLocalVersionInformation (void)
{
}

void HCI::ReadLocalSupportedFeatures (void)
{
}

void HCI::ReadBufferSize (void)
{
	HLOG (1, "HCI::ReadBufferSize\n");
	fServer->Output ((UByte *) "\001\005\020\000", 4);
}

void HCI::ReadCountryCode (void)
{
}

void HCI::ReadBdAddr (void)
{
	HLOG (1, "HCI::ReadBdAddr\n");
	fServer->Output ((UByte *) "\001\011\020\000", 4);
}

#pragma mark -

//-----------------------------------------------------------------------------
// Status Parameters 
//-----------------------------------------------------------------------------

void HCI::ReadFailedContactCounter (void)
{
}

void HCI::ResetFailedContactCounter (void)
{
}

void HCI::GetLinkQuality (Short handle)
{
	UByte packet[6];
	
	HLOG (1, "HCI::GetLinkQuality (0x%04x)\n", handle);
	memset (packet, 0, sizeof (packet));
	memcpy (packet, "\001\005\024\002", 4);
	SET_SHORT (packet, 4, handle);
	fServer->Output (packet, sizeof (packet));
}

void HCI::ReadRSSI (void)
{
}

#pragma mark -

//-----------------------------------------------------------------------------
// Testing Commands 
//-----------------------------------------------------------------------------

void HCI::ReadLoopbackMode (void)
{
}

void HCI::WriteLoopbackMode (void)
{
}

void HCI::EnableDeviceUnderTestMode (void)
{
}

#pragma mark -

//-----------------------------------------------------------------------------
// Possible Events (HCI general)
//-----------------------------------------------------------------------------

void HCI::InquiryComplete (void)
{
	HLOG (1, "HCI::InquiryComplete\n");
}

void HCI::InquiryResult (void)
{
	int n, i, j;
	
	HLOG (1, "HCI::InquiryResult\n");
	
	n = fPacket[3];
	HLOG (1, "  Responses: %d\n", n);
	for (i = 0; i < 1; i++) {
		HLOG (1, "  BD_ADDR: ");
		for (j = 0; j < 6; j++) {
			HLOG (1, "%02x ", fPacket[4 + j + i]);
		}
		
		HLOG (1, "PS: %d %d %d ",
			fPacket[4 + (6 + 0 + i)],
			fPacket[4 + (6 + 1 + i)],
			fPacket[4 + (6 + 2 + i)]);
			
		HLOG (1, "Class: %02x%02x%02x",
			fPacket[4 + 0 + (6 + 3 + i)],
			fPacket[4 + 1 + (6 + 3 + i)],
			fPacket[4 + 2 + (6 + 3 + i)]);
			
		HLOG (1, "\n");
	}
}

void HCI::RemoteNameRequestComplete (void)
{
	HLOG (1, "HCI::RemoteNameRequestComplete\n");
}

void HCI::CommandComplete (void)
{
	HLOG (1, "HCI::CommandComplete: %02x %02d %02d\n",
		fPacket[6], fPacket[4], fPacket[5]);
}

void HCI::CommandStatus (void)
{
	HLOG (1, "HCI::CommandStatus: 0x%02x %d\n", fPacket[3], fPacket[4]);
	HLOG (1, "  Command: %02x %02x\n", fPacket[5], fPacket[6]);
}

void HCI::HardwareError (void)
{
	HLOG (0, "HCI::HardwareError\n  Code: 0x%02x Packets: %d\n", fPacket[3], fOutstandingPackets);
}

void HCI::NumberOfCompletedPackets (void)
{
	int i, n;
	Short o;
	
	HLOG (1, "HCI::NumberOfCompletedPackets\n");

	n = fPacket[3];
	o = fPacket[4 + (n * 2)] + fPacket[4 + (n * 2) + 1] * 256;

	HLOG (1, "%02x%02x ",
		fPacket[4 + 1],
		fPacket[4]);
	HLOG (1, "%d\n", o);
	
	fOutstandingPackets -= o;
}

void HCI::RoleChange (void)
{
	HLOG (1, "HCI::RoleChange\n");
}

void HCI::ReturnLinkKeys (void)
{
	HLOG (1, "HCI::ReturnLinkKeys\n");
}

void HCI::PINCodeRequest (void)
{
	HLOG (1, "HCI::PINCodeRequest\n");
}

void HCI::LinkKeyRequest (void)
{
	HLOG (1, "HCI::LinkKeyRequest\n");
}

void HCI::LinkKeyNotification (void)
{
	HLOG (1, "HCI::LinkKeyNotification\n");
	for (int i = 0; i < 16; i++) HLOG (1, "%02x ", fPacket[9 + i]); HLOG (1, "\n");
	memcpy (fLinkKey, &fPacket[9], 16);
	fLinkKeyValid = true;
}

void HCI::LoopbackCommand (void)
{
	HLOG (1, "HCI::LoopbackCommand\n");
}

void HCI::DataBufferOverflow (void)
{
	HLOG (1, "HCI::DataBufferOverflow\n");
}

void HCI::PageScanModeChange (void)
{
	HLOG (1, "HCI::PageScanModeChange\n");
}

void HCI::PageScanRepetitionModeChange (void)
{
	HLOG (1, "HCI::PageScanRepetitionModeChange\n");
}

#pragma mark -

//-----------------------------------------------------------------------------
// Possible Events (Link events)
//-----------------------------------------------------------------------------

void HCI::ConnectionComplete (void)
{
	HLOG (1, "HCI::ConnectionComplete: 0x%02x %04x\n", fPacket[3], fPacket[4] + (fPacket[5] << 8));
	fOutstandingPackets = 0;
	fConnectionHandle = fPacket[4] + (fPacket[5] << 8);
}

void HCI::ConnectionRequest (void)
{
	int i;
	
	HLOG (1, "HCI::ConnectionRequest\n");
	
	HLOG (1, "  BD_ADDR: ");
	for (i = 0; i < 6; i++) {
		HLOG (1, "%02x ", fPacket[3 + i]);
	}
	HLOG (1, "Class: %02x%02x%02x ",
		fPacket[9],
		fPacket[10],
		fPacket[11]);
	HLOG (1, "Link type: %02x\n",
		fPacket[12]);
}

void HCI::DisconnectionComplete (void)
{
	HLOG (1, "HCI::DisconnectionComplete\n");
	fConnectionHandle = -1;
	fOutstandingPackets = -1;
}

void HCI::AuthenticationComplete (void)
{
	HLOG (1, "HCI::AuthenticationComplete\n");
}

void HCI::EncryptionChange (void)
{
	HLOG (1, "HCI::EncryptionChange\n");
}

void HCI::ChangeConnectionLinkKeyComplete (void)
{
	HLOG (1, "HCI::ChangeConnectionLinkKeyComplete\n");
}

void HCI::MasterLinkKeyComplete (void)
{
	HLOG (1, "HCI::MasterLinkKeyComplete\n");
}

void HCI::ReadRemoteSupportedFeaturesComplete (void)
{
	HLOG (1, "HCI::ReadRemoteSupportedFeaturesComplete\n");
}

void HCI::ReadRemoteVersionInformationComplete (void)
{
	HLOG (1, "HCI::ReadRemoteVersionInformationComplete\n");
}

void HCI::QoSSetupComplete (void)
{
	HLOG (1, "HCI::QoSSetupComplete\n");
}

void HCI::FlushOccurred (void)
{
	HLOG (1, "HCI::FlushOccurred\n");
}

void HCI::ModeChange (void)
{
	HLOG (1, "HCI::ModeChange\n");
}

void HCI::MaxSlotsChange (void)
{
	HLOG (1, "HCI::MaxSlotsChange\n");
}

void HCI::ReadClockOffsetComplete (void)
{
	HLOG (1, "HCI::ReadClockOffsetComplete\n");
}

void HCI::ConnectionPacketTypeChanged (void)
{
	HLOG (1, "HCI::ConnectionPacketTypeChanged\n");
}

void HCI::QoSViolation (void)
{
	HLOG (1, "HCI::QoSViolation\n");
}
