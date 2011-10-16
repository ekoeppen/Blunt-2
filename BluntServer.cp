#define DEFAULT_LOGLEVEL 0

#include <HALOptions.h>
#include <SerialChipRegistry.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "CircleBuf.h"
#include "BluntServer.h"
#include "HCI.h"
#include "L2CAP.h"
#include "RFCOMM.h"
#include "SDP.h"

#include "Logger.h"

#include <NewtonGestalt.h>

typedef long Ref;

const Ref	NILREF = 0x02;
const Ref	TRUEREF = 0x1A;
const Ref	FALSEREF = NILREF;	

class TObjectIterator;
class RefVar;
typedef const RefVar& RefArg;

typedef Ref (*MapSlotsFunction)(RefArg tag, RefArg value, ULong anything);

extern Ref	MakeInt(long i);
extern Ref	MakeChar(unsigned char c);
extern Ref	MakeBoolean(int val);

extern Boolean IsInt(RefArg r); 
extern Boolean IsChar(RefArg r);	
extern Boolean IsPtr(RefArg r);		
extern Boolean IsMagicPtr(RefArg r);	
extern Boolean IsRealPtr(RefArg r);	  
					
extern long RefToInt(RefArg r);			
extern UniChar RefToUniChar(RefArg r);		

#define kReceiveBufReg 3
#define kTransmitBufReg 3
#define kInterruptEnbReg 2
#define kInterruptIDReg 1
#define kLineControlReg 0
#define kModemControlReg 7
#define kLineStatusReg 6
#define kModemStatusReg 5
#define kScratchReg 4

#define DEBUGSTR
// #define DEBUGSTR DebugStr
// #define DEBUGSTR printf

#define EVT_BLUNT_CHECK_INPUT 0
#define EVT_BLUNT_HEARTBEAT 1

#define DMA_NOTIFY_LEVEL 129
#define MAX_OUTPUT 4096
#define MAX_INPUT 4096


extern "C" void EnterFIQAtomic (void);
extern "C" void ExitFIQAtomic (void);
extern "C" void _EnterFIQAtomic (void);
extern "C" void _ExitFIQAtomicFast (void);
extern TUPort* GetNewtTaskPort (void);
extern "C" long LockStack(TULockStack* lockRef, ULong additionalSpace); // Lock the entire stack plus additionalSpace # of bytes
extern "C" long UnlockStack(TULockStack* lockRef);
extern "C" long LockHeapRange(VAddr start, VAddr end, Boolean wire);	// Lock range from start up to but not including end.  Wire will prevent v->p mappings from changing.
extern "C" long UnlockHeapRange(VAddr start, VAddr end);
extern "C" _sys_write (int fd, char* data, int len);

// ================================================================================
// ¥ Interrupt Handlers
// ================================================================================

void TxBEmptyIntHandler (void* server)
{
	DEBUGSTR ("TxBEmptyIntHandler\n");
	((BluntServer*)server)->TxBEmptyIntHandler ();
}

void RxCAvailIntHandler (void* server)
{
	DEBUGSTR ("RxCAvailIntHandler\n");
	 ((BluntServer*)server)->RxCAvailIntHandler ();
}

void RxCSpecialIntHandler (void* server)
{
	DEBUGSTR ("RxCSpecialIntHandler\n");
	((BluntServer*)server)->RxCSpecialIntHandler ();
}

void RxDmaIntHandler (void* server, RxErrorStatus status)
{
	DEBUGSTR ("RxDmaIntHandler\n");
	 ((BluntServer*)server)->RxDMAIntHandler (status);
}

void TxDmaIntHandler (void* server)
{
	DEBUGSTR ("TxDmaIntHandler\n");
	 ((BluntServer*)server)->TxDMAIntHandler ();
}

void ExtStsIntHandler (void* server)
{
	DEBUGSTR ("ExtStsIntHandler\n");
//	 ((BluntServer*)server)->ExtStsIntHandler ();
}

void __EndOfInterruptHandlers ()
{
}

#pragma mark -

// ================================================================================
// ¥ TExtendedCircleBuf Functions
// ================================================================================

UByte TExtendedCircleBuf::PeekByte (ULong offset)
{
	return fBuffer[(fStart + offset) % fSize];
}

#pragma mark -

// ================================================================================
// ¥ Driver Specific Functions
// ================================================================================

class TGPIOInterface
{
public:
	NewtonErr ReadGPIOData (UByte, ULong *);
};

class TBIOInterface
{
public:
	NewtonErr ReadDIOPins (UByte pin, ULong *data);
	NewtonErr WriteDIODir (UByte pin, UByte dir, UByte *data);

	NewtonErr WriteDIOPins (UByte pin, UByte value, UByte *data);
};

class TVoyagerPlatform
{
public:
	char filler_0000[0x0010];
	
	TGPIOInterface *fGPIOInterface;
	TBIOInterface *fBIOInterface;
	
	char filler_0014[0x00f0];
};

extern TVoyagerPlatform *GetPlatformDriver (void);

static UByte SetChannel3Selector (int value)
{
	TVoyagerPlatform *p;
	UByte data;
	
	p = GetPlatformDriver ();
	p->fBIOInterface->WriteDIOPins (0x22, value, &data);
	return data;
}

