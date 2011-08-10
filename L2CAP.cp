#define DEFAULT_LOGLEVEL 0

#include <AEvents.h>
#include <AEventHandler.h>
#include <BufferSegment.h>
#include <CommManagerInterface.h>
#include <CommTool.h>
#include <SerialOptions.h>
#include <CommOptions.h>
#include <UserPorts.h>

#include "BluntServer.h"
#include "HCI.h"
#include "SDP.h"
#include "L2CAP.h"
#include "RFCOMM.h"

#define EVT_L2CAP_COMMAND_REJECT 0x01
#define EVT_L2CAP_CONNECTION_REQUEST 0x02
#define EVT_L2CAP_CONNECTION_RESPONSE 0x03
#define EVT_L2CAP_CONFIGURE_REQUEST 0x04
#define EVT_L2CAP_CONFIGURE_RESPONSE 0x05
#define EVT_L2CAP_DISCONNECTION_REQUEST 0x06
#define EVT_L2CAP_DISCONNECTION_RESPONSE 0x07
#define EVT_L2CAP_ECHO_REQUEST 0x08
#define EVT_L2CAP_ECHO_RESPONSE 0x09
#define EVT_L2CAP_INFORMATION_REQUEST 0x0a
#define EVT_L2CAP_INFORMATION_RESPONSE 0x0b
#define EVT_L2CAP_TIMER 0xff

enum {
	L2CAP_ANY,
	L2CAP_IDLE,
	L2CAP_CONNECTION_REQUEST,
	L2CAP_CONN_CONF_OUT,
	L2CAP_CONN_CONF_IN,
	L2CAP_ACCPT_CONF_OUT,					/* 5 */
	L2CAP_ACCPT_CONF_IN,
	L2CAP_DISCONNECTION_REQUEST
};

enum {
	A_SND_CONN_RESP,
	A_SND_CONF_REQ,
	A_SND_CONF_RESP,
	A_CONNECTED,
	A_SND_CONF_RESP_ACCEPTED,
	A_SND_CONF_RESP_CONNECTED,				/* 5 */
	A_ACCEPTED,
	A_DISCONNECTED,
	A_SND_INFORMATION_RESPONSE,
	A_COMMAND_REJECT,
	A_SND_DISC_RESP,						/* 10 */
	A_RESEND_CONN_RESP,
	A_SND_CONF_RESP_PENDING,
	A_RESEND_CONN_REQ,
	A_NONE
};

static const int kStates[][4] = {
	/* Connect states */
	{L2CAP_CONNECTION_REQUEST,	EVT_L2CAP_CONNECTION_RESPONSE,	A_SND_CONF_REQ,				L2CAP_CONN_CONF_OUT},
	{L2CAP_CONNECTION_REQUEST,	EVT_L2CAP_TIMER,				A_RESEND_CONN_REQ,			L2CAP_CONNECTION_REQUEST},
	{L2CAP_CONNECTION_REQUEST,	EVT_L2CAP_CONFIGURE_REQUEST,	A_SND_CONF_RESP_PENDING,	L2CAP_CONN_CONF_IN},
	{L2CAP_CONN_CONF_OUT,		EVT_L2CAP_CONFIGURE_REQUEST,	A_SND_CONF_RESP,			L2CAP_CONN_CONF_IN},
	{L2CAP_CONN_CONF_OUT,		EVT_L2CAP_CONFIGURE_RESPONSE,	A_NONE,						L2CAP_CONN_CONF_IN},
	{L2CAP_CONN_CONF_IN,		EVT_L2CAP_CONFIGURE_RESPONSE,	A_CONNECTED,				L2CAP_IDLE},
	{L2CAP_CONN_CONF_IN,		EVT_L2CAP_CONFIGURE_REQUEST,	A_SND_CONF_RESP_CONNECTED,	L2CAP_IDLE},
	
	/* Accept states */
	{L2CAP_ANY,					EVT_L2CAP_CONNECTION_REQUEST,	A_SND_CONN_RESP,			L2CAP_ACCPT_CONF_OUT},
	{L2CAP_ACCPT_CONF_OUT,		EVT_L2CAP_TIMER,				A_RESEND_CONN_RESP,			L2CAP_ACCPT_CONF_OUT},
	{L2CAP_ACCPT_CONF_OUT,		EVT_L2CAP_CONFIGURE_REQUEST,	A_SND_CONF_RESP,			L2CAP_ACCPT_CONF_IN},
	{L2CAP_ACCPT_CONF_OUT,		EVT_L2CAP_CONFIGURE_REQUEST,	A_SND_CONF_RESP,			L2CAP_ACCPT_CONF_IN},
	{L2CAP_ACCPT_CONF_OUT,		EVT_L2CAP_CONFIGURE_RESPONSE,	A_NONE,						L2CAP_ACCPT_CONF_IN},
	{L2CAP_ACCPT_CONF_IN,		EVT_L2CAP_CONFIGURE_RESPONSE,	A_ACCEPTED,					L2CAP_IDLE},
	{L2CAP_ACCPT_CONF_IN,		EVT_L2CAP_CONFIGURE_REQUEST,	A_SND_CONF_RESP_ACCEPTED,	L2CAP_IDLE},
	
	/* Disconnect states */
	{L2CAP_ANY,					EVT_L2CAP_DISCONNECTION_REQUEST,	A_SND_DISC_RESP,		L2CAP_IDLE},
	
	/* Info request states */
	{L2CAP_ANY,					EVT_L2CAP_INFORMATION_REQUEST,	A_SND_INFORMATION_RESPONSE,	L2CAP_ANY},
	
	/* Other states */
	{L2CAP_ANY,					EVT_L2CAP_COMMAND_REJECT,		A_COMMAND_REJECT,			L2CAP_ANY}
};
	
