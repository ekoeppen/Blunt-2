#define DEFAULT_LOG_LEVEL 0

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

#define GET_DATA_TYPE(b) 	(((b)[0] & 0xf8) >> 3)
#define GET_DATA_WIDTH(b) 	((b)[0] & 0x07)
#define GET_DATA_BYTE(b) 	((b)[1])
#define GET_DATA_SHORT(b) 	(((b)[1] << 8) + (b)[2])
#define GET_DATA_LONG(b)	(*(Long *) ((b) + 1))
#define GET_DATA_ULONG(b)	(*(ULong *) ((b) + 1))

#define TYPE_NIL			0
#define TYPE_UINT			1
#define TYPE_INT			2
#define TYPE_UUID			3
#define TYPE_STRING			4
#define TYPE_BOOLEAN		5
#define TYPE_SEQUENCE		6
#define TYPE_ALTERNATIVE	7
#define TYPE_URL			8

#define SIZE_BYTE			0
#define SIZE_SHORT			1
#define SIZE_LONG			2
#define SIZE_8BYTE			3
#define SIZE_16BYTE			4
#define SIZE_BYTE_LEN		5
#define SIZE_SHORT_LEN		6
#define SIZE_LONG_LEN		7

#define MAKE_HEADER(type, width) ((type << 3) | width)

#define ATTR_SERVICE_RECORD_HANDLE 					0x0000
#define ATTR_SERVICE_CLASS_ID_LIST 					0x0001 
#define ATTR_SERVICE_RECORD_STATE 					0x0002 
#define ATTR_SERVICE_ID 							0x0003 
#define ATTR_PROTOCOL_DESCRIPTOR_LIST 				0x0004 
#define ATTR_BROWSE_GROUP_LIST 						0x0005 
#define ATTR_LANGUAGE_BASE_ATTRIBUTE_ID_LIST 		0x0006 
#define ATTR_SERVICE_INFO_TIME_TO_LIVE 				0x0007 
#define ATTR_SERVICE_AVAILABILITY 					0x0008 
#define ATTR_BLUETOOTH_PROFILE_DESCRIPTOR_LIST 		0x0009 
#define ATTR_DOCUMENTATION_URL 						0x000A 
#define ATTR_CLIENT_EXECUTABLE_URL 					0x000B 
#define ATTR_ICON_URL 								0x000C 
#define ATTR_ADDITIONAL_PROTOCOL_DESCRIPTOR_LISTS	0x000D 
#define ATTR_SERVICE_NAME							0x0100
#define ATTR_GROUP_ID 								0x0200 
#define ATTR_IP_SUBNET 								0x0200
#define ATTR_VERSION_NUMBER_LIST 					0x0200 
#define ATTR_SERVICE_DATABASE_STATE 				0x0201

#define UUID_SDP									0x0001
#define UUID_RFCOMM									0x0003
#define UUID_OBEX									0x0008
#define UUID_L2CAP									0x0100
#define UUID_SERIAL_PORT							0x1101
#define UUID_PPP_LAN								0x1102
#define UUID_DUN									0x1103
#define UUID_SYNC									0x1104
#define UUID_OBEX_PUSH								0x1105
#define UUID_OBEX_FILETRANSFER						0x1106
#define UUID_PNP_INFO								0x1200

#define SRH_SERIAL_PORT							0x00010000
#define SRH_OBEX_PUSH							0x00010001
#define SRH_OBEX_FILETRANSFER					0x00010003
#define SRH_NEWTON_DOCK							0x00010004

#define EVT_SDP_ERROR_RESPONSE 						0x01
#define EVT_SDP_SERVICE_SEARCH_REQUEST 				0x02
#define EVT_SDP_SERVICE_SEARCH_RESPONSE 			0x03
#define EVT_SDP_SERVICE_ATTRIBUTE_REQUEST 			0x04
#define EVT_SDP_SERVICE_ATTRIBUTE_RESPONSE 			0x05
#define EVT_SDP_SERVICE_SEARCH_ATTRIBUTE_REQUEST 	0x06
#define EVT_SDP_SERVICE_SEARCH_ATTRIBUTE_RESPONSE 	0x07
#define EVT_SDP_TIMER								0xff

#define RFCOMM_SERIAL_PORT							4
#define RFCOMM_OBEX_PUSH							5
#define RFCOMM_OBEX_FILETRANSFER					6

enum {
	SDP_ANY,
	SDP_GET_SERVICES,
	SDP_IDLE
};

enum {
	A_NONE,
	A_SND_SEARCH_ATTRIBUTE_RESPONSE,
	A_SERVICE_RESPONSE,
	A_ERROR_RESPONSE
};

static const ULong kQueriedServices[] = {
	UUID_SERIAL_PORT,
	UUID_PPP_LAN,
	UUID_DUN,
	UUID_OBEX_PUSH,
	UUID_OBEX_FILETRANSFER
};

static const int kStates[][4] = {
	/* Request states */
	{SDP_GET_SERVICES,	EVT_SDP_SERVICE_SEARCH_ATTRIBUTE_RESPONSE,	A_SERVICE_RESPONSE,			SDP_GET_SERVICES},

	/* Response states */
	{SDP_ANY,	EVT_SDP_SERVICE_SEARCH_ATTRIBUTE_REQUEST,	A_SND_SEARCH_ATTRIBUTE_RESPONSE,	SDP_ANY},
	
	/* Error states */
	{SDP_ANY,	EVT_SDP_ERROR_RESPONSE,						A_ERROR_RESPONSE,					SDP_ANY}
};
	
static const kElementSize[] = {
	1, 2, 4, 8, 16
};

SDP::SDP (void)
{
	fLogLevel = DEFAULT_LOG_LEVEL;

	HLOG (1, "SDP::SDP %08x\n", this);

	fLocalTID = 0;
	fRemoteTID = 0;
	fHandlerType = H_SDP;
	fState = SDP_IDLE;
}
	
int SDP::Action (int action, void *eventData)
{
	int newState;
	BluntServiceResultEvent *e;
	
	newState = SDP_ANY;
	HLOG (1, "SDP::Action (%08x) %d %d\n", this, fState, action);
	switch (action) {
		case A_SND_SEARCH_ATTRIBUTE_RESPONSE:
			ServiceSearchAttributeRequest ();
			break;
		case A_SERVICE_RESPONSE:
			ServiceSearchAttributeResponse ();
			if (fCurrentQueriedService < NUM_ELEMENTS (kQueriedServices)) {
				SndServiceSearchAttributeRequest (kQueriedServices[++fCurrentQueriedService]);
			} else {
				newState = SDP_IDLE;
				e = new BluntServiceResultEvent (kResultComplete);
				fServer->SendEvent (e);
				((HCI *) fParentHandler->fParentHandler)->Disconnect (0x13);
			}
			break;
		case A_ERROR_RESPONSE:
			HLOG (1, "  Error: %d\n", GET_SHORT (fInputPacket, 0));
			break;
	}
	return newState;
}