void BluntServer::DriverReset (void)
{
	TSerialChip16450 *chip;
	Byte prescale;
	
	HLOG (1, "BluntServer::DriverReset (%d)\n", fDriver);
	chip = (TSerialChip16450 *) fChip;
	* (Long *) ((Byte*) (fChip) + 0x10) = 0;
	HLOG (1, "	Features: %08x\n", fChip->GetFeatures ());
	fRxDMA = false;
	fRxDMA = false;
	if (fDriver == DRIVER_BT2000 || fDriver == DRIVER_PICO) {
		// Enable Enhanced Mode to get larger FIFO buffers
		chip->WriteSerReg (kLineControlReg, 0x83);
		chip->WriteSerReg (kInterruptIDReg, 0x21);
		chip->WriteSerReg (kLineControlReg, 0x03);
	} else if (fDriver == DRIVER_BT2000E ||
		fDriver == DRIVER_950_7MHZ ||
		fDriver == DRIVER_950_14MHZ ||
		fDriver == DRIVER_950_18MHZ ||
		fDriver == DRIVER_950_32MHZ) {
		// Enable '950 Mode
		chip->WriteSerReg (kLineControlReg, 0xbf);
		chip->WriteSerReg (kInterruptIDReg, 0x10);
		chip->WriteSerReg (kLineControlReg, 0x03);

		// Enable Enhanced Mode to get larger FIFO buffers
		chip->WriteSerReg (kLineControlReg, 0x83);
		chip->WriteSerReg (kInterruptIDReg, 0x21);
		chip->WriteSerReg (kLineControlReg, 0x03);

		// Enable the prescaler
		chip->WriteSerReg (kModemControlReg, 0x80);
		chip->WriteSerReg (kScratchReg, 0x01);
		chip->WriteSerReg (kLineStatusReg, 0x20);

		// Set the prescale value
		if (fDriver == DRIVER_BT2000E || fDriver == DRIVER_950_7MHZ) {
			prescale = 0x20;
		} else if (fDriver == DRIVER_950_14MHZ) {
			prescale = 0x40;
		} else if (fDriver == DRIVER_950_18MHZ) {
			prescale = 0x50;
		} else if (fDriver == DRIVER_950_32MHZ) {
			prescale = 0x8b;
		}
		chip->WriteSerReg (kScratchReg, 0x01);
		chip->WriteSerReg (kLineStatusReg, prescale);
	} else if (fDriver == DRIVER_TAIYO_YUDEN) {
		HLOG (1, "Resetting module\n");
		SetChannel3Selector (1);
		Wait (100);
		SetChannel3Selector (0);
		HLOG (1, "Resetting module done.\n");
		fRxDMA = false;
		fTxDMA = false;
	}
	Wait (100);
}

void BluntServer::DriverSendDelay (void)
{
	switch (fDriver) {
		case DRIVER_TAIYO_YUDEN:
			Wait (1);
			break;
		default:
			break;
	}
}

#pragma mark -

// ================================================================================
// ¥ Generic Protocol Handler Specific Functions
// ================================================================================

Handler::Handler ()
{
	fNumHandlers = 0;
	fServer = NULL;
	fParentHandler = NULL;
	fLogLevel = 0;
}

Handler::~Handler ()
{
}

void Handler::AddHandler (Handler *handler, BluntServer *server)
{
	if (fNumHandlers < MAX_HANDLERS) {
		fHandlers[fNumHandlers] = handler;
		handler->fServer = server;
		handler->fParentHandler = this;
		if (server->fDefaultLogLevel[handler->fHandlerType] != -1) {
			handler->fLogLevel = server->fDefaultLogLevel[handler->fHandlerType];
		}
		fNumHandlers++;
	} else {
		HLOG (0, "**** %08x number of handlers exceeded ****\n", this);
	}
}

void Handler::RemoveHandler (Handler *handler)
{
	int i;
	
	for (i = 0; i < fNumHandlers; i++) {
		if (fHandlers[i] == handler) {
			memmove (&fHandlers[i], &fHandlers[i + 1],
				sizeof (Handler *) + fNumHandlers - 1);
			fNumHandlers--;
			break;
		}
	}
}

void Handler::HCIClearToSend (Boolean isClear)
{
	int i;
	
	for (i = 0; i < fNumHandlers; i++) {
		fHandlers[i]->HCIClearToSend (isClear);
	}
}

void Handler::Print (int depth)
{
	int i, j;
	char buffer[80];
	
	for (j = 0; j < depth; j++) buffer[j] = ' ';
	buffer[j] = 0;
	if (fHandlerType == H_HCI) HLOG (0, "%sHCI (%04x)", buffer, ((HCI *) this)->fConnectionHandle);
	else if (fHandlerType == H_L2CAP) HLOG (0, "%sL2CAP", buffer);
	else if (fHandlerType == H_SDP) HLOG (0, "%sSDP", buffer);
	else if (fHandlerType == H_RFCOMM) HLOG (0, "%sRFCOMM", buffer);
	HLOG (0, " (%08x) states: %d %d\n", this, fState, fConnectionState);
	for (i = 0; i < fNumHandlers; i++) {
		fHandlers[i]->Print (depth + 4);
	}
}

void Handler::SetLogLevel (Byte level[5])
{
	int i;
	
	HLOG (0, "Handler::SetLogLevel %08x %d\n", this, level[fHandlerType]);
	fLogLevel = level[fHandlerType];
	for (i = 0; i < fNumHandlers; i++) {
		fHandlers[i]->SetLogLevel (level);
	}
}

void Handler::Log (Long logLevel, char *format, ...)
{
	va_list args;
	char buffer[128];
	char *c;
	
	if (fLogLevel >= logLevel && fServer && fServer->fLogger) {
		va_start (args, format);
		vsprintf (buffer, format, args);
		va_end (args);
		fServer->fLogger->Output ((UByte*) buffer, strlen (buffer));
	}
}

#pragma mark -