L2CAP::L2CAP (void)
{
	fLogLevel = DEFAULT_LOGLEVEL;
	
	HLOG (1, "L2CAP::L2CAP %08x\n", this);
	fLocalIdentifier = 1;
	fRemoteIdentifier = 0;
	fConnectionAttempts = 3;
	fLastCID = 0x0040;
	fState = L2CAP_IDLE;
	fTimeout = false;
	fHandlerType = H_L2CAP;
}

L2CAP::~L2CAP ()
{
	int i;
	
	HLOG (1, "L2CAP::~L2CAP\n");
	for (i = 0; i < fNumHandlers; i++) {
		if (fHandlers[i]->fHandlerType == H_RFCOMM) {
		delete (RFCOMM *) fHandlers[i];
		} else {
			delete (SDP *) fHandlers[i];
		}
	}
}

int L2CAP::Action (int action, void *eventData)
{
	int newState;
	L2CAP* l2cap;
	int i;

	newState = L2CAP_ANY;
	HLOG (1, "L2CAP::Action (%08x) %d %d\n", this, fState, action);
	switch (action) {
		case A_SND_CONF_REQ:
			l2cap = ConnectionResponse ();
			if (l2cap != NULL) {
				SndConfigureRequest (l2cap->fRemoteCID);
			} else {
				newState = L2CAP_CONNECTION_REQUEST;
			}
			break;
		case A_SND_CONF_RESP:
			l2cap = ConfigureRequest ();
			if (l2cap != NULL) SndConfigureResponse (l2cap->fLocalCID, l2cap->fRemoteMTU);
			break;
		case A_SND_CONF_RESP_PENDING:
			l2cap = ConfigureRequest ();
			if (l2cap != NULL) {
				SndConfigureResponse (l2cap->fLocalCID, l2cap->fRemoteMTU);
				SndConfigureRequest (l2cap->fRemoteCID);
			}
			break;
		case A_SND_CONN_RESP:
			l2cap = ConnectionRequest ();
			if (l2cap != NULL) {
				SndConnectionResponse (l2cap->fLocalCID, l2cap->fRemoteCID, 0x0000, 0x0000);
				SndConfigureRequest (l2cap->fRemoteCID);
				fServer->SetTimer (this, 2000, l2cap);
			}
			break;
		case A_RESEND_CONN_RESP:
			l2cap = (L2CAP*) eventData;
			if (l2cap != NULL) {
				SndConnectionResponse (l2cap->fLocalCID, l2cap->fRemoteCID, 0x0000, 0x0000);
				SndConfigureRequest (l2cap->fRemoteCID);
			}
			break;
		case A_CONNECTED:
			l2cap = ConfigureResponse ();
			if (l2cap != NULL) {
				l2cap->CompleteConnection ();
				l2cap->fConnectionState = CONN_CONNECTED;
			}
			break;
		case A_ACCEPTED:
			l2cap = ConfigureResponse ();
			if (l2cap != NULL) {
				l2cap->fConnectionState = CONN_CONNECTED;
			}
			break;
		case A_SND_CONF_RESP_CONNECTED:
			l2cap = ConfigureRequest ();
			if (l2cap != NULL) {
				SndConfigureResponse (l2cap->fLocalCID, l2cap->fRemoteMTU);
				l2cap->CompleteConnection ();
				l2cap->fConnectionState = CONN_CONNECTED;
			}
			break;
		case A_SND_CONF_RESP_ACCEPTED:
			l2cap = ConfigureRequest ();
			if (l2cap != NULL) {
				SndConfigureResponse (l2cap->fLocalCID, l2cap->fRemoteMTU);
				l2cap->fConnectionState = CONN_CONNECTED;
			}
			break;
		case A_SND_INFORMATION_RESPONSE:
			SndInformationResponse (GET_SHORT (fInputPacket, 4), 1, 0, NULL);
			break;
		case A_SND_DISC_RESP:
			l2cap = DisconnectionRequest ();
			if (l2cap != NULL) {
				SndDisconnectionResponse (l2cap->fLocalCID, l2cap->fRemoteCID);
				for (i = 0; i < l2cap->fNumHandlers; i++) {
					if (l2cap->fHandlers[i]->fHandlerType == H_RFCOMM) {
						((RFCOMM *) l2cap->fHandlers[i])->CompleteDisconnect ();
					}
				}
				fParentHandler->RemoveHandler (l2cap);
				delete l2cap;
				fServer->Print ();
			}
			break;
		case A_RESEND_CONN_REQ:
			l2cap = (L2CAP*) eventData;
			HLOG (1, "  Resend: %08x %d\n", l2cap, l2cap->fConnectionAttempts);
			if (l2cap->fConnectionAttempts-- > 0) {
				SndConnectionRequest (l2cap->fProtocol);
			} else {
				ConnectionError (-16013);
			}
			break;
		case A_COMMAND_REJECT:
			HLOG (1, "  Command reject: %d\n", GET_SHORT (fInputPacket, 4));
			break;
	}
	return newState;
}

