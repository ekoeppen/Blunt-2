#ifndef __SDPPLAYER_H
#define __SDPPLAYER_H

class L2CAP;
class Channel;

class SDP: public Handler
{
public:
	UByte					*fInputPacket;
	Short					fInputPacketLength;
	Short					fLocalTID;
	Short					fRemoteTID;
	Byte					fPDUID;
	
	int						fCurrentQueriedService;

							SDP (void);
							
	void					Transition (ULong event, void *eventData = NULL);
	int						Action (int state, void *eventData = NULL);
	Boolean					HandleData (UByte *data, ULong length);
	void					HandleTimer (void *userData);
	
	void					Connect ();
	
	void					GetServices (void);
	
	// Utility functions

	void					GetDataElementLength (UByte *data, ULong& length, Byte& fieldLength);	
	void					GetSequenceLength (UByte *data, ULong& length, Byte& fieldLength);
	void					GetUUIDFromSequence (UByte *data, ULong& UUID, Byte& fieldLength);
	void					GetAttrRangeFromSequence (UByte *data, ULong& attr, Byte& fieldLength);
	void					GetInteger (UByte *data, Long& value, Byte& fieldLength);
	
	int 					GetElementSize (UByte* data);
	UByte* 					MakeUInt (UByte* buffer, unsigned int data);
	UByte* 					MakeUUID (UByte* buffer, unsigned int uuid);
	UByte* 					MakeAttribute (UByte* buffer, unsigned short attr);
	UByte* 					MakeString (UByte* buffer, char *data);
	UByte* 					MakeSequence (UByte* buffer, int numElements);

	// Events

	void					ErrorResponse (void);
	void					ServiceSearchRequest (void);
	void					ServiceSearchResponse (void);
	void					ServiceAttributeRequest (void);
	void					ServiceAttributeResponse (void);
	void					ServiceSearchAttributeRequest (void);
	void					ServiceSearchAttributeResponse (void);

	// Actions

	void					SndErrorResponse (void);
	void					SndServiceSearchRequest (void);
	void					SndServiceSearchResponse (ULong *UUID, Byte count);
	void					SndServiceAttributeRequest (void);
	void					SndServiceAttributeResponse (ULong handle);
	void					SndServiceSearchAttributeRequest (ULong UUID);
	void					SndServiceSearchAttributeResponse (ULong *UUID, Byte UUIDCount, ULong *attr, Byte attrCount);
	
	void					SndData (UByte *data, Short length);
};

#endif