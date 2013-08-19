#include <HALOptions.h>
#include <SerialChipRegistry.h>
#include "NSandDDKIncludes.h"
#include "BluntServer.h"
#include "BluntClient.h"
#include "CircleBuf.h"
#include "Logger.h"

extern "C" Ref MStart (RefArg rcvr, RefArg location, RefArg driver, RefArg speed, RefArg logLevel)
{
	BluntServer* server = NULL;
	BluntClient* client = NULL;
	UChar* pc;
	ULong l;
	
	printf("--- Starting -------------------------------------------------------\n");
	WITH_LOCKED_BINARY(location, p);
		pc = (UChar *) p; l = (pc[1] << 24) + (pc[3] << 16) + (pc[5] << 8) + pc[7];
	END_WITH_LOCKED_BINARY(location);

	server = new BluntServer ();
	if (server->Initialize (l, RINT (driver), RINT (speed), RINT (logLevel)) == noErr) {
		server->StartTask ();
		SetFrameSlot (rcvr, SYM (server), MAKEINT ((ULong) server));
		client = new BluntClient (rcvr, BluntServer::Port ());
		SetFrameSlot (rcvr, SYM (client), MAKEINT ((ULong) client));
		return TRUEREF;
	} else {
		delete server;
		return NILREF;
	}
}

extern "C" Ref MReset (RefArg rcvr, RefArg name)
{
	Char buffer[64];
	Ref asciiName;
	
	BluntClient* client = (BluntClient*) RINT (GetFrameSlot(rcvr, SYM (client)));
	asciiName = ASCIIString (name);
	WITH_LOCKED_BINARY(asciiName, n)
	memset (buffer, 0, 64);
	memcpy (buffer, n, Length (asciiName));
	client->Reset (buffer);
	END_WITH_LOCKED_BINARY(asciiName)
	return NILREF;
}

extern "C" Ref MDiscover (RefArg rcvr, RefArg time, RefArg amount)
{
	BluntClient* client = (BluntClient*) RINT (GetFrameSlot(rcvr, SYM (client)));
	client->Discover (RINT (time), RINT (amount));
	return NILREF;
}

extern "C" Ref MCancelDiscover (RefArg rcvr)
{
	BluntClient* client = (BluntClient*) RINT (GetFrameSlot(rcvr, SYM (client)));
	client->CancelDiscover ();
	return NILREF;
}

extern "C" Ref MNameRequest (RefArg rcvr, RefArg bdAddr, RefArg psRepMode, RefArg psMode)
{
	BluntClient* client = (BluntClient*) RINT (GetFrameSlot(rcvr, SYM (client)));
	WITH_LOCKED_BINARY(bdAddr, a)
	client->NameRequest ((UByte*) a, RINT (psRepMode), RINT (psMode));
	END_WITH_LOCKED_BINARY(bdAddr)
	return NILREF;
}

extern "C" Ref MGetServices (RefArg rcvr, RefArg bdAddr, RefArg psRepMode, RefArg psMode, RefArg linkKey)
{
	BluntClient* client = (BluntClient*) RINT (GetFrameSlot(rcvr, SYM (client)));
	WITH_LOCKED_BINARY(linkKey, k)
	WITH_LOCKED_BINARY(bdAddr, a)
	client->GetServices ((UByte*) a, RINT (psRepMode), RINT (psMode), (UByte *) k);
	END_WITH_LOCKED_BINARY(bdAddr)
	END_WITH_LOCKED_BINARY(linkKey)
	return NILREF;
}

extern "C" Ref MConnect (RefArg rcvr, RefArg bdAddr, RefArg psRepMode, RefArg psMode, RefArg port, RefArg linkKey)
{
	BluntClient* client = (BluntClient*) RINT (GetFrameSlot(rcvr, SYM (client)));
	WITH_LOCKED_BINARY(linkKey, k)
	WITH_LOCKED_BINARY(bdAddr, a)
	client->Connect ((UByte*) a, RINT (psRepMode), RINT (psMode), RINT (port), (UByte *) k);
	END_WITH_LOCKED_BINARY(bdAddr)
	END_WITH_LOCKED_BINARY(linkKey)
	return NILREF;
}

extern "C" Ref MDisconnect (RefArg rcvr, RefArg bdAddr)
{
	BluntClient* client = (BluntClient*) RINT (GetFrameSlot(rcvr, SYM (client)));
	WITH_LOCKED_BINARY(bdAddr, a)
	client->Disconnect ((UByte*) a);
	END_WITH_LOCKED_BINARY(bdAddr)
	return NILREF;
}

extern "C" Ref MPair (RefArg rcvr, RefArg bdAddr, RefArg PIN, RefArg psRepMode, RefArg psMode)
{
	Char buffer[64];
	Ref asciiPIN;
	
	BluntClient* client = (BluntClient*) RINT (GetFrameSlot(rcvr, SYM (client)));
	asciiPIN = ASCIIString (PIN);
	WITH_LOCKED_BINARY(asciiPIN, p)
	memset (buffer, 0, 64);
	memcpy (buffer, p, Length (asciiPIN));
	END_WITH_LOCKED_BINARY(asciiPIN)
	WITH_LOCKED_BINARY(bdAddr, a)
	client->Pair ((UByte*) a, buffer, RINT (psRepMode), RINT (psMode));
	END_WITH_LOCKED_BINARY(bdAddr)
	return NILREF;
}

extern "C" Ref MStop (RefArg rcvr)
{
	BluntServer* server = NULL;
	BluntClient* client = NULL;
	TUPort port;
	
	client = (BluntClient*) RINT (GetFrameSlot(rcvr, SYM (client)));
	server = (BluntServer*) RINT (GetFrameSlot(rcvr, SYM (server)));
	client->Stop ();
	
	Sleep(1 * kSeconds);
	delete client;
	delete server;
	
	RemoveSlot (rcvr, SYM (client));
	RemoveSlot (rcvr, SYM (server));

	return NILREF;
}

extern "C" Ref MStatus (RefArg rcvr)
{
	BluntClient* client = (BluntClient*) RINT (GetFrameSlot(rcvr, SYM (client)));
	client->Status ();
	return NILREF;
}

extern "C" Ref MSetLogLevel (RefArg rcvr, RefArg levels)
{
	BluntClient* client = (BluntClient*) RINT (GetFrameSlot(rcvr, SYM (client)));
	client->SetLogLevel (
		RINT (GetFrameSlot(levels, SYM (client))),
		RINT (GetFrameSlot(levels, SYM (server))),
		RINT (GetFrameSlot(levels, SYM (hci))),
		RINT (GetFrameSlot(levels, SYM (l2cap))),
		RINT (GetFrameSlot(levels, SYM (sdp))),
		RINT (GetFrameSlot(levels, SYM (rfcomm))));
	return NILREF;
}

extern "C" Ref MPowerOff (RefArg rcvr, RefArg location)
{
	PSerialChipRegistry* reg;
	SerialChipID id;
	TSerialChip* chip;
	UChar* pc;
	ULong l;
	
	WITH_LOCKED_BINARY(location, p)
	pc = (UChar *) p; l = (pc[1] << 24) + (pc[3] << 16) + (pc[5] << 8) + pc[7];
	END_WITH_LOCKED_BINARY(location)
	reg = GetSerialChipRegistry ();
	id = reg->FindByLocation (l);
	chip = reg->GetChipPtr (id);
	if (chip != NULL) {
		chip->PowerOff ();
	}
}