void L2CAP::Transition (ULong event, void *eventData)
{
	int i;
	int newState;
	
	HLOG (1, "L2CAP::Transition (%08x) %d %02x -> ", this, fState, event);
	for (i = 0; i < NUM_ELEMENTS(kStates); i++) {
		if ((fState == kStates[i][0] || kStates[i][0] == L2CAP_ANY) && event == kStates[i][1]) {
			HLOG (1, "%d\n", i);
			newState = Action (kStates[i][2], eventData);
			if (newState != L2CAP_ANY) {
				fState = newState;
			} else if (kStates[i][3] != L2CAP_ANY) {
				fState = kStates[i][3];
			}
			break;
		}
	}
	HLOG (1, "New state: %d\n", fState);
}

Boolean L2CAP::HandleData (UByte *data, ULong length)
{
	UByte continuation;
	UByte broadcast;
	Boolean r;
	
	r = false;
	continuation = (data[1] & 0x30) >> 4;
	broadcast = (data[1] & 0xc0) >> 6;
	
	HLOG (1, "L2CAP::HandleData %08x (%d)\n", this, length);
	if (length >= 8) {
		HLOG (1, "  CID: %04x %04x\n", fLocalCID, GET_SHORT (data, 6));
	}
	if (length >= 8 && continuation == CONT_FIRST && GET_SHORT (data, 6) == fLocalCID ||
		length >= 1 && continuation == CONT_FOLLOW && fContinuationActive) {
		HLOG (1, "  Flags: %d %d %d\n", length, broadcast, continuation);
		if (continuation == CONT_FIRST) {
			if (length - 8 > sizeof (fInputPacket)) {
				printf ("*** Overflow in ProcessACLData: %d ***\n", length - 4);
				length = sizeof (fInputPacket) - 4;
			}
			memcpy (fInputPacket, &data[8], length - 4);
			fCurrentInputPacketLength = length - 4;
			fInputPacketLength = GET_SHORT (data, 4);
			fInputPacketCID = GET_SHORT (data, 6);
			fRemoteIdentifier = fInputPacket[1];
			HLOG (1, "  F: Length: %d (%d) CID: %04x\n", fCurrentInputPacketLength, fInputPacketLength, fInputPacketCID);
			fContinuationActive = true;
		} else if (continuation == CONT_FOLLOW) {
			HLOG (1, "  C: Length: %d (%d) CID: %04x\n", fCurrentInputPacketLength, fInputPacketLength, fInputPacketCID);
			if (fCurrentInputPacketLength + length > sizeof (fInputPacket)) {
				printf ("*** Overflow in ProcessACLData (2): %d ***\n", fCurrentInputPacketLength + length);
				fCurrentInputPacketLength = sizeof (fInputPacket) - length;
			}
			memcpy (&fInputPacket[fCurrentInputPacketLength], &data[4], length);
			fCurrentInputPacketLength += length;
			HLOG (1, "\n");
		} else {
			HLOG (1, "\n");
		}
		
		if (fCurrentInputPacketLength == fInputPacketLength) {
			fContinuationActive = false;
			switch (fInputPacketCID) {
				case CID_NULL:
					break;
				case CID_SIGNAL:
					ProcessACLEvent ();
					break;
				case CID_CL_RCV:
					break;
				default:
					ProcessL2CAPData ();
					break;
			}
		}
		r = true;
	}
	
	return r;
}