void SDP::Transition (ULong event, void* eventData)
{
	int i;
	int newState;
	
	HLOG (1, "SDP::Transition (%08x) %d %02x -> ", this, fState, event);
	for (i = 0; i < NUM_ELEMENTS(kStates); i++) {
		if ((fState == kStates[i][0] || kStates[i][0] == SDP_ANY) && event == kStates[i][1]) {
			HLOG (1, "%d\n", i);
			newState = Action (kStates[i][2], eventData);
			if (newState != SDP_ANY) {
				fState = newState;
			} else if (kStates[i][3] != SDP_ANY) {
				fState = kStates[i][3];
			}
			break;
		}
	}
	HLOG (1, "New state: %d\n", fState);
}

void SDP::HandleTimer (void *userData)
{
	HLOG (1, "SDP::HandleTimer\n");
	Transition (EVT_SDP_TIMER, userData);
}

#pragma mark -

void SDP::Connect (void)
{
	HLOG (1, "SDP::Connect\n");
	fConnectionState = CONN_CONNECT;
	if (fParentHandler->fConnectionState != CONN_CONNECTED) {
		((L2CAP *) fParentHandler)->Connect ();
	} else {
		GetServices ();
	}
}

void SDP::GetServices (void)
{
	HLOG (1, "SDP::GetServices\n");
	fState = SDP_GET_SERVICES;
	fCurrentQueriedService = 0;
	SndServiceSearchAttributeRequest (kQueriedServices[fCurrentQueriedService]);
}

#pragma mark -

// ================================================================================
// ¥ SDP Event Handling
// ================================================================================

/*void SDP::Services (UByte *event)
{
	HLOG (1, "BluntServer::ServicesSDP (%d)\n", event[0]);

	switch (event[0]) {
		case SDP_PDU_ERROR_RESPONSE:
//			TerminateConnection ();
			break;
		case SDP_PDU_SERVICE_SEARCH_ATTRIBUTE_RESPONSE:
			fCurrentService++;
			if (fCurrentService < fNumQueriedServices) {
				fSDP->SndServiceSearchAttributeRequest (
					fQueriedServices[fCurrentService]);
			} else {
				fL2CAP->SndDisconnectionRequest (fSDP->fChannel);
			}
			break;
	}
}*/

Boolean SDP::HandleData (UByte *data, ULong length)
{
	HLOG (1, "SDP::HandleData (%08x) %08x %d\n", this, data, length);
	fRemoteTID = data[1] * 256 + data[2];
	fInputPacketLength = data[3] * 256 + data[4];
	fInputPacket = data + 5;
	fPDUID = data[0];
	HLOG (1, "  %d %d %d\n", fInputPacket[0], fRemoteTID, fInputPacketLength);
	Transition (fPDUID);
}

void SDP::GetDataElementLength (UByte *data, ULong& length, Byte& fieldLength)
{
	fieldLength = 1;
	switch (GET_DATA_WIDTH (data)) {
		case 0: length = 1; break;
		case 1: length = 2; break;
		case 2: length = 4; break;
		case 3: length = 8; break;
		case 4: length = 16; break;
		case 5: fieldLength += 1; length = GET_DATA_BYTE (data); break;
		case 6: fieldLength += 2; length = GET_DATA_SHORT (data); break;
		case 7: fieldLength += 4; length = GET_DATA_LONG (data); break;
	}
}

void SDP::GetSequenceLength (UByte *data, ULong& length, Byte& fieldLength)
{
	fieldLength = -1;
	
	if (GET_DATA_TYPE (data) == 6) {
		switch (GET_DATA_WIDTH (data)) {
			case 5:
				length = GET_DATA_BYTE (data);
				fieldLength = 2;
				break;
			case 6:
				length = GET_DATA_SHORT (data);
				fieldLength = 3;
				break;
			case 7:
				length = GET_DATA_LONG (data);
				fieldLength = 5;
				break;
		}
	}

	HLOG (2, "  SL: %d FL: %d\n", length, fieldLength);
}

void SDP::GetInteger (UByte *data, Long& value, Byte& fieldLength)
{
	switch (GET_DATA_WIDTH (data)) {
		case 0: value = GET_DATA_BYTE (data); fieldLength = 2; break;
		case 1: value = GET_DATA_SHORT (data); fieldLength = 3; break;
		case 2: value = GET_DATA_LONG (data); fieldLength = 5; break;
	}
}

void SDP::GetUUIDFromSequence (UByte *data, ULong& UUID, Byte& fieldLength)
{
	fieldLength = -1;
	
	if (GET_DATA_TYPE (data) == 3) {
		switch (GET_DATA_WIDTH (data)) {
			case 1:
				UUID = GET_DATA_SHORT (data);
				fieldLength = 2;
				break;
			case 2:
				UUID = GET_DATA_LONG (data);
				fieldLength = 4;
				break;
			case 4:
				UUID = 0;
				fieldLength = 16;
				break;
		}
	}

	if (fieldLength > 0) HLOG (2, "  UUID: %04x FL: %d\n", UUID, fieldLength);
}

void SDP::GetAttrRangeFromSequence (UByte *data, ULong& attr, Byte& fieldLength)
{
	fieldLength = -1;
	
	attr = 0;
	if (GET_DATA_TYPE (data) == 1) {
		switch (GET_DATA_WIDTH (data)) {
			case 2:
				attr = (data[1] << 24) + (data[2] << 16) + (data[4] << 8) + data[4];
				fieldLength = 5;
				break;
		}
	}

	if (fieldLength > 0) HLOG (2, "  {Attr: %08x FL: %d}", attr, fieldLength);
}

void SDP::ErrorResponse (void)
{
	HLOG (1, "SDP::ErrorResponse\n");
}

