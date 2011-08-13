#define DEFAULT_LOGLEVEL 0

#include <AEvents.h>
#include <AEventHandler.h>
#include <BufferSegment.h>
#include <CommManagerInterface.h>
#include <CommTool.h>
#include <SerialOptions.h>
#include <CommOptions.h>

#include "BluntServer.h"
#include "HCI.h"
#include "SDP.h"
#include "L2CAP.h"
#include "RFCOMM.h"

#define PF					0x10
#define COMMAND			 	0x02
#define RESPONSE			0x00
#define EA					0x01

#define CTRL_SABM	0x2f
#define CTRL_UA		0x63
#define CTRL_DM		0x0f
#define CTRL_DISC	0x43
#define CTRL_UIH	0xef

#define MPX_PN		0x20
#define MPX_PSC		0x10
#define MPX_CLD		0x30
#define MPX_Test	0x08
#define MPX_FCOn	0x28
#define MPX_FCOff	0x18
#define MPX_MSC		0x38
#define MPX_NSC		0x04
#define MPX_RPN		0x24
#define MPX_RLS		0x14
#define MPX_SNC		0x34

#define MPX_FLAG			0x100
#define EVT_RFCOMM_MPX_PN	(0x20 | MPX_FLAG)
#define EVT_RFCOMM_MPX_RPN	(0x24 | MPX_FLAG)
#define EVT_RFCOMM_MPX_MSC	(0x38 | MPX_FLAG)
#define EVT_RFCOMM_MPX_RLS	(0x14 | MPX_FLAG)
#define EVT_RFCOMM_SABM		0x2f
#define EVT_RFCOMM_UA		0x63
#define EVT_RFCOMM_DM		0x0f
#define EVT_RFCOMM_DISC		0x43
#define EVT_RFCOMM_UI		0xef
#define EVT_RFCOMM_TIMER	0xff

#define USE_CREDIT
#define REMOTE_DEFAULT_CREDIT 7
#define REMOTE_CREDIT_INCREASE 16
#define REMOTE_CREDIT_LOW 6

enum {
	RFCOMM_ANY,
	RFCOMM_MPX_CONNECT,
	RFCOMM_MPX_CONNECTED,
	RFCOMM_CONNECT,
	RFCOMM_CONNECTED,
	RFCOMM_IDLE
};

enum {
	A_NONE,
	A_MPX_CONNECTED,
	A_DATA_CONFIGURED,
	A_DATA_CONNECTED,
	A_MPX_DATA,				/* 5 */
	A_DATA,
	A_MPX_CONNECT_ERROR,
	A_RESEND_DATA_CONNECT,
	A_MPX_MSC
};

static const int kStates[][4] = {
	/* Connect states */
	{RFCOMM_MPX_CONNECT,		EVT_RFCOMM_UA,				A_MPX_CONNECTED,		RFCOMM_MPX_CONNECTED},
	{RFCOMM_MPX_CONNECTED,		EVT_RFCOMM_MPX_PN,			A_DATA_CONFIGURED,		RFCOMM_MPX_CONNECTED},
	
	{RFCOMM_CONNECT,			EVT_RFCOMM_TIMER,			A_RESEND_DATA_CONNECT,	RFCOMM_CONNECT},
	{RFCOMM_CONNECT,			EVT_RFCOMM_UA,				A_DATA_CONNECTED,		RFCOMM_CONNECTED},
	
	/* Multiplexer commands */
	{RFCOMM_MPX_CONNECTED,		EVT_RFCOMM_UI,				A_MPX_DATA,				RFCOMM_ANY},
	{RFCOMM_MPX_CONNECTED,		EVT_RFCOMM_MPX_MSC,			A_MPX_MSC,				RFCOMM_ANY},
	
	/* Data */
	{RFCOMM_CONNECTED,			EVT_RFCOMM_UI,				A_DATA,					RFCOMM_ANY},
	
	/* Disconnect states */
	{RFCOMM_MPX_CONNECT,		EVT_RFCOMM_DM,				A_MPX_CONNECT_ERROR,	RFCOMM_IDLE},
	{RFCOMM_MPX_CONNECTED,		EVT_RFCOMM_DM,				A_MPX_CONNECT_ERROR,	RFCOMM_IDLE},
};
	