void L2CAP::HandleTimer (void *userData)
{
	HLOG (1, "L2CAP::HandleTimer\n");
	Transition (EVT_L2CAP_TIMER, userData);
}

void L2CAP::ProcessACLEvent (void)
{
	HLOG (1, "L2CAP::ProcessACLEvent (%d %d %d %d)\n",
		fRemoteIdentifier, fOutstandingRequestID, fLocalIdentifier, fInputPacket[0]);
	Transition (fInputPacket[0], NULL);
}

Boolean L2CAP::ProcessL2CAPData (void)
{
	Boolean r = false;
	int i;
	
	HLOG (1, "L2CAP::ProcessL2CAPData\n");
	
	for (i = 0; i < fInputPacketLength && i < 96; i++) {
		if ((i + 1) % 16 == 0) HLOG (2, "\n");
		HLOG (2, "%02x ", fInputPacket[i]);
	}
	HLOG (2, "\n");
	
/*	
	HLOG (1, "  Debug: %d\n", fServer->fDebug);
	if (fServer->fDebug > 0) {
		fServer->fBytesRead += fInputPacketLength;
		if ((fServer->fBytesRead - fInputPacketLength) / 1024 < fServer->fBytesRead / 1024) {
			HLOG (0, "Got %d bytes\n", fServer->fBytesRead);
		}
		return true;
	}
*/
	
	for (i = 0; r == false && i < fNumHandlers; i++) {
		HLOG (1, "  Handler: %i %08x\n", i, fHandlers[i]);
		if (fHandlers[i]->fHandlerType == H_RFCOMM) {
			r = ((RFCOMM *) fHandlers[i])->HandleData (fInputPacket, fInputPacketLength);
		} else {
			r = ((SDP *) fHandlers[i])->HandleData (fInputPacket, fInputPacketLength);
		}
	}
	return r;
}

#pragma mark -

// ================================================================================
// ¥ Utility functions
// ================================================================================

Byte L2CAP::NextLocalIdentifier (void)
{
	HLOG (2, "L2CAP::NextLocalIdentifier\n  %d %d", fTimeout, fLocalIdentifier);
	if (!fTimeout) {
		fLocalIdentifier++;
		if (fLocalIdentifier == 0) fLocalIdentifier = 1;
		fOutstandingRequestID = fLocalIdentifier;
	} else {
		fTimeout = false;
	}
	HLOG (2, " %d\n", fLocalIdentifier);
	
	return fLocalIdentifier;
}

Short L2CAP::GetNewCID ()
{
	Short r;
	
	r = fLastCID++;
	if (fLastCID == 0x7fff) r = 0x0040;
	return r;
}