// ================================================================================
// ¥ Server Functions
// ================================================================================

NewtonErr BluntServer::Initialize (ULong location, ULong driver, ULong speed, Long logLevel)
{
	PSerialChipRegistry* reg;
	SerialChipID id;
	NewtonErr r = noErr;
	int i;

	fServer = this;
	fLogger = new Logger ();
	fLogger->Initialize ();
	fLogger->Main ();
	if (logLevel != -1) {
		fLogLevel = logLevel;
	} else {
		fLogLevel = DEFAULT_LOGLEVEL;
	}
	for (i = 0; i < 5; i++) fDefaultLogLevel[i] = logLevel;
	fDebug = -1;
	fBytesRead = 0;
	fBufferOutput = false;
	HLOG (0, "BluntServer::Initialize\n");
	fLocation = location;
	fDriver = driver;
	fSpeed = speed;
	fChip = NULL;
	fSemaphore = 0;
	reg = GetSerialChipRegistry ();
	id = reg->FindByLocation (fLocation);
	fChip = reg->GetChipPtr (id);
	HLOG (1, "Driver: %d, Speed: %d Chip: %d (%08x)\n", fDriver, fSpeed, id, fChip);
	if (fChip == NULL) {
		HLOG (0, "No chip found at this location!\n");
		r = kErrNoHW;
	} else {
		* (Long *) ((Byte*) (fChip) + 0x10) = 0;
	}
	return r;
}

ULong BluntServer::GetSizeOf ()
{
	return sizeof (BluntServer);
}

void BluntServer::TxBEmptyIntHandler (void)
{
	ULong n, size;
	int i;
	UByte c;
	SerialStatus status;
	
	fSemaphore = 1;
	EnterFIQAtomic ();
	if (fTxDMA) {
		if (fTxDMABuffer->BufferCount () > 0) {
			fChip->TxDMAControl (kDMAStart);
		} else {
			fChip->ResetTxBEmpty ();
		}
	} else {
		fChip->SetTxDTransceiverEnable (true);
		fChip->ResetTxBEmpty ();
	}
	ExitFIQAtomic ();
	fSemaphore = 0;
}

void BluntServer::ExtStsIntHandler (void)
{
}

void BluntServer::RxCAvailIntHandler (void)
{
	if (fRxDMA) {
		fChip->RxDMAControl (kDMANotifyOnNext | kDMASync);
		SendForInterrupt (fPort, fIntMessage.GetMsgId (), 0, NULL, 0, M_DATA);
	} else {
		while (fChip->RxBufFull()) {
			fInputBuffer[fInputHead] = fChip->GetByte ();
			fInputHead = (fInputHead + 1) % MAX_INPUT;
		}
	}
	SendForInterrupt (fPort, fIntMessage.GetMsgId (), 0, NULL, 0, M_DATA);
}

void BluntServer::RxCSpecialIntHandler (void)
{
	if (fRxDMA) {
	} else {
		while (fChip->RxBufFull()) {
			fInputBuffer[fInputHead] = fChip->GetByte ();
			fInputHead = (fInputHead + 1) % MAX_INPUT;
		}
	}
	SendForInterrupt (fPort, fIntMessage.GetMsgId (), 0, NULL, 0, M_DATA);
}

void BluntServer::RxDMAIntHandler (RxErrorStatus status)
{
	ULong n;
	
	fSemaphore = 1;
	fRxErrorStatus = status;
	fChip->RxDMAControl (kDMANotifyOnNext | kDMASync);
	fSemaphore = 0;
	SendForInterrupt (fPort, fIntMessage.GetMsgId (), 0, NULL, 0, M_DATA);
}

void BluntServer::TxDMAIntHandler ()
{
}