RFCOMM::RFCOMM (void)
{
	fLogLevel = DEFAULT_LOGLEVEL;
	HLOG (1, "RFCOMM::RFCOMM\n");
	
	fHandlerType = H_RFCOMM;
	fInitiator = false;
	fOutputPacket = NULL;
	fInputPacket = NULL;
	fState = RFCOMM_IDLE;
	fConfigured = false;
	fClearToSend = true;
    fCreditReceived = 0;
    fCreditGiven = 0;
	fRemoteMTU = 0;
	fDLCI = 0;
    fTotalBytesReceived = 0;
	CreateCRCTable ();
	fConnectionState = CONN_IDLE;
    fUseCredit = true;
}
	
RFCOMM::~RFCOMM (void)
{
}
	
#pragma mark -

int RFCOMM::Action (int action, void *eventData)
{
	int newState;
	RFCOMM* rfcomm;

	newState = RFCOMM_ANY;
	HLOG (1, "RFCOMM::Action (%08x) %d %d\n", this, fState, action);
	switch (action) {
		case A_MPX_CONNECTED:
			UnnumberedAcknowlegdement ();
			MPXConnected ();
			break;
		case A_DATA_CONFIGURED:
			rfcomm = MPXParameterNegotiation (fMPXCR);
			if (rfcomm != NULL) {
				rfcomm->SndSetAsynchronousBalancedMode ();
			}
			break;
		case A_DATA_CONNECTED:
			SndMPXModemStatusCommand (fDLCI, true, false, 0x8c);
			CompleteConnection ();
			break;
		case A_RESEND_DATA_CONNECT:
			SndSetAsynchronousBalancedMode ();
			break;
		case A_MPX_CONNECT_ERROR:
			HLOG (1, "Disconnect command");
		case A_MPX_MSC:
			MPXModemStatusCommand ((fInputPacket[0] & COMMAND) == COMMAND ? true : false);
			break;
		case A_DATA:
			UnnumberedInformation ();
			break;
	}
	return newState;
}

void RFCOMM::Transition (ULong event, void *eventData)
{
	int i;
	int newState;
	
	HLOG (1, "RFCOMM::Transition (%08x) %d %02x -> ", this, fState, event);
	for (i = 0; i < NUM_ELEMENTS(kStates); i++) {
		if ((fState == kStates[i][0] || kStates[i][0] == RFCOMM_ANY) && event == kStates[i][1]) {
			HLOG (1, "%d\n", i);
			newState = Action (kStates[i][2], eventData);
			if (newState != RFCOMM_ANY) {
				fState = newState;
			} else if (kStates[i][3] != RFCOMM_ANY) {
				fState = kStates[i][3];
			}
			break;
		}
	}
	HLOG (1, "New state: %d\n", fState);
}

Boolean RFCOMM::HandleData (UByte *data, ULong length)
{
	Boolean r;
	ULong control;
	ULong mpx;
	Byte DLCI;
	
	DLCI = data[0] >> 2;
	HLOG (1, "RFCOMM::HandleData %08x (%d %d)\n", data, DLCI, fDLCI);
	r = false;
	if (DLCI == fDLCI) {
		if ((data[0] & COMMAND) == COMMAND) fCR = true; else fCR = false;
		if ((data[1] & PF) == PF) fPF = true; else fPF = false;
		control = data[1] & 0xef;
		
		if ((data[2] & 0x01) != 0x01) {
			fInputPacketLength = (data[2] >> 1) + (data[3] << 7);
			fInputPacket = data + 4;
		} else {
			fInputPacketLength = data[2] >> 1;
			fInputPacket = data + 3;
		}
		
		if (fInputPacket[0] & 0x01 == 0x01) {
			mpx = (fInputPacket[0] >> 2) | MPX_FLAG;
			fMPXCR = (fInputPacket[0] & COMMAND) == COMMAND ? true : false;
		}
		else mpx = 0;
        
		HLOG (1, " Address: %d Control: %02x MPX: %02x, Length: %d\n", DLCI, control, mpx, fInputPacketLength);
		
		if (fDLCI == 0 && control == EVT_RFCOMM_UI) Transition (mpx, NULL);
		else Transition (control, NULL);
		r = true;
	}
	
	return r;
}