L2CAP* L2CAP::GetHandler (Short sourceCID)
{
	int i;
	L2CAP* l2cap;
	
	HLOG (1, "L2CAP::GetHandler %d", sourceCID);
	for (i = 1, l2cap = NULL; l2cap == NULL && i < fParentHandler->fNumHandlers; i++) {
		HLOG (1, "  %d (%08x) %d\n", i, fParentHandler->fHandlers[i],
			((L2CAP *) fParentHandler->fHandlers[i])->fLocalCID);
		if (((L2CAP *) fParentHandler->fHandlers[i])->fLocalCID == sourceCID) {
			l2cap = (L2CAP *) fParentHandler->fHandlers[i];
		}
	}
	return l2cap;
}

void L2CAP::Connect (void)
{
	HLOG (1, "L2CAP::Connect %08x\n", this);
	fConnectionState = CONN_CONNECT;
	if (fParentHandler->fConnectionState != CONN_CONNECTED) {
		((HCI *) fParentHandler)->Connect ();
	} else {
		SndConnectionRequest (fProtocol);
	}
}

void L2CAP::CompleteConnection (void)
{
	int i;
	SDP *sdp;
	RFCOMM *rfcomm;
	Boolean done;
	
	HLOG (1, "L2CAP::CompleteConnection (%08x) %d\n", this, fNumHandlers);
	fConnectionState = CONN_CONNECTED;
	if (fTool) {
/*		
		BluntConnectionCompleteEvent *e;
		TUPort p (fTool);

		e = new BluntConnectionCompleteEvent (noErr);
		e->fHCIHandle = ((HCI *) fParentHandler)->fConnectionHandle;
		e->fL2CAPLocalCID = fLocalCID;
		e->fL2CAPRemoteCID = fRemoteCID;
		e->fHandler = this;
		p.Send ((TUAsyncMessage *) e, e, sizeof (BluntConnectionCompleteEvent), kNoTimeout, nil, BLUNT_MSG_TYPE);
*/		
	}
	for (done = true, i = 0; i < fNumHandlers; i++) {
		HLOG (1, "Handler: %08x %d %d\n", fHandlers[i], fHandlers[i]->fHandlerType, fHandlers[i]->fConnectionState);
		if (fHandlers[i]->fConnectionState == CONN_CONNECT) {
			HLOG (1, "  Continue with %08x (%d)\n", fHandlers[i], fHandlers[i]->fHandlerType);
			switch (fHandlers[i]->fHandlerType) {
				case H_RFCOMM:
					if (i >= 1) {
						// Control channel is automatically connected
						rfcomm = (RFCOMM *) fHandlers[i];
						rfcomm->Connect ();
					}
					break;
				case H_SDP:
					sdp = (SDP *) fHandlers[i];
					sdp->GetServices ();
					break;
			}
			done = false;
		}
	}
}

void L2CAP::ConnectionError (NewtonErr error)
{
	BluntConnectionCompleteEvent *e;
	TUPort p (fTool);
	
	HLOG (1, "L2CAP::ConnectionError %d\n", error);
	e = new BluntConnectionCompleteEvent (error);
	p.Send ((TUAsyncMessage *) e, e, sizeof (BluntConnectionCompleteEvent),
		kNoTimeout, nil, BLUNT_MSG_TYPE);
}

#pragma mark -

// ================================================================================
// ¥ Requests
// ================================================================================

void L2CAP::CommandReject (void)
{
	HLOG (1, "L2CAP::CommandReject (%d)\n", GET_SHORT (fInputPacket, 4));
}

L2CAP* L2CAP::ConnectionRequest (void)
{
	Short prot;
	L2CAP* l2cap;
	SDP* sdp;
	RFCOMM* rfcomm;
	Short CID;
	
	HLOG (1, "L2CAP::ConnectionRequest\n");

	l2cap = NULL;
	prot = GET_SHORT (fInputPacket, 4);
	CID = GET_SHORT (fInputPacket, 6);
	HLOG (1, "  Protocol: %d Source: %d\n", prot, CID);
	
	if (prot == PSM_SDP || prot == PSM_RFCOMM) {
		l2cap = new L2CAP ();
		l2cap->fLocalCID = GetNewCID ();
		l2cap->fRemoteCID = CID;
		fTargetProtocol = prot;
		l2cap->fConnectionState = CONN_ACCEPT;
		fParentHandler->AddHandler (l2cap, fServer);
		if (prot == PSM_SDP) {
			sdp = new SDP;
			l2cap->AddHandler (sdp, fServer);
		} else {
		}
		fServer->Print ();
	}
	return l2cap;
}