long BluntServer::TaskConstructor ()
{
	TUNameServer nameServer;
	TCMOSerialIOParms parms;
	SCCChannelInts handlers;
	NewtonErr r;
	ULong n;
	HCI* hci;
	TULockStack lock;
	
	HLOG (0, "BluntServer::TaskConstructor\n");

	fOutputHead = 0;
	fOutputTail = 0;
	fInputHead = 0;
	fInputTail = 0;
	fRxDMA = false;
	fTxDMA = false;

	LockHeapRange((VAddr) &::TxBEmptyIntHandler, (VAddr) &::__EndOfInterruptHandlers, true);
	LockHeapRange((VAddr)this, (VAddr)((char*)this+sizeof(*this)), false);

	HLOG (1, "Server: %08x Debug: %08x Buffer: %08x\n", this, &fDebug, fRxDMABuffer);

	if (!fChip->PowerIsOn ()) fChip->PowerOn ();
	HLOG (1, "Power: %d\n", fChip->PowerIsOn ());

	DriverReset ();
	if (fRxDMA) {
		fRxDMABuffer = new TExtendedCircleBuf ();
		fRxDMABuffer->Allocate (512, 8, 2, 0);
	} else {
		fInputBuffer = new UByte[MAX_INPUT];
	}
	
	if (fTxDMA) {
		fTxDMABuffer = new TExtendedCircleBuf ();
		fTxDMABuffer->Allocate (512, 8, 2, 0);
	} else {
		fOutputBuffer = new UByte[MAX_OUTPUT];
	}

	fChip->SetSerialMode (kSerModeAsync);
	fChip->SetInterruptEnable (false);

	LockStack (&lock, 64);
	_EnterFIQAtomic ();

	handlers.TxBEmptyIntHandler = ::TxBEmptyIntHandler;
	handlers.ExtStsIntHandler = ::ExtStsIntHandler;
	handlers.RxCAvailIntHandler = ::RxCAvailIntHandler;
	handlers.RxCSpecialIntHandler = ::RxCSpecialIntHandler;
	
	r = fChip->InstallChipHandler (this, &handlers);
	parms.fStopBits = k1StopBits;
	parms.fParity = kNoParity;
	parms.fDataBits = k8DataBits;
	parms.fSpeed = fSpeed;
	fChip->SetIOParms (&parms);
	fChip->SetSpeed (fSpeed);
	fChip->Reconfigure ();
	fChip->SetInterruptEnable (true);
	fChip->SetTxDTransceiverEnable (true);
	_ExitFIQAtomicFast ();
	UnlockStack (&lock);

	if (fRxDMA) {
		fRxDMABuffer->DMABufInfo (&n, NULL, NULL, NULL);
		fChip->InitRxDMA (fRxDMABuffer, DMA_NOTIFY_LEVEL, RxDmaIntHandler);
		fChip->RxDMAControl (kDMANotifyOnNext | kDMAStart);
	}
	if (fTxDMA) {
		fTxDMABuffer->DMABufInfo (&n, NULL, NULL, NULL);
		fChip->InitTxDMA (fTxDMABuffer, TxDmaIntHandler);
		fChip->TxDMAControl (kDMANotifyOnNext | kDMAStart);
	}
	HLOG (1, "Chip configured, DMA: %d %d\n", fRxDMA, fTxDMA);
	
	fNumHandlers = 0;
	fHandlers = NULL;
	hci = new HCI ();
	hci->fConnectionHandle = -1;
	AddHandler ((Handler *) hci);
	
	fPort.Init ();
	fIntMessage.Init (false);
	HLOG (1, "Server port: %08x\n", fPort.fId);
	nameServer.RegisterName ("BluntServer", "TUPort", fPort.fId, 0);
	
	fOutstandingPackets = 0;
	
	// SetTimer (nil, 30000, (void *) EVT_BLUNT_HEARTBEAT);

	return noErr;
}

void BluntServer::TaskDestructor ()
{
	TUNameServer nameServer;

	HLOG (0, "BluntServer::TaskDestructor\n");
	fChip->SetInterruptEnable (false);
	fChip->RemoveChipHandler (this);
	fChip->SetTxDTransceiverEnable (false);
	fChip->PowerOff ();
	nameServer.UnRegisterName ("BluntServer", "TUPort");
	UnlockHeapRange((VAddr) &::TxBEmptyIntHandler, (VAddr) &::__EndOfInterruptHandlers);
	UnlockHeapRange((VAddr)this, (VAddr)((char*)this+sizeof(*this)));
	* (Long *) ((Byte*) (fChip) + 0x10) = 0;
	LogInputBuffer (0);

	if (fRxDMA) {
		delete fRxDMABuffer;
	} else {
		delete[] fInputBuffer;
	}
	if (fTxDMA) {
		delete fTxDMABuffer;
	} else {
		delete[] fOutputBuffer;
	}
	HLOG (0, "Done\n");
	Sleep (3000 * kMilliseconds);
	fLogger->Close ();
	Sleep (100 * kMilliseconds);
	delete fLogger;
	Sleep (100 * kMilliseconds);
}

void BluntServer::TaskMain ()
{
	ULong type = 0;
	Boolean end = false;
	TUMsgToken token;
	ULong n;
	
	HLOG (1, "BluntServer::TaskMain\n");
	while (!end) {
		n = MAX_MESSAGE;
		fPort.Receive (&n, fMessage, MAX_MESSAGE, &token, &type);
		HLOG (1, "--> Got the message %d (%08x): %d bytes (%08x) \n", type, fMessage, n, &token);
		switch (type) {
			case M_DATA:
				HandleData ();
				break;
			case M_COMMAND:
				HandleCommand ((BluntCommand *) fMessage);
				break;
			case M_TIMER:
				HandleTimer ((BluntTimerEvent*) fMessage);
//				delete (BluntTimerEvent*) fMessage;
				break;
			case M_QUIT:
				end = true;
				break;
			default:
				break;
		}
	}
}

void BluntServer::StartOutput ()
{
	UByte c;
	ULong n;
	
	fBufferOutput = false;
	n = 0;
	if (fTxDMA) {
		HLOG (1, "BluntServer::StartOutput (%d)\n", fTxDMABuffer->BufferCount ());
		fChip->SetTxDTransceiverEnable (true);
		fChip->TxDMAControl (kDMANotifyOnNext | kDMAStart);
		HLOG (1, " Sent %d bytes (%d left)\n", n, fTxDMABuffer->BufferCount ());
	} else {
		HLOG (1, "BluntServer::StartOutput (%d %d)\n", fOutputHead, fOutputTail);
		fChip->SetTxDTransceiverEnable (true);
		while (fOutputHead != fOutputTail) {
			while (!(fChip->TxBufEmpty () &&
				fChip->GetSerialStatus () & (kSerialCTSAsserted | kSerialDSRAsserted)))
				;
			n++;

			c = fOutputBuffer[fOutputTail];
			fOutputTail = (fOutputTail + 1) % MAX_OUTPUT;
			fChip->PutByte (c);
		}

		HLOG (1, " Sent %d bytes (%d %d left)\n", n, fOutputHead, fOutputTail);
	}
}