void RFCOMM::HandleTimer (void *userData)
{
	HLOG (1, "RFCOMM::HandleTimer\n");
	Transition (EVT_RFCOMM_TIMER, userData);
}

void RFCOMM::Connect (void)
{
	RFCOMM *control;

	HLOG (1, "RFCOMM::Connect (%08x)\n", this);
	fConnectionState = CONN_CONNECT;
	if (fParentHandler->fConnectionState != CONN_CONNECTED) {
		((L2CAP *) fParentHandler)->Connect ();
	} else {
		control = (RFCOMM *) fParentHandler->fHandlers[0];
		if (this == control || control->fConnectionState == CONN_CONNECTED) {
			SndSetAsynchronousBalancedMode ();
		} else {
			control->SndSetAsynchronousBalancedMode ();
		}
	}
}

void RFCOMM::MPXConnected (void)
{
	int i;
	RFCOMM *r, *rfcomm;
	
	HLOG (1, "RFCOMM::MPXConnected (%08x)\n", this);
	fConnectionState = CONN_CONNECTED;
	for (rfcomm = NULL, i = 1; rfcomm == NULL && i < fParentHandler->fNumHandlers; i++) {
		r = (RFCOMM *) fParentHandler->fHandlers[i];
		HLOG (1, "  %d %d %d\n", i, r->fPort, r->fConnectionState);
		if (r->fConnectionState == CONN_CONNECT) {
			rfcomm = r;
		}
	}
	if (rfcomm != NULL) {
		HLOG (1, "  Connecting %08x (%d)\n", rfcomm, rfcomm->fPort);
        if (fUseCredit) {
            SndMPXParameterNegotiation (rfcomm->fPort << 1, 0x0f, fMTU, REMOTE_DEFAULT_CREDIT);
            rfcomm->fCreditGiven = REMOTE_DEFAULT_CREDIT;
        } else {
            SndMPXParameterNegotiation (rfcomm->fPort << 1, 0, fMTU, 0);
        }
	}
}

void RFCOMM::CompleteConnection (void)
{
	HLOG (1, "RFCOMM::CompleteConnection (%08x)\n", this);
	fConnectionState = CONN_CONNECTED;
	if (fTool) {
		BluntConnectionCompleteEvent *e;
		TUPort p (fTool);

		e = new BluntConnectionCompleteEvent (noErr);
		e->fHCIHandle = ((HCI *) fParentHandler->fParentHandler)->fConnectionHandle;
		e->fL2CAPLocalCID = ((L2CAP *) fParentHandler)->fLocalCID;
		e->fL2CAPRemoteCID = ((L2CAP *) fParentHandler)->fRemoteCID;
		e->fRFCOMMPort = fPort;
		e->fHandler = this;
		p.Send ((TUAsyncMessage *) e, e, sizeof (BluntConnectionCompleteEvent), kNoTimeout, nil, BLUNT_MSG_TYPE);
	}
	fTotalBytesSent = 0;
	fTotalBytesReceived = 0;
}

void RFCOMM::CompleteDisconnect (void)
{
	HLOG (1, "RFCOMM::CompleteDisconnect (%08x)\n", this);
	fConnectionState = CONN_DISCONNECT;
	if (fTool) {
		TUPort p (fTool);
		BluntDisconnectCompleteEvent *e;
		e = new BluntDisconnectCompleteEvent (noErr);
		p.Send ((TUAsyncMessage *) e, e, sizeof (BluntDisconnectCompleteEvent), kNoTimeout, nil, BLUNT_MSG_TYPE);
	}
	fTotalBytesSent = 0;
	fTotalBytesReceived = 0;
}

