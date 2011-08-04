#ifndef __HCI_H
#define __HCI_H

class HCI: public Handler
{
public:
	Boolean					fLinkKeyValid;
	UByte					fLinkKey[16];	
	UByte					fPeerAddress[6];
	UByte					fPSRepMode;
	UByte					fPSMode;

	UByte					fPairAddress[6];
	Char					fPairPINCode[64];

	UByte					fPacket[2048];
	ULong					fPacketLength;
	
	Short 					fConnectionHandle;

	Short					fHCIBufferSize;
	Byte					fHCIWindowSize;
	Short					fOutstandingPackets;
	Char					fName[64];

	HCI*					fControlChannel;
	
							HCI (void);
							~HCI ();
	
	Boolean					CheckPacketComplete (TExtendedCircleBuf *buffer, Boolean alwaysConsume);
	Boolean					IsMyPacket (TExtendedCircleBuf *buffer);
	Boolean					HandleData (TExtendedCircleBuf *buffer);
	void					RemoveUnhandledPackets (TExtendedCircleBuf *buffer);

	Boolean					CheckPacketComplete (Boolean alwaysConsume);
	Boolean					IsMyPacket ();
	Boolean					HandleData ();
	void					RemoveUnhandledPackets ();

	void					HandleTimer (void *userData);
	
	void					Transition (ULong event, void *eventData);
	int						Action (int state, void *eventData);

	void					ProcessACLData (void);
	
	HCI*					GetHandler (Short handle);
	HCI*					GetHandler (UByte *);
	
	// Packet header
	
	void					SndPacketHeader (Byte flags, Size length);

	// Event handlers

	Boolean					ResetState ();
	Boolean					ConnectState ();
	Boolean					ConnectedState ();
	Boolean					DisconnectState ();
	Boolean					AcceptState ();
	Boolean					DiscoverState ();
	Boolean					PairIncomingState ();
	Boolean					PairOutgoingState ();
	
	void					CompleteConnection (NewtonErr err);
	void					SetConnectionData ();
	void					CompleteDisconnect (Short handle);
	void					CompleteDisconnect (UByte *addr);
	void					TrackCompletedPackets ();
	
	// High level commands
	
	void					Pair (UByte *addr, Char *PIN);
	void					Connect ();
	void					Status ();

	// Link Control Commands

	void					Inquiry (UByte time, UByte amount);
	void					InquiryCancel (void);
	void					PeriodicInquiryMode (void);
	void					ExitPeriodicInquiryMode (void);
	void					CreateConnection (UByte *bd_addr, Byte psRep, Byte psMode);
	void					Disconnect (Byte reason);
	void					AddSCOConnection (void);
	void					AcceptConnectionRequest (UByte *bd_addr, Byte role);
	void					RejectConnectionRequest (void);
	void					LinkKeyRequestReply (UByte *bd_addr, UByte *linkKey);
	void					LinkKeyRequestNegativeReply (UByte *bd_addr);
	void					PINCodeRequestReply (UByte *bd_addr, Char *PINCode);
	void					PINCodeRequestNegativeReply (void);
	void					ChangeConnectionPacketType (void);
	void					AuthenticationRequested (Short handle);
	void					SetConnectionEncryption (void);
	void					ChangeConnectionLinkKey (void);
	void					MasterLinkKey (void);
	void					RemoteNameRequest (UByte *bdAddr, Byte psRep, Byte psMode);
	void					ReadRemoteSupportedFeatures (void);
	void					ReadRemoteVersionInformation (void);
	void					ReadClockOffset (void);

	// Link Policy Commands 

	void					HoldMode (void);
	void					SniffMode (void);
	void					ExitSniffMode (void);
	void					ParkMode (void);
	void					ExitParkMode (void);
	void					QoSSetup (void);
	void					RoleDiscovery (void);
	void					SwitchRole (void);
	void					ReadLinkPolicySettings (void);
	void					WriteLinkPolicySettings (void);

	// Host Controller & Baseband Commands

	void					SetEventMask (void);
	void					Reset (void);
	void					SetEventFilter (void);
	void					Flush (void);
	void					ReadPINType (void);
	void					WritePINType (void);
	void					CreateNewUnitKey (void);
	void					ReadStoredLinkKey (void);
	void					WriteStoredLinkKey (void);
	void					DeleteStoredLinkKey (void);
	void					ChangeLocalName (UByte *name);
	void					ReadLocalName (void);
	void					ReadConnectionAcceptTimeout (void);
	void					WriteConnectionAcceptTimeout (void);
	void					ReadPageTimeout (void);
	void					WritePageTimeout (void);
	void					ReadScanEnable (void);
	void					WriteScanEnable (UByte enable);
	void					ReadPageScanActivity (void);
	void					WritePageScanActivity (UShort interval, UShort window);
	void					ReadInquiryScanActivity (void);
	void					WriteInquiryScanActivity (UShort interval, UShort window);
	void					ReadAuthenticationEnable (void);
	void					WriteAuthenticationEnable (void);
	void					ReadEncryptionMode (void);
	void					WriteEncryptionMode (void);
	void					ReadClassofDevice (void);
	void					WriteClassofDevice (UByte *deviceClass);
	void					ReadVoiceSetting (void);
	void					WriteVoiceSetting (void);
	void					ReadAutomaticFlushTimeout (void);
	void					WriteAutomaticFlushTimeout (void);
	void					ReadNumBroadcastRetransmissions (void);
	void					WriteNumBroadcastRetransmissions (void);
	void					ReadHoldModeActivity (void);
	void					WriteHoldModeActivity (void);
	void					ReadTransmitPowerLevel (void);
	void					ReadSCOFlowControlEnable (void);
	void					WriteSCOFlowControlEnable (void);
	void					SetHostControllerToHostFlowControl (void);
	void					HostBufferSize (void);
	void					HostNumberOfCompletedPackets (void);
	void					ReadLinkSupervisionTimeout (void);
	void					WriteLinkSupervisionTimeout (void);
	void					ReadNumberOfSupportedIAC (void);
	void					ReadCurrentIACLAP (void);
	void					WriteCurrentIACLAP (void);
	void					ReadPageScanPeriodMode (void);
	void					WritePageScanPeriodMode (void);
	void					ReadPageScanMode (void);
	void					WritePageScanMode (void);