void SDP::ServiceSearchRequest (void)
{
	int i, j;
	Byte fieldLength;
	ULong sequenceLength;
	ULong UUID[12];
	int UUIDCount;
	
	HLOG (1, "SDP::ServiceSearchRequest\n  ");

	i = 0; j = 0;
	UUIDCount = 0;
	GetSequenceLength (fInputPacket + i, sequenceLength, fieldLength);
	i += fieldLength;
	
	while (fieldLength != -1 && j < sequenceLength && UUIDCount < 12) {
		GetUUIDFromSequence (fInputPacket + i, UUID[UUIDCount], fieldLength);
		if (fieldLength > 0) {
			HLOG (1, "%04x ", UUID[UUIDCount]);
			i += fieldLength; j += fieldLength; UUIDCount++;
		}
	}
	HLOG (1, "\n");
	
	SndServiceSearchResponse (UUID, UUIDCount);
}

void SDP::ServiceSearchResponse (void)
{
	HLOG (1, "SDP::ServiceSearchResponse\n");
}

void SDP::ServiceAttributeRequest (void)
{
	int i, j;
	Byte fieldLength;
	ULong sequenceLength;
	ULong UUID[12];
	int UUIDCount;
	ULong attr[12];
	int attrCount;
	Short maxReturnedAttrData;
	ULong handle;
	
	HLOG (1, "SDP::ServiceAttributeRequest\n");

//	handle = *(ULong *) (&fInputPacket[0]);
	handle = (fInputPacket[0] << 24) + (fInputPacket[1] << 16) + (fInputPacket[2] << 8) + fInputPacket[3];
	HLOG (1, "  %08x  ", handle);

	i = 6;
	GetSequenceLength (fInputPacket + i, sequenceLength, fieldLength);
	i += fieldLength;
	
	j = 0;
	attrCount = 0;
	while (fieldLength != -1 && j < sequenceLength && attrCount < 12) {
		GetAttrRangeFromSequence (fInputPacket + i, attr[attrCount], fieldLength);
		if (fieldLength > 0) {
			HLOG (1, "%08x ", attr[attrCount]);
			i += fieldLength; j += fieldLength;
			attrCount++;
		}
	}
	HLOG (1, "\n");

	SndServiceAttributeResponse (handle);
}

void SDP::ServiceAttributeResponse (void)
{
	HLOG (1, "SDP::ServiceAttributeResponse\n");
}

void SDP::ServiceSearchAttributeRequest (void)
{
	int i, j;
	Byte fieldLength;
	ULong sequenceLength;
	ULong UUID[12];
	int UUIDCount;
	ULong attr[12];
	int attrCount;
	Short maxReturnedAttrData;
	
	HLOG (1, "SDP::ServiceSearchAttributeRequest\n");

	i = 0;
	GetSequenceLength (fInputPacket + i, sequenceLength, fieldLength);
	i += fieldLength;
	
	j = 0;
	UUIDCount = 0;
	HLOG (1, "  UIDs: ");
	while (fieldLength != -1 && j < sequenceLength && UUIDCount < 12) {
		GetUUIDFromSequence (fInputPacket + i, UUID[UUIDCount], fieldLength);
		if (fieldLength > 0) {
			HLOG (1, "%04x ", UUID[UUIDCount]);
			i += fieldLength; j += fieldLength;
			UUIDCount++;
		}
	}
	HLOG (1, "\n");

	i++; // XXXX
	
	maxReturnedAttrData = GET_SHORT_N (fInputPacket, i);
	i += 2;
	
	GetSequenceLength (fInputPacket + i, sequenceLength, fieldLength);
	i += fieldLength;
	
	j = 0;
	attrCount = 0;
	HLOG (1, "  Attrs: ");
	while (fieldLength != -1 && j < sequenceLength && attrCount < 12) {
		GetAttrRangeFromSequence (fInputPacket + i, attr[attrCount], fieldLength);
		if (fieldLength > 0) {
			HLOG (1, "%08x ", attr[attrCount]);
			i += fieldLength; j += fieldLength;
			attrCount++;
		}
	}
	HLOG (1, "\n");

	SndServiceSearchAttributeResponse (UUID, UUIDCount, attr, attrCount);
}

void SDP::ServiceSearchAttributeResponse (void)
{
	int j, k, l, m;
	ULong serviceSequenceLength, attrSequenceLength, attrValueLength;
	ULong protSequenceLength, elementLength;
	Byte fieldLength, length;
	ULong attributeID;
	ULong UUID;
	Long value;
	ULong resultUUID;
	Byte resultPort;
	UByte *packet;
	Boolean serviceValid;
	BluntServiceResultEvent *e;
	
	HLOG (1, "SDP::ServiceSearchAttributeResponse\n");
	
	packet = fInputPacket + 2;
	GetSequenceLength (packet, serviceSequenceLength, fieldLength);
	packet += fieldLength;
	
	j = 0;
	while (j < serviceSequenceLength) {
		GetSequenceLength (packet + j, attrSequenceLength, fieldLength);
		HLOG (1, "  Service: %d bytes\n", attrSequenceLength);
		j += fieldLength;
		
		k = 0;
		serviceValid = false;
		while (k < attrSequenceLength) {
			// Attribute ID

			attributeID = GET_DATA_SHORT (packet + j + k);
			k += 3;
			
			// Attribute Value

			if (GET_DATA_TYPE (packet + j + k) == 6) {
				GetSequenceLength (packet + j + k, attrValueLength, fieldLength);
				k += fieldLength;
				
				if (attributeID == ATTR_SERVICE_CLASS_ID_LIST) {
					l = 0;
					while (l < attrValueLength) {
						GetUUIDFromSequence (packet + j + k + l, UUID, fieldLength);
						HLOG (1, "    Service UUID: %04x\n", UUID);
						if (UUID == UUID_SERIAL_PORT ||
							UUID == UUID_OBEX_PUSH ||
							UUID == UUID_PPP_LAN ||
							UUID == UUID_DUN ||
							UUID == UUID_OBEX_FILETRANSFER) {
							serviceValid = true;
							resultUUID = UUID;
						}
						l += fieldLength + 1;
					}
				} else if (attributeID == ATTR_PROTOCOL_DESCRIPTOR_LIST) {
					l = 0;
					while (l < attrValueLength) {
						GetSequenceLength (packet + j + k + l, protSequenceLength, fieldLength);
						l += fieldLength;
	
						m = 0;
						GetUUIDFromSequence (packet + j + k + l, UUID, fieldLength);
						HLOG (1, "    Protocol UUID: %04x\n", UUID);
						m += fieldLength + 1;
						
						while (m < protSequenceLength) {
							HLOG (1, "      Parameter: (%d)", GET_DATA_TYPE (packet + j + k + l + m));
							GetDataElementLength (packet + j + k + l + m, elementLength, fieldLength);
							if (serviceValid && UUID == UUID_RFCOMM) {
								GetInteger (packet + j + k + l + m, value, length);
								resultPort = value;
								HLOG (1, " %d", value);
							}
							HLOG (1, "\n");
							m += elementLength + fieldLength;
						}
						
						l += protSequenceLength;
					}
				}
			} else {
				GetDataElementLength (packet + j + k, attrValueLength, fieldLength);
				k += fieldLength;
			}

			k += attrValueLength;
		}
		
		if (serviceValid) {
			e = new BluntServiceResultEvent (noErr);
			memcpy (e->fBdAddr, ((HCI *) fParentHandler->fParentHandler)->fPeerAddress, 6);
			e->fServiceUUID = resultUUID;
			e->fServicePort = resultPort;
			fServer->SendEvent (e);
		}
		j += attrSequenceLength;
	}
}

