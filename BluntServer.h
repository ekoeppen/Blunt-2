#ifndef __BLUNTSERVER_H
#define __BLUNTSERVER_H

#include <UserTasks.h>
#include <SerialChipV2.h>
#include "Definitions.h"
#include "EventsCommands.h"
#include "CircleBuf.h"
#include "Logger.h"

class BluntServer;
class Handler;
class HCI;
class L2CAP;
class RFCOMM;
class SDP;

#define DRIVER_GENERIC		0
#define DRIVER_PICO			1
#define DRIVER_TAIYO_YUDEN	2
#define DRIVER_BT2000		3
#define DRIVER_BT2000E		4
#define DRIVER_950_7MHZ		5
#define DRIVER_950_14MHZ	6
#define DRIVER_950_18MHZ	7
#define DRIVER_950_32MHZ	8

enum {
	M_DATA,
	M_COMMAND,
	M_QUIT,
	M_TIMER
};

typedef enum {
	CONN_RESET,
	CONN_IDLE,
	CONN_BIND,
	CONN_CONNECT,
	CONN_DISCONNECT,
	CONN_LISTEN,				// 5
	CONN_ACCEPT,
	CONN_CONNECTED,
	CONN_CLOSE,
	CONN_PAIR_INCOMING,
	CONN_PAIR_OUTGOING,			// 10
	CONN_DISCOVER,
	CONN_SDP,
	CONN_REMOVE
} ConnectionState;

typedef enum {
	H_SERVER,
	H_HCI,
	H_L2CAP,
	H_SDP,
	H_RFCOMM
} HandlerType;

struct DiscoveredDevice {
	UByte fBdAddr[6];
	UByte fPageScanRepetitionMode;
	UByte fPageScanPeriodMode;
	UByte fPageScanMode;
	UByte fClass[3];
	UByte fName[249];
};

// ================================================================================
// ¥ TSerialChip16450
// ================================================================================

class TSerialChip16450
{
public:
	void WriteSerReg (ULong, UByte);
};

// ================================================================================
// ¥ TProtocolHander classes
// ================================================================================

class Handler
{
public:
	Long 					fLogLevel;
	BluntServer				*fServer;
	Handler					*fParentHandler;
	Handler					*fHandlers[MAX_HANDLERS];
	Long					fNumHandlers;
	TObjectId				fTool;
	HandlerType				fHandlerType;
	
	int						fState;
	ConnectionState         fConnectionState;
	
							Handler ();
							~Handler ();
	void					AddHandler (Handler *handler, BluntServer *server);
	void					RemoveHandler (Handler *handler);
	void					Print (int depth);
	void					SetLogLevel (Byte level[6]);
    void                    Log (Long logLevel, char* format, ...);
};

// ================================================================================
// ¥ BluntServer
// ================================================================================

class BluntServer: public TUTaskWorld
{
public:
    BluntServer*            fServer;

	Long 					fLogLevel;
	Byte					fDefaultLogLevel[5];

	TSerialChip* 			fChip;
	Logger*					fLogger;
	
	ULong 					fLastRead;
	ULong 					fRxStatus;
	ULong					fRxErrorStatus;
	ULong 					fRxBufFull;

	volatile ULong			fInputHead;
	ULong 					fInputTail;

	ULong 					fOutputHead;
	ULong 					fOutputTail;

	Boolean					fRxDMA;
	Boolean					fTxDMA;
	Boolean					fBufferOutput;
	
	TExtendedCircleBuf*		fRxDMABuffer;
	TExtendedCircleBuf*		fTxDMABuffer;
	UByte* 					fOutputBuffer;
	UByte* 					fInputBuffer;
	TUPort 					fPort;
	TUAsyncMessage 			fIntMessage;
	
	UByte					fMessage[MAX_MESSAGE];
	
	ULong					fOutstandingPackets;
	
	ULong					fDriver;
	ULong					fLocation;
	ULong					fSpeed;
	
	Handler					**fHandlers;
	Long					fNumHandlers;
	
	Long					fSemaphore;
	
	Long					fDebug;
	ULong					fBytesRead;
	
	// Device address
	
	UByte					fBdAddr[6];
	
	UByte					fName[65];
	
	// Discovery data
	
	DiscoveredDevice		*fDiscoveredDevices;
	Byte					fNumDiscoveredDevices;
	Byte					fCurrentDevice;
	
	// Services data
	
	ULong					*fQueriedServices;
	Byte					fCurrentService;
	Byte					fNumQueriedServices;

	ConnectionState         fConnectionState;

	NewtonErr				Initialize (ULong location, ULong driver, ULong speed, Long logLevel);
	
	void					AddHandler (Handler *handler);
	void					RemoveHandler (Handler *handler);
	void					DeferredRemoveHandler (Handler *handler);
	void					HandleData ();
	void					HandleCommand (BluntCommand* command);
	void					HandleTimer (BluntTimerEvent *event);
	void					SetupProtocolStack (BluntConnectionCommand* command);
	
	NewtonErr				PeekInputBuffer (UByte *data, int offset, int len);
	NewtonErr				ReadInputBuffer (UByte *data, int len);
	void					ConsumeInputBuffer (int len);
	ULong					InputBufferCount ();
	void					ResetInputBuffer ();
	
	Boolean					RcvBufferLevelCritical ();
	Boolean					RcvBufferLevelOk ();
	
	HCI*					SetupHCILayer (TObjectId port, UChar* addr, UByte psRepMode, UByte psMode, UByte* linkKey);
	L2CAP*					SetupL2CAPLayer (TObjectId port, Short protocol, HCI* hci);
	SDP*					SetupSDPLayer (TObjectId port, L2CAP* l2cap);
	RFCOMM*					SetupRFCOMMLayer (TObjectId port, UByte comPort, L2CAP* l2cap);

	void					InitiatePairing (BluntInitiatePairingCommand* command);
	void					InitiateServiceRequest (BluntServiceRequestCommand* command);
	void					SendData (BluntDataCommand* command);
    void                    Status ();
	
	virtual ULong 			GetSizeOf ();
	
	virtual long 			TaskConstructor ();
	virtual void 			TaskDestructor ();
	virtual void 			TaskMain ();
	
	void 					DriverReset (void);
	void 					DriverSendDelay (void);
	
	void 					TxBEmptyIntHandler (void);
	void 					ExtStsIntHandler (void);
	void 					RxCAvailIntHandler (void);
	void 					RxCSpecialIntHandler (void);
	void 					RxDMAIntHandler (RxErrorStatus);
	void 					TxDMAIntHandler ();
	
	void					SetPacketStart ();
	void					BufferOutput (Boolean buffer);
	void					StartOutput ();
	void 					Output (UByte*, ULong, Boolean packetStart = true);
	void 					SendEvent (BluntEvent* event);
	void					SetTimer (Handler *handler, int milliSecondDelay, void *userData = NULL);

	void					Print (void);
    void                    Log (Long logLevel, char* format, ...);
	void					LogInputBuffer (Long logLevel);
	
	static TObjectId 		Port (void);
};

#endif