void BluntServer::Output (UByte* data, ULong size, Boolean packetStart)
{
	ULong n, processedBytes;
	int i;
	
	HLOG (2, "BluntServer::Output (%d): %08x %d\n", fBufferOutput, data, size);
	
	i = 0;
	while (i < size && i < 96) {
		HLOG (3, "%02x ", data[i]);
		i++;
		if ((i + 1) % 16 == 0) HLOG (3, "\n");
	}
	HLOG (3, "\n");
	
	if (packetStart) {
		fOutstandingPackets++;
	}
	
	if (fTxDMA) {
		n = size;
		fTxDMABuffer->CopyIn (data, &n, false, 0);
		processedBytes = size - n;
		HLOG (2, " Buffered %d of %d bytes (%d free)\n", processedBytes, size, fTxDMABuffer->BufferSpace ());
	} else {
		for (i = 0; i < size; i++) {
			fOutputBuffer[fOutputHead] = data[i];
			fOutputHead = (fOutputHead + 1) % MAX_OUTPUT;
		}
	}
	if (!fBufferOutput) {
		StartOutput ();
	}
}

void BluntServer::HandleData ()
{
	int i;
	Long lastAmount;
	ULong tail;
	ULong n;
	Boolean buffering;
	
	if (fRxDMA) {
		HLOG (1, "BluntServer::HandleData %08x %d\n", fRxDMABuffer, fRxDMABuffer->BufferCount ());
		if (fRxErrorStatus != 0) HLOG (0, "*** Circle buffer overflow\n");
		lastAmount = -1;
		do {
			lastAmount = fRxDMABuffer->BufferCount ();
			for (i = 0; fRxDMABuffer->BufferCount () > 0 && i < fNumHandlers; i++) {
				HLOG (2, "BluntServer Handler: %d (%08x), data: %d bytes\n", i, fHandlers[i], fRxDMABuffer->BufferCount ());
				if (fHandlers[i]->fConnectionState != CONN_REMOVE) {
					((HCI *) fHandlers[i])->HandleData (fRxDMABuffer);
				}
			}
//			if (fRxDMABuffer->BufferCount () > 0 && lastAmount == fRxDMABuffer->BufferCount ()) {
//			((HCI *) fHandlers[0])->RemoveUnhandledPackets (fInputBuffer, fInputCurrent);
//			}
		} while (fRxDMABuffer->BufferCount () > 0 && lastAmount != fRxDMABuffer->BufferCount ());
	} else {
		HLOG (1, "BluntServer::HandleData %d %d\n", fInputTail, fInputHead);
		buffering = fBufferOutput;
		fBufferOutput = true;
		do {
			tail = fInputTail;
			LogInputBuffer (2);
			for (i = 0; i < fNumHandlers; i++) {
				HLOG (2, "BluntServer Handler: %d (%08x), data: %d bytes\n", i, fHandlers[i], lastAmount);
				if (fHandlers[i]->fConnectionState != CONN_REMOVE) {
					LogInputBuffer (2);
					((HCI *) fHandlers[i])->HandleData ();
				}
			}
		} while (InputBufferCount () > 0 && tail != fInputTail);
		HLOG (1, "	Done %d %d\n", fInputTail, fInputHead);
		fBufferOutput = buffering;
		if (!fBufferOutput && fOutputHead != fOutputTail) {
			StartOutput ();
		}
	}

	i = 0;
	while (i < fNumHandlers) {
		if (fHandlers[i]->fConnectionState == CONN_REMOVE) {
			RemoveHandler (fHandlers[i]);
		} else {
			i++;
		}
	}
}

void BluntServer::HandleCommand (BluntCommand* command)
{
	HCI* hci;
	int i;

	if (command->fType != C_LOG) {
		HLOG (1, "BluntServer::HandleCommand %d\n", command->fType);
	}
	
	switch (command->fType) {
		case C_RESET: {
			BluntResetCommand* c = (BluntResetCommand*) command;
			HLOG (1, "	Reset: %s\n", c->fName);
			strcpy (((HCI *) fHandlers[0])->fName, c->fName);
			((HCI *) fHandlers[0])->Reset ();
			} break;
		case C_CONNECT: {
			BluntConnectionCommand* c = (BluntConnectionCommand*) command;
			HLOG (1, "	Connect: %d\n", c->fTargetLayer);
			SetupProtocolStack (c);
			} break;
		case C_DISCONNECT: {
			BluntDisconnectCommand* c = (BluntDisconnectCommand*) command;
			Boolean b = false;
			HLOG (1, "	Disconnect: %04x", c->fHCIHandle);
			for (i = 1; i < fNumHandlers; i++) {
				hci = (HCI *) fHandlers[i];
				if (hci->fHandlerType == H_HCI &&
					memcmp (c->fBdAddr, hci->fPeerAddress, 6) == 0) {
					hci->Disconnect (0x13);
					b = true;
				}
			}
			if (!b) {
				TUPort p (c->fToolPort);
				BluntDisconnectCompleteEvent* e = new BluntDisconnectCompleteEvent (noErr);
				p.Send ((TUAsyncMessage *) e, e, sizeof (*e), kNoTimeout, nil, BLUNT_MSG_TYPE);
			}
			} break;
		case C_INQUIRY: {
			BluntInquiryCommand* c = (BluntInquiryCommand*) command;
			HLOG (1, "	Inquiry %d %d\n", c->fTime, c->fAmount);
			((HCI *) fHandlers[0])->Inquiry (c->fTime, c->fAmount);
			} break;
		case C_INQUIRY_CANCEL: {
			HLOG (1, "	Cancel inquiry\n");
			((HCI *) fHandlers[0])->InquiryCancel ();
			} break;
		case C_NAME_REQUEST: {
			BluntNameRequestCommand* c = (BluntNameRequestCommand*) command;
			HLOG (1, "	Name request\n");
			((HCI *) fHandlers[0])->RemoteNameRequest (c->fBdAddr, c->fPSRepMode, c->fPSMode);
			} break;
		case C_INIT_PAIR:
			HLOG (1, "	Pair\n");
			InitiatePairing ((BluntInitiatePairingCommand*) command);
			break;
		case C_SERVICE_REQUEST:
			HLOG (1, "	Service request\n");
			InitiateServiceRequest ((BluntServiceRequestCommand*) command);
			break;
		case C_SET_LOG_LEVEL: {
			BluntSetLogLevelCommand* c = (BluntSetLogLevelCommand*) command;
			fLogLevel = c->fLevel[0];
			HLOG (1, "	Set log level %d\n", c->fLevel[0]);
			for (i = 0; i < fNumHandlers; i++) {
				fHandlers[i]->SetLogLevel (c->fLevel);
			}
			for (i = 0; i < 5; i++) fDefaultLogLevel[i] = c->fLevel[i];
			} break;
		case C_DATA:
			HLOG (1, "	Send data\n");
			SendData ((BluntDataCommand*) command);
			break;
		case C_STATUS:
			HLOG (1, "	Status\n");
			Status ();
			break;
		case C_LOG:
			if (fLogger) fLogger->Output (((BluntLogCommand*) command)->fData, ((BluntLogCommand*) command)->fSize);
			break;
	}
	if (command->fDelete) delete command->fOriginalCommand;
}