void SDP::SndErrorResponse (void)
{
}

void SDP::SndServiceSearchRequest (void)
{
	UByte response[14];
	
	HLOG (1, "SDP::SndServiceSearchRequest\n");

	response[0] = EVT_SDP_SERVICE_SEARCH_REQUEST;
	
	/*
	SET_SHORT_N (response, 1, fRemoteTID);
	SET_SHORT_N (response, 3, 9);
	
	SET_SHORT_N (response, 5, 1);
	SET_SHORT_N (response, 7, 1);
	SET_ULONG_N (response, 9, SRH_OBEX_PUSH);
	*/
	
	response[1] = (fRemoteTID & 0xff00) >> 8; response[2] = fRemoteTID & 0x00ff;
	
	response[3] = 0; response[4] = 9;
	
	response[5] = 0; response[6] = 1;
	response[7] = 0; response[8] = 1;
	response[9] = 0x00; response[10] = 0x01; response[11] = 0x00; response[12] = 0x00;
	
	response[13] = 0;
	
	SndData (response, 14);
}

void SDP::SndServiceSearchResponse (ULong *UUID, Byte count)
{
	UByte response[5 + 4 * 12];
	int i;
	Byte r;
	ULong handle;
	
	HLOG (1, "SDP::SndServiceSearchResponse\n");

	response[0] = EVT_SDP_SERVICE_SEARCH_RESPONSE;
	
	/*
	SET_SHORT_N (response, 1, fRemoteTID);
	SET_SHORT_N (response, 3, 9);
	
	SET_SHORT_N (response, 5, 1);
	SET_SHORT_N (response, 7, 1);
	SET_ULONG_N (response, 9, SRH_OBEX_PUSH);
	*/
	
	response[1] = (fRemoteTID & 0xff00) >> 8; response[2] = fRemoteTID & 0x00ff;
	
	r = 0;

	for (i = 0; i < count; i++)	{
		handle = 0;
		switch (UUID[i]) {
			case UUID_SERIAL_PORT:
				handle = SRH_SERIAL_PORT;
				break;
			case UUID_OBEX_PUSH:
				handle = SRH_OBEX_PUSH;
				break;
			case UUID_OBEX_FILETRANSFER:
				handle = SRH_OBEX_FILETRANSFER;
				break;
		}
		
		if (handle != 0) {
			response[r * 4 + 9] = (handle & 0xff000000) >> 24;
			response[r * 4 + 10] = (handle & 0x00ff0000) >> 16;
			response[r * 4 + 11] = (handle & 0x0000ff00) >> 8;
			response[r * 4 + 12] = (handle & 0x000000ff);
			r++;
		}
	}

	response[3] = 0; response[4] = r * 4 + 5;
	
	response[5] = 0; response[6] = r;
	response[7] = 0; response[8] = r;
	
	response[r * 4 + 5 + 4] = 0;
	
	HLOG (1, "  %08x %d\n", response, r);
	
	SndData (response, r * 4 + 5 + 5);
}

void SDP::SndServiceAttributeRequest (void)
{
}

void SDP::SndServiceAttributeResponse (ULong handle)
{
	UByte response[48];
	Short c;
	Byte p;
	Byte len;
	
	HLOG (1, "SDP::SndServiceAttributeResponse\n");

	len = sizeof (response);
	switch (handle) {
		case SRH_SERIAL_PORT: c = UUID_SERIAL_PORT; p = 2; len -= 5; break;
		case SRH_OBEX_PUSH: c = UUID_OBEX_PUSH; p = 3; break;
		case SRH_OBEX_FILETRANSFER: c = UUID_OBEX_FILETRANSFER; p = 4; break;
	}
	
	response[0] = EVT_SDP_SERVICE_ATTRIBUTE_RESPONSE;
	SET_SHORT_N (response, 1, fRemoteTID);
	SET_SHORT_N (response, 3, len - 5);
	
	SET_SHORT_N (response, 5, len - 7 - 1);
	
	response[7] = (6 << 3) | 5;
	response[8] = len - 9 - 1;

		// Attribute 1: Service Record Handle
	
		response[9] = (1 << 3) | 1; response[10] = 0x00; response[11] = 0x00;
		response[12] = (1 << 3) | 2;
		response[13] = (handle & 0xff000000) >> 24; 
		response[14] = (handle & 0x00ff0000) >> 16;
		response[15] = (handle & 0x0000ff00) >> 8;
		response[16] = (handle & 0x000000ff);
		
		// Attribute 2: Service Class ID List

		response[17] = (1 << 3) | 1; response[18] = 0x00; response[19] = 0x01;
		response[20] = (6 << 3) | 5; response[21] = 3;
		
			response[22] = (3 << 3) | 1;
			response[23] = (c & 0xff00) >> 8; response[24] = (c & 0x00ff);

		// Attribute 3: Protocol Descriptor List
		response[25] = (1 << 3) | 1; response[26] = 0x00; response[27] = 0x04;
		
		response[28] = (6 << 3) | 5; 
		if (handle == SRH_SERIAL_PORT) {
			response[29] = 12;
		} else {
			response[29] = 17;
		}
			// L2CAP
			response[30] = (6 << 3) | 5; response[31] = 3;
			
				response[32] = (3 << 3) | 1; response[33] = 0x01; response[34] = 0x00;
				
			// RFCOMM
			response[35] = (6 << 3) | 5; response[36] = 5;
			
				response[37] = (3 << 3) | 1; response[38] = 0x00; response[39] = 0x03;
				response[40] = (1 << 3) | 0; response[41] = p;
	
		if (handle != SRH_SERIAL_PORT) {
			// OBEX
			response[42] = (6 << 3) | 5; response[43] = 3;
			
				response[44] = (3 << 3) | 1; response[45] = 0x00; response[46] = 0x08;
						
			response[47] = 0;
		} else {
			response[42] = 0;
		}
	
	SndData (response, len);
}

