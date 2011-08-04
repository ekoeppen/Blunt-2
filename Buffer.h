#ifndef __BUFFER_H
#define __BUFFER_H

// ================================================================================
// ¥ CBuffer classes
// ================================================================================

class CMinBuffer
{
public:
	ULong dummy_00;
};

class CBuffer: public CMinBuffer
{
public:
};

class CShadowBufferSegment: public CBuffer
{
private:
	Byte filler_00[0x1C - sizeof (CBuffer)];

public:
												/* 0000 - 0000 */
												/* 0008 - 0018 */
};

class CBufferSegmentPublic
{
public:
	void 					*fJumpTable;
	UByte *dummy_04;
	ULong dummy_08;
	ULong					fPhysicalSize;
	UByte					*fBuffer;
	UByte					*fCurrent;
	UByte					*fBufferLast;
	ULong dummy_1c;
	ULong dummy_20;
	ULong dummy_24;
};

#endif