void BluntServer::HandleTimer (BluntTimerEvent *event)
{
	Handler *handler = event->fHandler;
	HLOG (1, "BluntServer::HandleTimer %08x (%08x %d)\n", handler, event->fUserData, event->fResult);
	if (handler) {
		switch (handler->fHandlerType) {
			case H_HCI:
				((HCI *) handler)->HandleTimer (event->fUserData);
				break;
			case H_L2CAP:
				((L2CAP *) handler)->HandleTimer (event->fUserData);
				break;
			case H_SDP:
				((SDP *) handler)->HandleTimer (event->fUserData);
				break;
			case H_RFCOMM:
				((RFCOMM *) handler)->HandleTimer (event->fUserData);
				break;
		}
	} else {
		switch ((int) event->fUserData) {
			case EVT_BLUNT_CHECK_INPUT:
				break;
			case EVT_BLUNT_HEARTBEAT:
				HLOG (0, "*** Heartbeat\n");
				if (fNumHandlers > 0) {
					((HCI *) fHandlers[0])->Status ();
				}
				SetTimer (nil, 30000, (void *) EVT_BLUNT_HEARTBEAT);
				break;
		}
	}
	delete event->fOriginalEvent;
}

void BluntServer::SetupProtocolStack (BluntConnectionCommand* command)
{
	HCI* hci;
	L2CAP* l2cap;
	RFCOMM* rfcomm;
	SDP* sdp;
	
	HLOG (1, "BluntServer::SetupProtocolStack\n");
	fNumHandlers = 1;

	hci = SetupHCILayer (command->fToolPort, command->fBdAddr, command->fPSRepMode, command->fPSMode, command->fLinkKey);
	switch (command->fTargetLayer) {
		case L_HCI:
			hci->Connect ();
			break;
		case L_L2CAP:
			l2cap = SetupL2CAPLayer (command->fToolPort, command->fL2CAPProtocol, hci);
			l2cap->Connect ();
			break;
		case L_RFCOMM:
			l2cap = SetupL2CAPLayer (command->fToolPort, PSM_RFCOMM, hci);
			rfcomm = SetupRFCOMMLayer (command->fToolPort, command->fRFCOMMPort, l2cap);
			rfcomm->Connect ();
			break;
		case L_SDP:
			l2cap = SetupL2CAPLayer (command->fToolPort, PSM_SDP, hci);
			sdp = SetupSDPLayer (command->fToolPort, l2cap);
			sdp->Connect ();
			break;
	}
	
	Print ();
}

HCI *BluntServer::SetupHCILayer (TObjectId port, UChar* addr, UByte psRepMode, UByte psMode, UByte* linkKey)
{
	HCI* hci;
	int i;

	HLOG (1, "BluntServer::SetupHCILayer\n");
	for (hci = NULL, i = 1; hci == NULL && i < fNumHandlers; i++) {
		if (memcmp (((HCI *) fHandlers[i])->fPeerAddress, addr, 6) == 0) hci = (HCI *) fHandlers[i];
	}
	if (hci == NULL) {
		hci = new HCI ();
		HLOG (1, "	HCI: %08x\n", hci);
		hci->fConnectionHandle = -1;
		hci->fTool = port;
		hci->fConnectionState = CONN_CONNECT;
		memcpy (hci->fPeerAddress, addr, 6);
		hci->fControlChannel = (HCI *) fHandlers[0];
		hci->fPSRepMode = psRepMode;
		hci->fPSMode = psMode;
		hci->fHCIWindowSize = hci->fControlChannel->fHCIWindowSize;
		if (linkKey != NULL) {
			memcpy (hci->fLinkKey, linkKey, sizeof (hci->fLinkKey));
		}
		AddHandler (hci);
	}
	
	return hci;
}