void RFCOMM::SetAsynchronousBalancedMode ()
{
	HLOG (1, "RFCOMM::SetAsynchronousBalancedMode\n");

	fInitiator = false;
	SndUnnumberedAcknowlegdement ();
}

void RFCOMM::UnnumberedAcknowlegdement ()
{
	HLOG (1, "RFCOMM::UnnumberedAcknowlegdement (%d)\n", fDLCI);
	if (fRemoteMTU > 0) {
		fMTU = MIN(RFCOMM_MTU_LEN, fRemoteMTU - 8);
	} else {
		fMTU = RFCOMM_MTU_LEN;
	}
}

void RFCOMM::DisconnectedMode ()
{
	HLOG (1, "RFCOMM::DisconnectedMode\n");
}

void RFCOMM::Disconnect ()
{
	HLOG (1, "RFCOMM::Disconnect\n");

	SndUnnumberedAcknowlegdement ();
	fState = RFCOMM_IDLE;
}

int RFCOMM::UnnumberedInformation ()
{
	int r;
	
	HLOG (1, "RFCOMM::UnnumberedInformation\n");
	r = noErr;
	if (fDLCI == 0) {
		ProcessMultiplexerCommands ();
	} else {
		r = ProcessRFCOMMData ();
	}
	return r;
}

void RFCOMM::SndSetAsynchronousBalancedMode ()
{
	UByte response[4];

	HLOG (1, "RFCOMM::SndSetAsynchronousBalancedMode %08x (%d)\n", this, fDLCI);
	if (fDLCI == 0) fState = RFCOMM_MPX_CONNECT;
	else fState = RFCOMM_CONNECT;

	fInitiator = true;
	response[0] = (fDLCI << 2) | COMMAND | EA;
	response[1] = CTRL_SABM | PF;
	response[2] = (0 << 1) | EA;
	response[3] = CalculateCRC (response, 3);
	SndData (response, sizeof (response));
}

void RFCOMM::SndUnnumberedAcknowlegdement ()
{
	UByte response[4];

	HLOG (1, "RFCOMM::SndUnnumberedAcknowlegdement\n");
	
	if (fDLCI != 0) {
		fState = RFCOMM_CONNECTED;
	}
	response[0] = (fDLCI << 2) | COMMAND | EA;
	response[1] = CTRL_UA | PF;
	response[2] = (0 << 1) | EA;
	response[3] = CalculateCRC (response, 3);
	SndData (response, sizeof (response));
}

void RFCOMM::SndDisconnectedMode ()
{
	HLOG (1, "RFCOMM::SndDisconnectedMode\n");
}

void RFCOMM::SndDisconnect ()
{
	UByte response[4];

	HLOG (1, "RFCOMM::SndDisconnect\n");

	if (fState == RFCOMM_CONNECTED) {
		response[0] = (fDLCI << 2) | COMMAND | EA;
		response[1] = CTRL_DISC | PF;
		response[2] = (0 << 1) | EA;
		response[3] = CalculateCRC (response, 3);
		SndData (response, sizeof (response));
	}
}

void RFCOMM::SndData (UByte *data, Short length)
{
	HLOG (1, "RFCOMM::SndData\n");

	fServer->BufferOutput (true);
	((L2CAP *) fParentHandler)->SndPacketHeader (length);
	fServer->Output (data, length);
}