void SDP::SndServiceSearchAttributeRequest (ULong UUID)
{
	UByte request[21];
	
	HLOG (1, "SDP::SndServiceSearchAttributeRequest (0x%08x)\n", UUID);

	request[0] = EVT_SDP_SERVICE_SEARCH_ATTRIBUTE_REQUEST;
	
	fLocalTID++;
	request[1] = (fLocalTID & 0xff00) >> 8; request[2] = fLocalTID & 0x00ff;
	
	request[3] = 0; request[4] = sizeof (request) - 5;
	
	// Service UUID List
	
	request[5] = (6 << 3) | 5; request[6] = 3;
	
		// Service UUID
	
		request[7] = (3 << 3) | 1; request[8] = (UUID & 0xff00) >> 8; request[9] = (UUID & 0x00ff);
			
	// Maximum Return Bytes		
			
	request[10] = ((((HCI *) fParentHandler->fParentHandler)->fHCIBufferSize - 8) & 0xff00) >> 8;
	request[11] =  (((HCI *) fParentHandler->fParentHandler)->fHCIBufferSize - 8) & 0x00ff;
	
	// Attribute UUID List
	
	request[12] = (6 << 3) | 5; request[13] = 6;
	
		// Protocol descriptor and service class attributes
	
		request[14] = (1 << 3) | 1;	request[15] = 0x00; request[16] = 0x01;
		request[17] = (1 << 3) | 1;	request[18] = 0x00; request[19] = 0x04;

	request[20] = 0;
			
	SndData (request, sizeof (request));
}