L2CAP* BluntServer::SetupL2CAPLayer (TObjectId port, Short protocol, HCI* hci)
{
	L2CAP* controlL2cap;
	L2CAP* l2cap;
	int i;

	HLOG (1, "BluntServer::SetupL2CAPLayer\n");
	
	if (hci->fNumHandlers == 0) {
		controlL2cap = new L2CAP ();
		controlL2cap->fLocalCID = CID_SIGNAL;
		controlL2cap->fRemoteCID = CID_SIGNAL;
		hci->AddHandler ((Handler *) controlL2cap, this);
	} else {
		controlL2cap = (L2CAP *) hci->fHandlers[0];
	}
	controlL2cap->fTool = port;
	controlL2cap->fTargetProtocol = protocol;
	HLOG (1, " L2CAP control: %08x\n", controlL2cap);
	
	for (l2cap = NULL, i = 1; l2cap == NULL && i < hci->fNumHandlers; i++) {
		if (((L2CAP *) hci->fHandlers[i])->fProtocol == protocol) l2cap = (L2CAP *) hci->fHandlers[i];
	}
	if (l2cap == NULL) {
		l2cap = new L2CAP ();
		l2cap->fLocalCID = controlL2cap->GetNewCID ();
		l2cap->fProtocol = protocol;
		l2cap->fConnectionState = CONN_CONNECT;
		hci->AddHandler ((Handler *) l2cap, this);
	}
	l2cap->fTool = port;
	HLOG (1, " L2CAP data: %08x S: %d Prot: %d\n", l2cap, l2cap->fLocalCID, l2cap->fProtocol);
	
	return l2cap;
}

SDP* BluntServer::SetupSDPLayer (TObjectId port, L2CAP* l2cap)
{
	SDP* sdp;
	int i;
	
	HLOG (1, "BluntServer::SetupSDPLayer\n");
	for (sdp = NULL, i = 0; sdp == NULL && i < l2cap->fNumHandlers; i++) {
		if (l2cap->fHandlers[i]->fHandlerType == H_SDP) sdp = (SDP *) l2cap->fHandlers[i];
	}
	if (sdp == NULL) {
		sdp = new SDP ();
		sdp->fConnectionState = CONN_CONNECT;
		l2cap->AddHandler ((Handler *) sdp, this);
	}
	sdp->fTool = port;
	return sdp;
}

RFCOMM* BluntServer::SetupRFCOMMLayer (TObjectId port, UByte comPort, L2CAP* l2cap)
{
	RFCOMM* rfcomm;
	RFCOMM* controlRfcomm;
	
	HLOG (1, "BluntServer::SetupRFCOMMLayer %08x\n", port);
	if (l2cap->fNumHandlers == 0) {
		controlRfcomm = new RFCOMM ();
		controlRfcomm->fTool = port;
		controlRfcomm->fDLCI = 0;
		controlRfcomm->fPort = 0;
		controlRfcomm->fConnectionState = CONN_CONNECT;
		l2cap->AddHandler ((Handler *) controlRfcomm, this);
	} else {
		controlRfcomm = (RFCOMM *) l2cap->fHandlers[0];
	}
	HLOG (1, " RFCOMM control: %08x\n", controlRfcomm);
	rfcomm = new RFCOMM ();
	rfcomm->fPort = comPort;
	rfcomm->fDLCI = comPort << 1;
	rfcomm->fTool = port;
	rfcomm->fConnectionState = CONN_CONNECT;
	l2cap->AddHandler ((Handler *) rfcomm, this);
	HLOG (1, " RFCOMM data: %08x Port: %d\n", rfcomm, rfcomm->fPort);

	return rfcomm;
}

Boolean BluntServer::RcvBufferLevelCritical ()
{
	if (InputBufferCount () > MAX_INPUT / 2) return true;
	else return false;
}

Boolean BluntServer::RcvBufferLevelOk () {
	if (InputBufferCount () < 512) return true;
	else return false;
}

void BluntServer::InitiatePairing (BluntInitiatePairingCommand* command)
{
	HCI* hci;

	HLOG (1, "BluntServer::InitiatePairing\n");
	
	hci = SetupHCILayer (0, command->fBdAddr, command->fPSRepMode, command->fPSMode, NULL);
	hci->Pair (command->fBdAddr, command->fPIN);
}

void BluntServer::InitiateServiceRequest (BluntServiceRequestCommand* command)
{
	HCI* hci;
	L2CAP* l2cap;
	SDP* sdp;

	HLOG (1, "BluntServer::InitiateServiceRequest\n");
	hci = SetupHCILayer (0, command->fBdAddr, command->fPSRepMode, command->fPSMode, command->fLinkKey);
	l2cap = SetupL2CAPLayer (0, PSM_SDP, hci);
	sdp = SetupSDPLayer (0, l2cap);
	Print ();
	sdp->Connect ();
}

void BluntServer::SendData (BluntDataCommand* command)
{
	HLOG (1, "BluntServer::SendData %08x %d %d\n", command, command->fHandler->fHandlerType, command->fData->GetSize ());
	if (command->fHandler->fHandlerType == H_RFCOMM) {
		((RFCOMM *) command->fHandler)->SendData (command->fData);
	} else {
		TUPort p (command->fHandler->fTool);
		BluntDataSentEvent *e;
		e = new BluntDataSentEvent (kErrNoPeer, 0);
		p.Send ((TUAsyncMessage *) e, e, sizeof (BluntDataSentEvent), kNoTimeout, nil, BLUNT_MSG_TYPE);
	}
}

void BluntServer::BufferOutput (Boolean buffer)
{
	HLOG (2, "BluntServer::BufferOutput\n");
	fBufferOutput = buffer;
}

void BluntServer::SendEvent (BluntEvent* event)
{
	TUPort* port;
	
	HLOG (1, "BluntServer::SendEvent %08x\n", event);
	port = GetNewtTaskPort ();
	port->Send (event, (void *) event, event->GetSizeOf ());
}