void RFCOMM::SndUnnumberedInformation (UByte *data, Short len)
{
	UByte header[5];
	UByte crc;
	Short headerLen;
    ULong credit;
	BluntDataSentEvent *e;
	TTime delay, *d;
	Boolean delayConfirmation;
	TUPort p (fTool);
	
	HLOG (1, "RFCOMM::SndUnnumberedInformation (%d %d)\n", fDLCI, len);
	header[0] = (fDLCI << 2) | COMMAND | EA; 
	header[1] = CTRL_UIH;

	if (len > 127) {
		header[2] = (len & 0x007f) << 1;
		header[3] = (len & 0x7f80) >> 7;
		headerLen = 4;
	} else {
		header[2] = (len << 1) | EA;
		headerLen = 3;
	}

	delayConfirmation = false;
	d = nil;
    if (fUseCredit) {
        HLOG (1, " Remote credit %d\n", fCreditGiven);
        fCreditReceived--;
        if (fCreditReceived < REMOTE_CREDIT_LOW) {
            HLOG (0, "*** Received credit low: %d\n", fCreditReceived);
			delayConfirmation = true;
        }
        if (fCreditGiven < REMOTE_CREDIT_LOW) {
            header[1] |= PF;
            if (true /*!fServer->RcvBufferLevelCritical ()*/) {
                credit = REMOTE_CREDIT_INCREASE;
            } else {
                credit = 1;
				delayConfirmation = true;
            }
            header[headerLen++] = credit;
            fCreditGiven += credit;
            HLOG (1, " Credit given: %d\n", credit);
        }
    }

	crc = CalculateCRC (header, 2);

	((L2CAP *) fParentHandler)->SndPacketHeader (headerLen + len + sizeof (crc));
	fServer->Output (header, headerLen);
	fServer->Output (data, len);
	fServer->Output (&crc, sizeof (crc));

	if (!delayConfirmation) delayConfirmation = ((HCI *)(fParentHandler->fParentHandler))->IsWindowCritical ();

	e = new BluntDataSentEvent (noErr, len);
	if (delayConfirmation) {
		HLOG (1, "  Delaying receipt confirmation");
		delay = GetGlobalTime () + TTime (200, kMilliseconds);
		d = &delay;
	}
	p.Send ((TUAsyncMessage *) e, e, sizeof (BluntDataSentEvent), kNoTimeout, d, BLUNT_MSG_TYPE);
}

void RFCOMM::ProcessMultiplexerCommands (void)
{
	Byte command;
	Short length;
	Boolean cr;
	
	HLOG (1, "RFCOMM::ProcessMultiplexerCommands\n");
	
	if ((fInputPacket[0] & 0x01) == 0x01) {
		cr = (fInputPacket[0] & COMMAND) == COMMAND ? true : false;
		command = fInputPacket[0] >> 2;
		
		switch (command) {
			case MPX_PN:
				MPXParameterNegotiation (cr);
				break;
			case MPX_RPN:
				MPXRemotePortNegotiation (cr);
				break;
			case MPX_MSC:
				MPXModemStatusCommand (cr);
				break;
			case MPX_PSC:
			case MPX_CLD:
			case MPX_Test:
			case MPX_FCOn:
			case MPX_FCOff:
			case MPX_NSC:
			case MPX_RLS:
			case MPX_SNC:
				HLOG (1, "  %02x\n", command);
				break;
		}
	}
}

int RFCOMM::ProcessRFCOMMData (void)
{
	Short len;
	int r, i;

	HLOG (1, "RFCOMM::ProcessRFCOMMData %d\n", fInputPacketLength);
	r = noErr;
    if (fUseCredit) {
        HLOG (1, " Remote credit: %d\n", fCreditGiven);
        if (fPF) {
            fCreditReceived += fInputPacket[0];
            HLOG (1, " Got %d credit, total %d\n", fInputPacket[0], fCreditReceived);
            fInputPacket++;
        }
    }

	if (fInputPacketLength > 0) {
		if (((fTotalBytesReceived + fInputPacketLength) / 1024) > (fTotalBytesReceived / 1024)) {
			HLOG (1, " Total bytes received %d\n", fTotalBytesReceived);
		}
		fTotalBytesReceived += fInputPacketLength;
		if (fTool) {
			HLOG (1, "  Sending %d bytes to tool %08x\n", fInputPacketLength, fTool);
			BluntDataEvent *e;
			TUPort p (fTool);
			ULong n;
			e = new BluntDataEvent (noErr, fInputPacket, fInputPacketLength, this);
			p.Send ((TUAsyncMessage *) e, e, sizeof (BluntDataEvent), kNoTimeout, nil, BLUNT_MSG_TYPE);
			HLOG (1, "  Data sent.\n", fTool);
		}
	}
    
    if (fUseCredit) {
        fCreditGiven--;
        if (fCreditGiven < REMOTE_CREDIT_LOW) {
            SndUnnumberedInformation (NULL, 0);
        }
    } else {
        if (!fClearToSend) {
            if (fServer->RcvBufferLevelOk ()) {
                SndMPXFCOn ();
                fClearToSend = true;
            }
        } else if (fServer->RcvBufferLevelCritical ()) {
            SndMPXFCOff ();
            fClearToSend = false;
        }
    }	
	return r;
}