L2CAP* L2CAP::ConnectionResponse (void)
{
	Short result;
	L2CAP* l2cap;

	l2cap = NULL;
	result = GET_SHORT (fInputPacket, 8);
	HLOG (1, "L2CAP::ConnectionResponse (%d %d)\n", result, fInputPacket[1]);
	HLOG (1, "D: %d S: %d\n", GET_SHORT (fInputPacket, 4), GET_SHORT (fInputPacket, 6));
	l2cap = GetHandler (GET_SHORT (fInputPacket, 6));
	l2cap->fRemoteCID = GET_SHORT (fInputPacket, 4);
	if (result == 0) {
		if (l2cap != NULL) {
			HLOG (1, "  L2CAP: %08x %d\n", l2cap, GET_SHORT (fInputPacket, 4));
		} else {
			HLOG (1, "  Channel %d not found\n", GET_SHORT (fInputPacket, 6));
		}
	} else {
		if (result != 1) {
			HLOG (1, "  Connection failed\n");
			if (l2cap != NULL) {
				fParentHandler->RemoveHandler (l2cap);
				delete l2cap;
			}
			ConnectionError (kCommErrConnectionAborted);
		}
		l2cap = NULL;
	}
	return l2cap;
}

L2CAP* L2CAP::ConfigureRequest (void)
{
	int i;
	Short len;
	Short flags;
	Short configureCID;
	L2CAP* l2cap;
	Short MTU;
	
	configureCID = GET_SHORT (fInputPacket, 4);
	HLOG (1, "L2CAP::ConfigureRequest (%d)\n", configureCID);
	l2cap = GetHandler (configureCID);
	if (l2cap != NULL) {
		len = GET_SHORT (fInputPacket, 2) - 4;
		flags = GET_SHORT (fInputPacket, 6);
		fTempCID = l2cap->fRemoteCID;
		l2cap->fRemoteMTU = 672;
	
		HLOG (1, "  ID: %d Length: %d ", fRemoteIdentifier, len);
		i = 8;
		while (i - 8 < len) {
			switch (fInputPacket[i]) {
				case CONF_MTU:
					l2cap->fRemoteMTU = GET_SHORT (fInputPacket, i + 2);
					HLOG (1, "MTU: %d ", l2cap->fRemoteMTU);
					break;
				case CONF_FLUSH_TIMEOUT:
					HLOG (1, "FTO: %d", GET_SHORT (fInputPacket, i + 2));
					break;
				case CONF_QOS:
					HLOG (1, "QoS: ");
					break;
			}
			i += fInputPacket[i + 1] + 2;
		};
		HLOG (1, "\n");
	} else {
		HLOG (1, "  Channel %d not found\n", configureCID);
	}
	return l2cap;
}

L2CAP* L2CAP::ConfigureResponse (void)
{
	int i;
	Short len;
	Short flags;
	L2CAP* l2cap;
	
	HLOG (1, "L2CAP::ConfigureResponse\n");
	l2cap = GetHandler (GET_SHORT (fInputPacket, 4));
	if (l2cap != NULL) {
		HLOG (1, "  Length %d Flags: %04x Result: %d\n", GET_SHORT (fInputPacket, 2),
			GET_SHORT (fInputPacket, 6), GET_SHORT (fInputPacket, 8));
	} else {
		HLOG (1, "  Channel %d not found\n", GET_SHORT (fInputPacket, 4));
	}
	return l2cap;
}

L2CAP* L2CAP::DisconnectionRequest (void)
{
	L2CAP* l2cap = NULL;

	HLOG (1, "L2CAP::DisconnectionRequest\n");
	l2cap = GetHandler (GET_SHORT (fInputPacket, 4));	
	if (l2cap != NULL) {
		HLOG (1, "  DCID: %d SCID: %d\n", l2cap->fLocalCID, l2cap->fRemoteCID);
	} else {
		HLOG (1, "  Channel %d not found\n", GET_SHORT (fInputPacket, 4));
	}
	return l2cap;
}

void L2CAP::DisconnectionResponse (void)
{
	HLOG (1, "L2CAP::DisconnectionResponse\n");
}

void L2CAP::EchoRequest (void)
{
	HLOG (1, "L2CAP::EchoRequest\n");
}

void L2CAP::EchoResponse (void)
{
	HLOG (1, "L2CAP::EchoResponse\n");
}