void BluntServer::SetTimer (Handler *handler, int milliSecondDelay, void *userData)
{
	BluntTimerEvent* timer = new BluntTimerEvent (kResultOk, handler, userData);
	TTime delay;
	NewtonErr r;

	HLOG (1, "BluntServer::SetTimer %08x (%d) %d\n", handler, handler != NULL ? handler->fHandlerType : -1, milliSecondDelay);
	delay = GetGlobalTime () + TTime (milliSecondDelay, kMilliseconds);
	r = fPort.Send (timer, (void *) timer, sizeof (*timer), kNoTimeout, &delay, M_TIMER);
	HLOG (1, "  Result: %d\n", r);
}

TObjectId BluntServer::Port ()
{
	TUNameServer nameServer;
	ULong id;
	ULong spec;

	nameServer.Lookup ("BluntServer", "TUPort", &id, &spec);
	return id;
}

void BluntServer::AddHandler (Handler *handler)
{
	HLOG (1, "BluntServer::AddHandler: %08x\n", handler);
	handler->fServer = this;
	fHandlers = (Handler**) realloc (fHandlers, (fNumHandlers + 1) *
		sizeof (Handler *));
	if (fDefaultLogLevel[handler->fHandlerType] != -1) {
		handler->fLogLevel = fDefaultLogLevel[handler->fHandlerType];
	}
	fHandlers[fNumHandlers] = handler;
	fNumHandlers++;
}

void BluntServer::RemoveHandler (Handler *handler)
{
	int i;
	
	HLOG (1, "BluntServer::RemoveHandler %08x\n", handler);
	for (i = 0; i < fNumHandlers; i++) {
		if (fHandlers[i] == handler) {
			memmove (&fHandlers[i], &fHandlers[i + 1],
				sizeof (Handler *) + fNumHandlers - 1);
			fNumHandlers--;
			if (handler->fHandlerType == H_HCI) {
				delete (HCI *) handler;
			} else {
				delete handler;
			}
			break;
		}
	}
	Print ();
}

void BluntServer::DeferredRemoveHandler (Handler *handler)
{
	int i;
	
	HLOG (1, "BluntServer::DeferredRemoveHandler %08x\n", handler);
	for (i = 0; i < fNumHandlers; i++) {
		if (fHandlers[i] == handler) {
			fHandlers[i]->fConnectionState = CONN_REMOVE;
			break;
		}
	}
}

NewtonErr BluntServer::PeekInputBuffer (UByte *data, int offset, int len)
{
	int i;
	NewtonErr r = noErr;
	
	if (fRxDMA) {
		while (len-- > 0) {
			*data = fRxDMABuffer->PeekByte (offset);
			data++;
			offset++;
		}
	} else {
		i = (fInputTail + offset) % MAX_INPUT;
		while (len > 0) {
			*data = fInputBuffer[i];
			len--;
			data++;
			i = (i + 1) % MAX_INPUT;
		}
	}
	return r;
}

NewtonErr BluntServer::ReadInputBuffer (UByte *data, int len)
{
	NewtonErr r = noErr;

	if (fRxDMA) {
		fRxDMABuffer->CopyOut (data, (ULong *) &len, &fRxStatus);
	} else {
		while (len > 0) {
			*data = fInputBuffer[fInputTail];
			len--;
			data++;
			fInputTail = (fInputTail + 1) % MAX_INPUT;
		}
	}
	return r;
}

void BluntServer::ConsumeInputBuffer (int len)
{
	if (fRxDMA) {
	} else {
		if (len > InputBufferCount ()) {
			len = InputBufferCount ();
		}
		fInputTail = (fInputTail + len) % MAX_INPUT;
	}
}

ULong BluntServer::InputBufferCount ()
{
	Long n;
	
	if (fRxDMA) {
		n = fRxDMABuffer->BufferCount ();
	} else {
		n = fInputHead - fInputTail;
		if (n < 0) n = -n;
	}
	return n;
}

void BluntServer::ResetInputBuffer ()
{
	if (fRxDMA) {
		fRxDMABuffer->Reset ();
	} else {
		fInputHead = fInputTail = 0;
	}
}

void BluntServer::Print ()
{
	int i;
	
	HLOG (0, "BluntServer::Print\n");
	for (i = 0; i < fNumHandlers; i++) fHandlers[i]->Print (4);
}

void BluntServer::Status ()
{
	HLOG (1, "BluntServer::Status\n	 Rx Error Status: %d\n", fRxErrorStatus);
	Print ();
	((HCI *) fHandlers[0])->Status ();
}

void BluntServer::Log (Long logLevel, char *format, ...)
{
	va_list args;
	char buffer[128];
	char *c;
	
	if (fLogLevel >= logLevel) {
		va_start (args, format);
		vsprintf (buffer, format, args);
		va_end (args);
		fLogger->Output ((UByte*) buffer, strlen (buffer));
	}
}

void BluntServer::LogInputBuffer (Long logLevel)
{
	int i;
	UByte c;
	ULong n;
	
	if (fLogLevel >= logLevel) {
		n = InputBufferCount ();
		if (n > 0) {
			HLOG (logLevel, "  Input buffer: %d bytes (%d %d)\n	 ", n, fInputTail, fInputHead);
			for (i = 0; i < n && i < 256; i++) {
				PeekInputBuffer (&c, i, sizeof (c));
				HLOG (logLevel, "%02x ", c);
				if ((i + 1) % 16 == 0) HLOG (logLevel, "\n	");
			}
			 HLOG (logLevel, "\n");
		} else {
			HLOG (logLevel, "  Input buffer: 0 bytes\n");
		}
	}
}