	// Informational Parameters

	void					ReadLocalVersionInformation (void);
	void					ReadLocalSupportedFeatures (void);
	void					ReadBufferSize (void);
	void					ReadCountryCode (void);
	void					ReadBdAddr (void);

	// Status Parameters 

	void					ReadFailedContactCounter (void);
	void					ResetFailedContactCounter (void);
	void					GetLinkQuality (Short handle);
	void					ReadRSSI (void);

	// Testing Commands 

	void					ReadLoopbackMode (void);
	void					WriteLoopbackMode (void);
	void					EnableDeviceUnderTestMode (void);

	// Possible Events 

	void					InquiryComplete (void);
	void					InquiryResult (void);
	void					ConnectionComplete (void);
	void					ConnectionRequest (void);
	void					DisconnectionComplete (void);
	void					AuthenticationComplete (void);
	void					RemoteNameRequestComplete (void);
	void					EncryptionChange (void);
	void					ChangeConnectionLinkKeyComplete (void);
	void					MasterLinkKeyComplete (void);
	void					ReadRemoteSupportedFeaturesComplete (void);
	void					ReadRemoteVersionInformationComplete (void);
	void					QoSSetupComplete (void);
	void					CommandComplete (void);
	void					CommandStatus (void);
	void					HardwareError (void);
	void					FlushOccurred (void);
	void					RoleChange (void);
	void					NumberOfCompletedPackets (void);
	void					ModeChange (void);
	void					ReturnLinkKeys (void);
	void					PINCodeRequest (void);
	void					LinkKeyRequest (void);
	void					LinkKeyNotification (void);
	void					LoopbackCommand (void);
	void					DataBufferOverflow (void);
	void					MaxSlotsChange (void);
	void					ReadClockOffsetComplete (void);
	void					ConnectionPacketTypeChanged (void);
	void					QoSViolation (void);
	void					PageScanModeChange (void);
	void					PageScanRepetitionModeChange (void);
};

#define HCIERR_UNKNOWN_COMMAND 0x01
#define HCIERR_NO_CONNECTION 0x02
#define HCIERR_HARDWARE_FAILURE 0x03
#define HCIERR_PAGE_TIMEOUT 0x04
#define HCIERR_AUTHENTICATION_FAILURE 0x05
#define HCIERR_KEY_MISSING 0x06
#define HCIERR_MEMORY_FULL 0x07
#define HCIERR_CONNECTION_TIMEOUT 0x08
#define HCIERR_MAX_NUMBER_OF_CONNECTIONS 0x09
#define HCIERR_MAX_NUMBER_OF_SCO_CONNECTIONS 0x0A
#define HCIERR_ACL_CONNECTION_ALREADY_EXISTS 0x0B
#define HCIERR_COMMAND_DISALLOWED 0x0C
#define HCIERR_REJECTED_LIMITED_RESOURCES 0x0D
#define HCIERR_REJECTED_SECURITY_REASONS 0x0E
#define HCIERR_REJECTED_PERSONAL_DEVICE 0x0F
#define HCIERR_HOST_TIMEOUT 0x10
#define HCIERR_UNSUPPORTED 0x11
#define HCIERR_INVALID_PARAMETERS 0x12
#define HCIERR_TERMINATED_USER 0x13
#define HCIERR_TERMINATED_LOW_RESOURCES 0x14
#define HCIERR_TERMINATED_POWER_OFF 0x15
#define HCIERR_TERMINATED_LOCAL_HOST 0x16
#define HCIERR_REPEATED_ATTEMPTS 0x17
#define HCIERR_UNKNOWN_LMP_PDU 0x19
#define HCIERR_UNSUPPORTED_REMOTE_FEATURE 0x1A
#define HCIERR_SCO_OFFSET_REJECTED 0x1B
#define HCIERR_SCO_INTERVAL_REJECTED 0x1C
#define HCIERR_SCO_AIR_MODE_REJECTED 0x1D
#define HCIERR_INVALID_LMP_PARAMETERS 0x1E
#define HCIERR_UNSPECIFIED 0x1F
#define HCIERR_UNSUPPORTED_LMP_PARAMETER_VALUE 0x20
#define HCIERR_ROLE_CHANGE_NOT_ALLOWED 0x21
#define HCIERR_LMP_RESPONSE_TIMEOUT 0x22
#define HCIERR_LMP_TRANSACTION_COLLISION 0x23
#define HCIERR_LMP_PDU_NOT_ALLOWED 0x24
#define HCIERR_ENCRYPTION_MODE_NOT_ACCEPTABLE 0x25
#define HCIERR_UNIT_KEY_USED 0x26
#define HCIERR_QOS_IS_NOT_SUPPORTED 0x27
#define HCIERR_INSTANT_PASSED 0x28
#define HCIERR_PAIRING_WITH_UNIT_KEY_NOT_SUPPORTED 0x29

#endif