void L2CAP::InformationRequest (void)
{
	HLOG (1, "L2CAP::InformationRequest\n");
}

void L2CAP::InformationResponse (void)
{
	HLOG (1, "L2CAP::InformationResponse\n");
}

#pragma mark -

// ================================================================================
// ¥ Responses
// ================================================================================

void L2CAP::SndData (UByte *data, Short length)
{
	HLOG (2, "L2CAP::SndData %08x %d\n", data, length);
	
	fServer->BufferOutput (true);
	SndPacketHeader (length);
	fServer->Output (data, length, false);
}

void L2CAP::SndPacketHeader (Short dataSize)
{
	UByte header[4];

	HLOG (2, "L2CAP::SndPacketHeader (cid: 0x%02x)\n", fRemoteCID);

	SET_SHORT (header, 0, dataSize);
	SET_SHORT (header, 2, fRemoteCID);
	
	((HCI *) fParentHandler)->SndPacketHeader (CONT_FIRST, dataSize + 4);
	fServer->Output (header, 4, false);
}

void L2CAP::SndCommandReject ()
{
}

void L2CAP::SndConnectionRequest (Short prot)
{
	UByte data[8];
	int i;
	L2CAP *l2cap, *l;
	
	HLOG (1, "L2CAP::SndConnectionRequest %d %08x\n", prot, this);
	for (i = 1, l2cap = NULL; l2cap == NULL && i < fParentHandler->fNumHandlers; i++) {
		l = (L2CAP *) fParentHandler->fHandlers[i];
		HLOG (1, "  Check: %08x %d %d\n", l, l->fProtocol, l->fConnectionState);
		if (l->fProtocol == prot && l->fConnectionState != CONN_CONNECTED) {
			l2cap = l;
		}
	}
	
	if (l2cap != NULL) {
		fState = L2CAP_CONNECTION_REQUEST;
		l2cap->fConnectionState = CONN_CONNECT;
		fRemoteIdentifier = 0;

		data[0] = EVT_L2CAP_CONNECTION_REQUEST;
		data[1] = NextLocalIdentifier ();
		SET_SHORT (data, 2, sizeof (data) - 4);
		
		SET_SHORT (data, 4, prot);
		SET_SHORT (data, 6, l2cap->fLocalCID);

		HLOG (1, "  Sending (%08x) ID: %d CID: %d\n",
			l2cap, fLocalIdentifier, l2cap->fLocalCID);
		
		SndData (data, sizeof (data));
		fServer->SetTimer (this, 2000, l2cap);
	}
}

void L2CAP::SndConnectionResponse (Short destinationCID, Short sourceCID, Short result, Short status)
{
	UByte data[12];
	
	HLOG (1, "L2CAP::SndConnectionResponse (%d to %d)\n",
		destinationCID, sourceCID);

	data[0] = EVT_L2CAP_CONNECTION_RESPONSE;
	data[1] = fRemoteIdentifier;
	SET_SHORT (data, 2, 8);
	
	SET_SHORT (data, 4, destinationCID);
	SET_SHORT (data, 6, sourceCID);
	SET_SHORT (data, 8, result);
	SET_SHORT (data, 10, status);
	
	SndData (data, sizeof (data));
}

void L2CAP::SndConfigureRequest (Short destinationCID)
{
	UByte data[12];
	
	fRemoteIdentifier = 0;

	data[0] = EVT_L2CAP_CONFIGURE_REQUEST;
	data[1] = NextLocalIdentifier ();
	SET_SHORT (data, 2, sizeof (data) - 4);
	
	SET_SHORT (data, 4, destinationCID);
	SET_SHORT (data, 6, 0);
	data[8] = CONF_MTU;
	data[9] = 2;
	SET_SHORT (data, 10, L2CAP_IN_MTU_LEN);
	/*
	data[12] = CONF_QOS;
	data[13] = 22;
	data[14] = 0; data[15] = 0x02;
	SET_LONG (data, 16, 0);
	SET_LONG (data, 20, 512);
	SET_LONG (data, 24, 0);
	SET_LONG (data, 28, 0);
	SET_LONG (data, 32, 0);
	*/
	
	HLOG (1, "L2CAP::SndConfigureRequest (Id %d, %d)\n",
		fLocalIdentifier, destinationCID);

	SndData (data, sizeof (data));
}

