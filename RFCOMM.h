#ifndef __TRFCOMMLAYER_H
#define __TRFCOMMLAYER_H

#define RFCOMM_MTU_LEN 384

class RFCOMM: public Handler
{
public:
	Byte					fPort;
	Byte					fDLCI;
	
	UByte					*fInputPacket;
	UShort					fInputPacketLength;
	
	UByte					*fOutputPacket;
	UShort					fOutputPacketLength;
	
	Boolean					fCR;
	Boolean					fPF;
	Boolean					fInitiator;
	Boolean					fMPXCR;
	
	Boolean					fClearToSend;
	
	Boolean					fConfigured;
	Short					fMTU;
	Short					fRemoteMTU;
	
	ULong					fTotalBytesSent;
	ULong					fTotalBytesReceived;
    
    Boolean                 fUseCredit;
    Long                    fCreditReceived;
    Long                    fCreditGiven;

	Boolean					fBlockedByHCI;
	Boolean					fBlockedByLowCredit;
	
	UByte					fCRCTable[256];

							RFCOMM (void);
							~RFCOMM (void);
	
	void					Connect (void);
							
	int						Action (int action, void *eventData);
	void					Transition (ULong event, void *eventData);
	
	Boolean					HandleData (UByte *data, ULong length);
	void					HandleTimer (void *userData);
	int						ProcessRFCOMMEvent (UByte *data);
	NewtonErr				ProcessEvent (UByte event, UByte *data, Byte DLCI);

	virtual	void			HCIClearToSend (Boolean isClear);

	void					CompleteConnection (void);
	void					CompleteDisconnect (void);
	void					MPXConnected (void);
	void					ConnectionError (void);
	
	void					SetAsynchronousBalancedMode ();
	void					UnnumberedAcknowlegdement ();
	void					DisconnectedMode ();
	void					Disconnect ();
	int						UnnumberedInformation ();

	void					SndSetAsynchronousBalancedMode ();
	void					SndUnnumberedAcknowlegdement ();
	void					SndDisconnectedMode ();
	void					SndDisconnect ();
	void					SndMPXFCOff ();
	void					SndMPXFCOn ();
	void					SndUnnumberedInformation (UByte *data, Short length);
	
	void					SndData (UByte *data, Short length);

	void					ProcessMultiplexerCommands (void);
	int						ProcessRFCOMMData (void);

	RFCOMM					*MPXParameterNegotiation (Boolean command);
	void					MPXRemotePortNegotiation (Boolean command);
	void					MPXModemStatusCommand (Boolean command);

	void					SndMPXParameterNegotiation (Byte negotiationDLCI, Byte flowControl, Short frameSize, Byte windowSize);
	void					SndMPXRemotePortNegotiation (void);
	void					SndMPXModemStatusCommand (Byte DLCI, Boolean isCommand, Boolean pf, Byte status);
	
	void					CreateCRCTable (void);
	UByte					CalculateCRC (UByte *data, Byte Length);
	
	RFCOMM					*HandlerFromDLCI (Byte DLCI);
};

#endif