RFCOMM* RFCOMM::MPXParameterNegotiation (Boolean command)
{
	Byte negotiationDLCI;
	Byte flowControl;
	Short frameSize;
	Byte windowSize;
	RFCOMM *handler;
	
	HLOG (1, "RFCOMM::MPXParameterNegotiation %d\n", command);
	negotiationDLCI = fInputPacket[2];
	flowControl = fInputPacket[3] >> 4;
	frameSize = fInputPacket[6] + (fInputPacket[7] << 8);
	windowSize = fInputPacket[9];
	handler = HandlerFromDLCI (negotiationDLCI);
	handler->fRemoteMTU = frameSize;
	HLOG (1, "  DLCI: %d Flow: %02x Frame size: %d, Window: %d\n",
		negotiationDLCI, flowControl, frameSize, windowSize);

    if (flowControl == 0) {
        fUseCredit = false;
        handler->fUseCredit = false;
    } else {
        handler->fCreditReceived = windowSize;
    }

	if (command) {
        if (flowControl == 0) {
            SndMPXParameterNegotiation (negotiationDLCI, 0, 128, 0);
        } else {
            SndMPXParameterNegotiation (negotiationDLCI, 0x0f, 128, REMOTE_DEFAULT_CREDIT);
            handler->fCreditGiven = REMOTE_DEFAULT_CREDIT;
        }
/*	} else {
		CompleteConnection (); */
	}
	return handler;
}

void RFCOMM::MPXRemotePortNegotiation (Boolean command)
{
	HLOG (1, "RFCOMM::MPXRemotePortNegotiation\n");
}

void RFCOMM::MPXModemStatusCommand (Boolean command)
{
	Byte DLCI;

	DLCI = fInputPacket[2] >> 2;
	HLOG (1, "RFCOMM::MPXModemStatusCommand (%d %d %d 0x%02x)\n",
		DLCI, command, fPF, fInputPacket[3]);
	if (command) {
		SndMPXModemStatusCommand (DLCI, false, fPF, fInputPacket[3]);
	}
}

void RFCOMM::SndMPXParameterNegotiation (Byte negotiationDLCI, Byte flowControl, Short frameSize, Byte windowSize)
{
	UByte response[14];

	HLOG (1, "RFCOMM::SndMPXParameterNegotiation (%d %d %d)\n", fInitiator, negotiationDLCI, frameSize);

	response[0] = (0 << 2) | (fInitiator ? COMMAND : RESPONSE) | EA; 
	response[1] = CTRL_UIH | PF; 
//	response[1] = CTRL_UIH | 0; // T68i XXX
	response[2] = ((sizeof (response) - 4) << 1) | EA;
	
	response[3] = (MPX_PN << 2) | (fInitiator ? COMMAND : RESPONSE) | EA;
	response[4] = ((sizeof (response) - 6) << 1) | EA;
	response[5] = negotiationDLCI;
	response[6] = flowControl << 4 | 0; // CL flow control & I-Frames
	response[7] = 0; // Priority
	response[8] = 0; // ACK timer
	response[9] = frameSize & 0x00ff; // Frame size
	response[10] = frameSize >> 8;
	response[11] = 0; // Retransmissions;
	response[12] = windowSize; // Window size
	
	response[13] = CalculateCRC (response, 2);
	SndData (response, sizeof (response));
}

