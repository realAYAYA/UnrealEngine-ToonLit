// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "UObject/Object.h"

#include "DropTimecode.h"

/** Process audio samples to extract Linear Timecode Signal
*/

class LINEARTIMECODE_API FLinearTimecodeDecoder
{
	// public interface, methods
public:
	/** TimeCode structure used by reader
	*/
	FLinearTimecodeDecoder();

	/** Flush internal state of timecode reader 
	*/
	void Reset(void);

	/** Analyze a single sample, looking for a time code pattern
	* @param InSample - single normalized sample, 1.0 to -1.0 
	* @param OutTimeCode - returned timecode to this point, untouched if not end of sequence sample
	* @return true if sample was last in a timecode frame
	*/
	bool Sample(float InSample, FDropTimecode& OutTimeCode);

protected:
	// used to extract timecode from bit stream
	struct EDecodePattern
	{
		enum class EDecodeType
		{
			Hours,
			Minutes,
			Seconds,
			Frames,
			DropFrame,
			ColorFrame,	
			FrameRate,
			End,
		};
		EDecodeType Type;
		int32 Index;
		uint16 Mask;
		int32 Addition;
	};

protected:
	// Add bit to decoded stream.
	void ShiftAndInsert(uint16* InBitStream, bool InBit) const;

	void DecodeBitStream(uint16* InBitStream, EDecodePattern* InDecodePatern, int32 InForward, FDropTimecode& OutTimeCode);
	bool HasCompleteFrame(uint16* InBitStream) const;
	bool DecodeFrame(uint16* InBitStream, FDropTimecode& OutTimeCode);

	// Tries to calculate the number of clocks per bit
	int32 AdjustCycles(int32 InClock) const;

protected:
	static EDecodePattern ForwardPattern[];
	static EDecodePattern BackwardPattern[];

	// Sample value to detect zero crossings/edges of the input signal
	float Center;

	// Helper to detect zero crossings/edges of the input signal.
	bool bCurrent;

	// Counts number of samples since last edge
	int32 Clock;

	// Estimates number of samples for a logical 1 in the LTC stream.
	int32 Cycles;

	// Indicates that we're expecting the second half of a logical 1.
	bool bFlip;

	// Keeps track of largest frame value received, helps estimate FrameRate.
	int32 FrameMax;

	// Tries to infer FrameRate based on frame value rollovers.
	int32 FrameRate;

	// Holds the stream to be decoded as bits come in.
	uint16 TimecodeBits[6];

public:
	// Minimum allowable samples per edge in the signal. Helps filter out noise.
	int32 MinSamplesPerEdge;

	// Maximum allowable samples per edge in the signal. Helps filter out noise.
	int32 MaxSamplesPerEdge;
};