void L2CAP::SndConfigureResponse (Short sourceCID, Short MTU)
{
	UByte data[14];
	Short len;
	L2CAP *l2cap;

	if (MTU != 672) { 
		len = 14;
	} else {
		len = 10;
	}
	
	HLOG (1, "L2CAP::SndConfigureResponse (Id %d, %d)\n",
		fRemoteIdentifier, sourceCID);

	data[0] = EVT_L2CAP_CONFIGURE_RESPONSE;
	data[1] = fRemoteIdentifier;
	SET_SHORT (data, 2, len - 4);

	SET_SHORT (data, 4, sourceCID);
	SET_SHORT (data, 6, 0);
	SET_SHORT (data, 8, 0);

	if (MTU != 672) {
		HLOG (1, "  Accepting MTU %d\n", MTU);
		data[10] = CONF_MTU;
		data[11] = 2;
		SET_SHORT (data, 12, MTU);
	}

	SndData (data, len);
}

void L2CAP::SndDisconnectionRequest (Short destinationCID, Short sourceCID)
{
	UByte data[8];
	
	data[0] = EVT_L2CAP_DISCONNECTION_REQUEST;
	data[1] = NextLocalIdentifier ();
	SET_SHORT (data, 2, 4);
	
	SET_SHORT (data, 4, destinationCID);
	SET_SHORT (data, 6, sourceCID);
	
	HLOG (1, "L2CAP::SndDisconnectionRequest (%d to %d)\n",
		destinationCID, sourceCID);

	SndData (data, sizeof (data));
}

void L2CAP::SndDisconnectionResponse (Short destinationCID, Short sourceCID)
{
	UByte data[8];
	
	HLOG (1, "L2CAP::SndDisconnectionResponse\n");

	data[0] = EVT_L2CAP_DISCONNECTION_RESPONSE;
	data[1] = fRemoteIdentifier;
	SET_SHORT (data, 2, 4);
	
	SET_SHORT (data, 4, destinationCID);	
	SET_SHORT (data, 6, sourceCID);
	
	SndData (data, 8);
}

void L2CAP::SndEchoRequest ()
{
	HLOG (1, "L2CAP::SndEchoRequest\n");
}

void L2CAP::SndEchoResponse ()
{
	HLOG (1, "L2CAP::SndEchoResponse\n");
}

void L2CAP::SndInformationRequest ()
{
	HLOG (1, "L2CAP::SndInformationRequest\n");
}

void L2CAP::SndInformationResponse (Short infoType, Short result, Short respLength, UByte *resp)
{
	UByte data[8];
	
	HLOG (1, "L2CAP::SndInformationResponse %d %d %d\n", infoType, result, respLength);

	data[0] = EVT_L2CAP_INFORMATION_RESPONSE;
	data[1] = fRemoteIdentifier;
	SET_SHORT (data, 2, 4 + respLength);
	
	SET_SHORT (data, 4, infoType);	
	SET_SHORT (data, 6, result);
	
	SndData (data, 8);
	if (respLength > 0)	SndData (resp, respLength);
}
	
/*	
	switch (fInputPacket[0]) {
		case EVT_L2CAP_COMMAND_REJECT:
			CommandReject ();
			break;
		case EVT_L2CAP_CONNECTION_REQUEST:
			ConnectionRequest ();
			break;
		case EVT_L2CAP_CONNECTION_RESPONSE:
			ConnectionResponse ();
			break;
		case EVT_L2CAP_CONFIGURE_REQUEST:
			ConfigureRequest ();
			break;
		case EVT_L2CAP_CONFIGURE_RESPONSE:
			ConfigureResponse ();
			break;
		case EVT_L2CAP_DISCONNECTION_REQUEST:
			DisconnectionRequest ();
			break;
		case EVT_L2CAP_DISCONNECTION_RESPONSE:
			DisconnectionResponse ();
			break;
		case EVT_L2CAP_ECHO_REQUEST:
//			EchoRequest ();
			break;
		case EVT_L2CAP_ECHO_RESPONSE:
//			EchoResponse ();
			break;
		case EVT_L2CAP_INFORMATION_REQUEST:
//			InformationRequest ();
			break;
		case EVT_L2CAP_INFORMATION_RESPONSE:
//			InformationResponse ();
			break;
	}
*/