void SDP::SndServiceSearchAttributeResponse (ULong *UUID, Byte UUIDCount, ULong *attr, Byte attrCount)
{
	UByte buffer[200];
	UByte *b;
	int responses;
	
	HLOG (1, "SDP::SndServiceSearchAttributeResponse\n");

	b = buffer + sizeof (buffer) - 1;

	// Continuation state
	*b = 0;

	responses = 0;
	while (UUIDCount-- > 0) {
		switch (UUID[UUIDCount]) {
			case UUID_SERIAL_PORT:
				{
					// Attribute 5: Service Name
					b = MakeString (b, "Serial Port");
					b = MakeAttribute (b, ATTR_SERVICE_NAME);
					
					// Attribute 4: Bluetooth Profile Descriptor List
					{
						// Serial Port Profile
						{
							b = MakeUInt (b, 0x0100);
							b = MakeUUID (b, UUID_SERIAL_PORT);
						}
						b = MakeSequence (b, 2);
					}
					b = MakeSequence (b, 1);
					b = MakeAttribute (b, ATTR_BLUETOOTH_PROFILE_DESCRIPTOR_LIST);
					
					// Attribute 3: Protocol Descriptor List
					{
						// RFCOMM
						{
							b = MakeUInt (b, RFCOMM_SERIAL_PORT);
							b = MakeUUID (b, UUID_RFCOMM);
						}
						b = MakeSequence (b, 2);

						// L2CAP
						{ 
							b = MakeUUID (b, UUID_L2CAP);
						}
						b = MakeSequence (b, 1);
					}
					b = MakeSequence (b, 2);
					b = MakeAttribute (b, ATTR_PROTOCOL_DESCRIPTOR_LIST);
					
					// Attribute 2: Service Class ID List
					{
						{
							b = MakeUUID (b, UUID_SERIAL_PORT);
						}
						b = MakeSequence (b, 1);
					}
					b = MakeSequence (b, 1);
					b = MakeAttribute (b, ATTR_SERVICE_CLASS_ID_LIST);
		
					// Attribute 1: Service Record Handle
					b = MakeUInt (b, SRH_SERIAL_PORT);
					b = MakeAttribute (b, ATTR_SERVICE_RECORD_HANDLE);
				}
				b = MakeSequence (b, 10);
				responses++;
				break;
			case UUID_OBEX_PUSH:
				{
					// Attribute 5: Service Name
					b = MakeString (b, "OBEX Push");
					b = MakeAttribute (b, ATTR_SERVICE_NAME);
					
					// Attribute 4: Bluetooth Profile Descriptor List
					{
						// Serial Port Profile
						{
							b = MakeUInt (b, 0x0100);
							b = MakeUUID (b, UUID_OBEX_PUSH);
						}
						b = MakeSequence (b, 2);
					}
					b = MakeSequence (b, 1);
					b = MakeAttribute (b, ATTR_BLUETOOTH_PROFILE_DESCRIPTOR_LIST);
					
					// Attribute 3: Protocol Descriptor List
					{
						// RFCOMM
						{
							b = MakeUInt (b, RFCOMM_OBEX_PUSH);
							b = MakeUUID (b, UUID_RFCOMM);
						}
						b = MakeSequence (b, 2);

						// L2CAP
						{ 
							b = MakeUUID (b, UUID_L2CAP);
						}
						b = MakeSequence (b, 1);
					}
					b = MakeSequence (b, 2);
					b = MakeAttribute (b, ATTR_PROTOCOL_DESCRIPTOR_LIST);
					
					// Attribute 2: Service Class ID List
					{
						{
							b = MakeUUID (b, UUID_OBEX_PUSH);
						}
						b = MakeSequence (b, 1);
					}
					b = MakeSequence (b, 1);
					b = MakeAttribute (b, ATTR_SERVICE_CLASS_ID_LIST);
		
					// Attribute 1: Service Record Handle
					b = MakeUInt (b, SRH_OBEX_PUSH);
					b = MakeAttribute (b, ATTR_SERVICE_RECORD_HANDLE);
				}
				b = MakeSequence (b, 10);
				responses++;
				break;
			case UUID_OBEX_FILETRANSFER:
				{
					// Attribute 5: Service Name
					b = MakeString (b, "OBEX File Transfer");
					b = MakeAttribute (b, ATTR_SERVICE_NAME);
					
					// Attribute 4: Bluetooth Profile Descriptor List
					{
						// Serial Port Profile
						{
							b = MakeUInt (b, 0x0100);
							b = MakeUUID (b, UUID_OBEX_FILETRANSFER);
						}
						b = MakeSequence (b, 2);
					}
					b = MakeSequence (b, 1);
					b = MakeAttribute (b, ATTR_BLUETOOTH_PROFILE_DESCRIPTOR_LIST);
					
					// Attribute 3: Protocol Descriptor List
					{
						// RFCOMM
						{
							b = MakeUInt (b, RFCOMM_OBEX_FILETRANSFER);
							b = MakeUUID (b, UUID_RFCOMM);
						}
						b = MakeSequence (b, 2);

						// L2CAP
						{ 
							b = MakeUUID (b, UUID_L2CAP);
						}
						b = MakeSequence (b, 1);
					}
					b = MakeSequence (b, 2);
					b = MakeAttribute (b, ATTR_PROTOCOL_DESCRIPTOR_LIST);
					
					// Attribute 2: Service Class ID List
					{
						{
							b = MakeUUID (b, UUID_OBEX_FILETRANSFER);
						}
						b = MakeSequence (b, 1);
					}
					b = MakeSequence (b, 1);
					b = MakeAttribute (b, ATTR_SERVICE_CLASS_ID_LIST);
		
					// Attribute 1: Service Record Handle
					b = MakeUInt (b, SRH_SERIAL_PORT);
					b = MakeAttribute (b, ATTR_SERVICE_RECORD_HANDLE);
				}
				b = MakeSequence (b, 10);
				responses++;
				break;
		}
	}
	b = MakeSequence (b, responses);

	b -= 7;
	b[0] = EVT_SDP_SERVICE_SEARCH_ATTRIBUTE_RESPONSE;
	SET_SHORT_N (b, 1, fRemoteTID);

	// Packet length
	SET_SHORT_N (b, 3, sizeof (buffer) - (b - buffer) - 5);
	
	// Attribute data length
	SET_SHORT_N (b, 5, sizeof (buffer) - (b - buffer) - 7 - 1);

	SndData (b, sizeof (buffer) - (b - buffer));

/*	

		// Serial Port Service
		
		response[9] = (6 << 3) | 5;
		response[10] = 33;

			// Attribute 1: Service Record Handle
		
			response[11] = (1 << 3) | 1; response[12] = 0x00; response[13] = 0x00;
			response[14] = (1 << 3) | 2; response[15] = 0x00; response[16] = 0x01; response[17] = 0x00; response[18] = 0x00;
			
			// Attribute 2: Service Class ID List

			response[19] = (1 << 3) | 1; response[20] = 0x00; response[21] = 0x01;
			response[22] = (6 << 3) | 5; response[23] = 3;
			
				response[24] = (3 << 3) | 1; response[25] = 0x11; response[26] = 0x01;

			// Attribute 3: Protocol Descriptor List
			
			response[27] = (1 << 3) | 1; response[28] = 0x00; response[29] = 0x04;
			response[30] = (6 << 3) | 5; response[31] = 12;
			
				// L2CAP

				response[32] = (6 << 3) | 5; response[33] = 3;
				
					response[34] = (3 << 3) | 1; response[35] = 0x01; response[36] = 0x00;
					
				// RFCOMM
	
				response[37] = (6 << 3) | 5; response[38] = 5;
				
					response[39] = (3 << 3) | 1; response[40] = 0x00; response[41] = 0x03;
					response[42] = (1 << 3) | 0; response[43] = 0x04;

		// OBEX Push Service
		
		response[44] = (6 << 3) | 5;
		response[45] = 63;

			// Attribute 1: Service Record Handle
		
			response[46] = (1 << 3) | 1; response[47] = 0x00; response[48] = 0x00;
			response[49] = (1 << 3) | 2; response[50] = 0x00; response[51] = 0x01; response[52] = 0x00; response[53] = 0x01;
			
			// Attribute 2: Service Class ID List

			response[54] = (1 << 3) | 1; response[55] = 0x00; response[56] = 0x01;
			response[57] = (6 << 3) | 5; response[58] = 3;
			
				response[59] = (3 << 3) | 1; response[60] = 0x11; response[61] = 0x05;

			// Attribute 3: Protocol Descriptor List
			
			response[62] = (1 << 3) | 1; response[63] = 0x00; response[64] = 0x04;
			response[65] = (6 << 3) | 5; response[66] = 17;
			
				// L2CAP

				response[67] = (6 << 3) | 5; response[68] = 3;
				
					response[69] = (3 << 3) | 1; response[70] = 0x01; response[71] = 0x00;
					
				// RFCOMM
	
				response[72] = (6 << 3) | 5; response[73] = 5;
				
					response[74] = (3 << 3) | 1; response[75] = 0x00; response[76] = 0x03;
					response[77] = (1 << 3) | 0; response[78] = 0x05;

				// OBEX

				response[79] = (6 << 3) | 5; response[80] = 3;
				
					response[81] = (3 << 3) | 1; response[82] = 0x00; response[83] = 0x08;
					
			// Attribute 4: Bluetooth Profile Descriptor List
			
			response[84] = (1 << 3) | 1; response[85] = 0x00; response[86] = 0x09;
			response[87] = (6 << 3) | 5; response[88] = 8;
			
				// OBEX Push Profile
	
				response[89] = (6 << 3) | 5; response[90] = 5;
				
					response[91] = (3 << 3) | 1; response[92] = 0x11; response[93] = 0x05;
					response[94] = (1 << 3) | 1; response[95] = 0x01; response[96] = 0x00;

			// Attribute 5: Service Name
			
			response[97] = (1 << 3) | 1; response[98] = 0x01; response[99] = 0x00;
			response[100] = (4 << 3) | 5; response[101] = 7;
			
				// "OPUSH"

				response[102] = 'P';
				response[103] = 'u';
				response[104] = 's';
				response[105] = 'h';
				response[106] = ' ';
				response[107] = ' ';
				response[108] = ' ';

		// OBEX File Transfer Service
		
		response[109] = (6 << 3) | 5;
		response[110] = 69;

			// Attribute 1: Service Record Handle
		
			response[111] = (1 << 3) | 1; response[112] = 0x00; response[113] = 0x00;
			response[114] = (1 << 3) | 2; response[115] = 0x00; response[116] = 0x01; response[117] = 0x00; response[118] = 0x02;
			
			// Attribute 2: Service Class ID List

			response[119] = (1 << 3) | 1; response[120] = 0x00; response[121] = 0x01;
			response[122] = (6 << 3) | 5; response[123] = 3;
			
				response[124] = (3 << 3) | 1; response[125] = 0x11; response[126] = 0x06;

			// Attribute 3: Protocol Descriptor List
			
			response[127] = (1 << 3) | 1; response[128] = 0x00; response[129] = 0x04;
			response[130] = (6 << 3) | 5; response[131] = 17;
			
				// L2CAP

				response[132] = (6 << 3) | 5; response[133] = 3;
				
					response[134] = (3 << 3) | 1; response[135] = 0x01; response[136] = 0x00;
					
				// RFCOMM
	
				response[137] = (6 << 3) | 5; response[138] = 5;
				
					response[139] = (3 << 3) | 1; response[140] = 0x00; response[141] = 0x03;
					response[142] = (1 << 3) | 0; response[143] = 0x06;

				// OBEX

				response[144] = (6 << 3) | 5; response[145] = 3;
				
					response[146] = (3 << 3) | 1; response[147] = 0x00; response[148] = 0x08;
					
			// Attribute 4: Bluetooth Profile Descriptor List
			
			response[149] = (1 << 3) | 1; response[150] = 0x00; response[151] = 0x09;
			response[152] = (6 << 3) | 5; response[153] = 8;
			
				// OBEX File Transfer Profile
	
				response[154] = (6 << 3) | 5; response[155] = 5;
				
					response[156] = (3 << 3) | 1; response[157] = 0x11; response[158] = 0x05;
					response[159] = (1 << 3) | 1; response[160] = 0x01; response[161] = 0x00;

			// Attribute 5: Service Name
			
			response[162] = (1 << 3) | 1; response[163] = 0x01; response[164] = 0x00;

			// "TRANS"

			response[165] = (4 << 3) | 5; response[166] = 13;
			
				response[167] = 'F';
				response[168] = 'i';
				response[169] = 'l';
				response[170] = 'e';
				response[171] = ' ';
				response[172] = 'T';
				response[173] = 'r';
				response[174] = 'a';
				response[175] = 'n';
				response[176] = 's';
				response[177] = 'f';
				response[178] = 'e';
				response[179] = 'r';

		response[180] = 0;
*/	
}