void RFCOMM::SndMPXRemotePortNegotiation (void)
{
	HLOG (1, "RFCOMM::SndMPXRemotePortNegotiation\n");
}

void RFCOMM::SndMPXModemStatusCommand (Byte DLCI, Boolean isCommand, Boolean pf, Byte status)
{
	UByte response[8];

	HLOG (1, "RFCOMM::SndMPXModemStatusCommand (%d %d %d)\n", DLCI, fInitiator, isCommand);

	response[0] = (0 << 2) | (fInitiator ? COMMAND : RESPONSE) | EA; 
	response[1] = CTRL_UIH | (pf ? PF : 0);
	response[2] = ((sizeof (response) - 4) << 1) | EA;
	
	response[3] = (MPX_MSC << 2) | (isCommand ? COMMAND : RESPONSE) | EA;
	response[4] = ((sizeof (response) - 6) << 1) | EA;
	response[5] = (DLCI << 2) | 0x02 | EA;
	response[6] = status | 0x01;
	
	response[7] = CalculateCRC (response, 2);
	SndData (response, sizeof (response));
	fServer->fDebug++;
}

void RFCOMM::SndMPXFCOff ()
{
	UByte response[6];

	HLOG (1, "RFCOMM::SndMPXFCOff\n");

	response[0] = (0 << 2) | (fInitiator ? COMMAND : RESPONSE) | EA; 
	response[1] = CTRL_UIH | PF;
	response[2] = ((sizeof (response) - 4) << 1) | EA;
	
	response[3] = (MPX_FCOff << 2) | COMMAND | EA;
	response[4] = ((sizeof (response) - 6) << 1) | EA;
	
	response[5] = CalculateCRC (response, 2);
	SndData (response, sizeof (response));
}

void RFCOMM::SndMPXFCOn ()
{
	UByte response[6];

	HLOG (1, "RFCOMM::SndMPXFCOn\n");

	response[0] = (0 << 2) | (fInitiator ? COMMAND : RESPONSE) | EA; 
	response[1] = CTRL_UIH | PF;
	response[2] = ((sizeof (response) - 4) << 1) | EA;
	
	response[3] = (MPX_FCOn << 2) | COMMAND | EA;
	response[4] = ((sizeof (response) - 6) << 1) | EA;
	
	response[5] = CalculateCRC (response, 2);
	SndData (response, sizeof (response));
}

void RFCOMM::CreateCRCTable ()
{
	int i ,j;
	UByte data;
	UByte code_word = 0xe0; // pol = x8+x2+x1+1
	UByte sr = 0;			// Shiftregister initiated to zero
	
	for (j = 0; j < 256; j++) {
		data = (UByte) j;
		for (i = 0; i < 8; i++) {
			if ((data & 0x1) ^ (sr & 0x1)) {
				sr >>= 1;
				sr ^= code_word;
			} else {
				sr >>= 1;
			}
			data >>= 1;
			sr &= 0xff;
		}
		fCRCTable[j] = sr;
		sr = 0;
	} 
}

UByte RFCOMM::CalculateCRC (UByte *data, Byte length)
{
	UByte fcs = 0xff;

	while (length--) {
		fcs = fCRCTable[fcs ^ *data++];
	}
	
	return 0xff-fcs;
} 

RFCOMM *RFCOMM::HandlerFromDLCI (Byte DLCI)
{
	int i;
	RFCOMM *rfcomm;
	
	rfcomm = NULL;
	HLOG (1, "RFCOMM::HandlerFromDLCI (%08x) %d ", this, DLCI);
	for (i = 1; rfcomm == NULL && i < fParentHandler->fNumHandlers; i++) {
		if (fParentHandler->fHandlers[i]->fHandlerType == H_RFCOMM &&
			((RFCOMM *) fParentHandler->fHandlers[i])->fDLCI == DLCI) {
			rfcomm = (RFCOMM *) fParentHandler->fHandlers[i];
			HLOG (1, "%08x", rfcomm);
		}
	}
	HLOG (1, "\n");
	return rfcomm;
}

