#ifndef __L2CAPLAYER_H
#define __L2CAPLAYER_H

#include "BluntServer.h"
#include "HCI.h"

#define L2CAP_IN_MTU_LEN	 490
#define L2CAP_OUT_MTU_LEN	 490

#define CID_NULL 		0x0000
#define CID_SIGNAL		0x0001
#define CID_CL_RCV		0x0002

#define CONT_FOLLOW		0x01
#define CONT_FIRST		0x02

#define BC_P2P			0x00
#define BC_PARK			0x01
#define BC_NONPARK		0x02

#define PSM_SDP			0x0001
#define PSM_RFCOMM		0x0003
#define PSM_TELEPHONY	0x0005

#define CONF_MTU			0x01
#define CONF_FLUSH_TIMEOUT	0x02
#define CONF_QOS			0x03

#define MAX_L2CAP_CHANNELS	10
#define L2CAP_CID_BASE		200

#define CONFIGURE_DONE		0
#define CONFIGURE_ONE		1
#define CONFIGURE_TWO		2

class L2CAP: public Handler
{
public:
	// Packet identifiers
	
	Boolean					fTimeout;
	Byte					fLocalIdentifier;
	Byte					fRemoteIdentifier;
	Byte					fOutstandingRequestID;
	Short					fLastCID;
	
	// Information about the current L2CAP input packet
	
	UByte					fInputPacket[L2CAP_IN_MTU_LEN * 2];
	Long					fInputPacketLength;
	Long					fCurrentInputPacketLength;
	Short					fInputPacketCID;
	Boolean					fContinuationActive;
	
	// Information about the current L2CAP output packet
	
	UByte					fOutputPacket[L2CAP_OUT_MTU_LEN];
	Long					fOutputPacketLength;

	// Temporary CID (used in config phase)

	Short					fTempCID;
	
	// Configuration state and protocol for control channel
	
	Byte					fConfigureState;
	Short					fTargetProtocol;
	
	Short					fLocalCID;
	Short					fRemoteCID;
	Short					fProtocol;
	Short					fRemoteMTU;
	
	Byte					fConnectionAttempts;
	
							L2CAP ();
							~L2CAP ();
	void					Transition (ULong event, void *eventData);
	int						Action (int state, void *eventData);
	
	Boolean					HandleData (UByte *data, ULong length);
	void					HandleTimer (void *userData);
	
	void					ProcessACLEvent (void);
	Boolean					ProcessL2CAPData (void);
	
	void					CompleteConnection (void);
	void					ConnectionError (NewtonErr error);
	
	void					Connect (void);
	
	// Utility functions
	
	Byte					NextLocalIdentifier (void);
	Short					GetNewCID ();
	L2CAP*					GetHandler (Short sourceCID);
	
	// Event handling
	
	NewtonErr				Processvent (UByte *event);

	// Responses
	
	void					CommandReject (void);
	L2CAP*					ConnectionRequest (void);
	L2CAP*					ConnectionResponse (void);
	L2CAP*					ConfigureRequest (void);
	L2CAP*					ConfigureResponse (void);
	L2CAP*					DisconnectionRequest (void);
	void					DisconnectionResponse (void);
	void					EchoRequest (void);
	void					EchoResponse (void);
	void					InformationRequest (void);
	void					InformationResponse (void);
	
	// Requests
	
	void 					SndData (UByte *data, Short length);
	void					SndPacketHeader (Short length);
	
	void					SndCommandReject ();
	void					SndConnectionRequest (Short prot);
	void					SndConnectionResponse (Short destinationCID, Short sourceCID, Short result, Short status);
	void					SndConfigureRequest (Short sourceCID);
	void					SndConfigureResponse (Short sourceCID, Short MTU);
	void					SndDisconnectionRequest (Short destinationCID, Short sourceCID);
	void					SndDisconnectionResponse (Short destinationCID, Short sourceCID);
	void					SndEchoRequest ();
	void					SndEchoResponse ();
	void					SndInformationRequest ();
	void					SndInformationResponse (Short infoType, Short result, Short respLength, UByte *resp);
};

#endif