void SDP::SndData (UByte *data, Short length)
{
	((L2CAP*) fParentHandler)->SndData (data, length);
}

#pragma mark -

int SDP::GetElementSize (UByte* data)
{
	int r;
	
	r = 0;
	switch (*data & 0x07) {
		case SIZE_BYTE_LEN:
			r = data[1] + 2;
			break;
		case SIZE_SHORT_LEN:
			r = (data[1] << 8) + data[2] + 3;
			break;
		case SIZE_LONG_LEN:
			r = (((((data[1] << 8) + data[2]) << 8) + data[3]) << 8) + data[4] + 5;
			break;
		default:
			r = kElementSize[*data & 0x07] + 1;
			break;
	}
	return r;
}

UByte* SDP::MakeUInt (UByte* buffer, unsigned int data)
{
	if (data < 0x100) {
		buffer -= 2;
		buffer[0] = MAKE_HEADER (TYPE_UINT, SIZE_BYTE);
		buffer[1] = data & 0x000000ff;
	} else if (data < 0x10000) {
		buffer -= 3;
		buffer[0] = MAKE_HEADER (TYPE_UINT, SIZE_SHORT);
		SET_USHORT_N (buffer, 1, data);
	} else {
		buffer -= 5;
		buffer[0] = MAKE_HEADER (TYPE_UINT, SIZE_LONG);
		SET_ULONG_N (buffer, 1, data);
	}
	return buffer;
}

UByte* SDP::MakeUUID (UByte* buffer, unsigned int uuid)
{
	if (uuid < 0x10000) {
		buffer -= 3;
		buffer[0] = MAKE_HEADER (TYPE_UUID, SIZE_SHORT);
		SET_USHORT_N (buffer, 1, uuid);
	} else {
		buffer -= 5;
		buffer[0] = MAKE_HEADER (TYPE_UUID, SIZE_LONG);
		SET_ULONG_N (buffer, 1, uuid);
	}
	return buffer;
}

UByte* SDP::MakeAttribute (UByte* buffer, unsigned short attr)
{
	buffer -= 3;
	buffer[0] = MAKE_HEADER (TYPE_UINT, SIZE_SHORT);
	SET_USHORT_N (buffer, 1, attr);
	return buffer;
}

UByte* SDP::MakeString (UByte* buffer, char *data)
{
	int len;
	
	len = strlen (data);
	HLOG (1, "SDP::MakeString %08x %d", buffer, len);
	if (len < 0x100) {
		buffer = buffer - len - 2;
		buffer[0] =  MAKE_HEADER (TYPE_STRING, SIZE_BYTE_LEN);
		buffer[1] = len;
		memcpy (buffer + 2, data, len);
	} else {
		buffer -= (len + 3);
		buffer[0] =  MAKE_HEADER (TYPE_STRING, SIZE_SHORT_LEN);
		SET_USHORT_N (buffer, 1, len);
		memcpy (buffer + 3, data, len);
	}
	HLOG (1, " %08x\n", buffer);
	return buffer;
}

UByte* SDP::MakeSequence (UByte* buffer, int numElements)
{
	int len, n;
	UByte *b;
	
	HLOG (2, "SDP::MakeSequence %08x %d\n", buffer, numElements);
	b = buffer;
	len = 0;
	while (numElements-- > 0) {
		n = GetElementSize (b);
		HLOG (2, "  %08x %d (%d)\n", b, n, len);
		len += n;
		b += n;
	}
	if (len < 0x100) {
		buffer -= 2;
		buffer[0] =  MAKE_HEADER (TYPE_SEQUENCE, SIZE_BYTE_LEN);
		buffer[1] = len;
	} else {
		buffer -= 3;
		buffer[0] =  MAKE_HEADER (TYPE_SEQUENCE, SIZE_SHORT_LEN);
		SET_USHORT_N (buffer, 1, len);
	}
	return buffer;
}

/*
void SDP::SndServiceSearchAttributeResponse (ULong *UUID, Byte UUIDCount, ULong *attr, Byte attrCount)
{
	UByte response[181];
	
	HLOG (1, "SDP::SndServiceSearchAttributeResponse\n");

	response[0] = EVT_SDP_SERVICE_SEARCH_ATTRIBUTE_RESPONSE;
	SET_SHORT_N (response, 1, fRemoteTID);
	SET_SHORT_N (response, 3, sizeof (response) - 5);
	
	SET_SHORT_N (response, 5, sizeof (response) - 7 - 1);
	
	response[7] = (6 << 3) | 5;
	response[8] = sizeof (response) - 9 - 1;
	
		// Serial Port Service
		
		response[9] = (6 << 3) | 5;
		response[10] = 33;

			// Attribute 1: Service Record Handle
		
			response[11] = (1 << 3) | 1; response[12] = 0x00; response[13] = 0x00;
			response[14] = (1 << 3) | 2; response[15] = 0x00; response[16] = 0x01; response[17] = 0x00; response[18] = 0x00;
			
			// Attribute 2: Service Class ID List

			response[19] = (1 << 3) | 1; response[20] = 0x00; response[21] = 0x01;
			response[22] = (6 << 3) | 5; response[23] = 3;
			
				response[24] = (3 << 3) | 1; response[25] = 0x11; response[26] = 0x01;

			// Attribute 3: Protocol Descriptor List
			
			response[27] = (1 << 3) | 1; response[28] = 0x00; response[29] = 0x04;
			response[30] = (6 << 3) | 5; response[31] = 12;
			
				// L2CAP

				response[32] = (6 << 3) | 5; response[33] = 3;
				
					response[34] = (3 << 3) | 1; response[35] = 0x01; response[36] = 0x00;
					
				// RFCOMM
	
				response[37] = (6 << 3) | 5; response[38] = 5;
				
					response[39] = (3 << 3) | 1; response[40] = 0x00; response[41] = 0x03;
					response[42] = (1 << 3) | 0; response[43] = 0x04;

		// OBEX Push Service
		
		response[44] = (6 << 3) | 5;
		response[45] = 63;

			// Attribute 1: Service Record Handle
		
			response[46] = (1 << 3) | 1; response[47] = 0x00; response[48] = 0x00;
			response[49] = (1 << 3) | 2; response[50] = 0x00; response[51] = 0x01; response[52] = 0x00; response[53] = 0x01;
			
			// Attribute 2: Service Class ID List

			response[54] = (1 << 3) | 1; response[55] = 0x00; response[56] = 0x01;
			response[57] = (6 << 3) | 5; response[58] = 3;
			
				response[59] = (3 << 3) | 1; response[60] = 0x11; response[61] = 0x05;

			// Attribute 3: Protocol Descriptor List
			
			response[62] = (1 << 3) | 1; response[63] = 0x00; response[64] = 0x04;
			response[65] = (6 << 3) | 5; response[66] = 17;
			
				// L2CAP

				response[67] = (6 << 3) | 5; response[68] = 3;
				
					response[69] = (3 << 3) | 1; response[70] = 0x01; response[71] = 0x00;
					
				// RFCOMM
	
				response[72] = (6 << 3) | 5; response[73] = 5;
				
					response[74] = (3 << 3) | 1; response[75] = 0x00; response[76] = 0x03;
					response[77] = (1 << 3) | 0; response[78] = 0x05;

				// OBEX

				response[79] = (6 << 3) | 5; response[80] = 3;
				
					response[81] = (3 << 3) | 1; response[82] = 0x00; response[83] = 0x08;
					
			// Attribute 4: Bluetooth Profile Descriptor List
			
			response[84] = (1 << 3) | 1; response[85] = 0x00; response[86] = 0x09;
			response[87] = (6 << 3) | 5; response[88] = 8;
			
				// OBEX Push Profile
	
				response[89] = (6 << 3) | 5; response[90] = 5;
				
					response[91] = (3 << 3) | 1; response[92] = 0x11; response[93] = 0x05;
					response[94] = (1 << 3) | 1; response[95] = 0x01; response[96] = 0x00;

			// Attribute 5: Service Name
			
			response[97] = (1 << 3) | 1; response[98] = 0x01; response[99] = 0x00;
			response[100] = (4 << 3) | 5; response[101] = 7;
			
				// "OPUSH"

				response[102] = 'P';
				response[103] = 'u';
				response[104] = 's';
				response[105] = 'h';
				response[106] = ' ';
				response[107] = ' ';
				response[108] = ' ';

		// OBEX File Transfer Service
		
		response[109] = (6 << 3) | 5;
		response[110] = 69;

			// Attribute 1: Service Record Handle
		
			response[111] = (1 << 3) | 1; response[112] = 0x00; response[113] = 0x00;
			response[114] = (1 << 3) | 2; response[115] = 0x00; response[116] = 0x01; response[117] = 0x00; response[118] = 0x02;
			
			// Attribute 2: Service Class ID List

			response[119] = (1 << 3) | 1; response[120] = 0x00; response[121] = 0x01;
			response[122] = (6 << 3) | 5; response[123] = 3;
			
				response[124] = (3 << 3) | 1; response[125] = 0x11; response[126] = 0x06;

			// Attribute 3: Protocol Descriptor List
			
			response[127] = (1 << 3) | 1; response[128] = 0x00; response[129] = 0x04;
			response[130] = (6 << 3) | 5; response[131] = 17;
			
				// L2CAP

				response[132] = (6 << 3) | 5; response[133] = 3;
				
					response[134] = (3 << 3) | 1; response[135] = 0x01; response[136] = 0x00;
					
				// RFCOMM
	
				response[137] = (6 << 3) | 5; response[138] = 5;
				
					response[139] = (3 << 3) | 1; response[140] = 0x00; response[141] = 0x03;
					response[142] = (1 << 3) | 0; response[143] = 0x06;

				// OBEX

				response[144] = (6 << 3) | 5; response[145] = 3;
				
					response[146] = (3 << 3) | 1; response[147] = 0x00; response[148] = 0x08;
					
			// Attribute 4: Bluetooth Profile Descriptor List
			
			response[149] = (1 << 3) | 1; response[150] = 0x00; response[151] = 0x09;
			response[152] = (6 << 3) | 5; response[153] = 8;
			
				// OBEX File Transfer Profile
	
				response[154] = (6 << 3) | 5; response[155] = 5;
				
					response[156] = (3 << 3) | 1; response[157] = 0x11; response[158] = 0x05;
					response[159] = (1 << 3) | 1; response[160] = 0x01; response[161] = 0x00;

			// Attribute 5: Service Name
			
			response[162] = (1 << 3) | 1; response[163] = 0x01; response[164] = 0x00;

			// "TRANS"

			response[165] = (4 << 3) | 5; response[166] = 13;
			
				response[167] = 'F';
				response[168] = 'i';
				response[169] = 'l';
				response[170] = 'e';
				response[171] = ' ';
				response[172] = 'T';
				response[173] = 'r';
				response[174] = 'a';
				response[175] = 'n';
				response[176] = 's';
				response[177] = 'f';
				response[178] = 'e';
				response[179] = 'r';

		response[180] = 0;
	
	SndData (response, sizeof (response));
}

*/