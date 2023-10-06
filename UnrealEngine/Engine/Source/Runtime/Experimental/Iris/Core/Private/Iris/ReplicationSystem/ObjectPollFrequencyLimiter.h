// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Net/Core/NetBitArray.h"

namespace UE::Net::Private
{
	typedef uint32 FInternalNetRefIndex;
}

namespace UE::Net::Private
{

class FObjectPollFrequencyLimiter
{
public:
	FObjectPollFrequencyLimiter();

	void Init(uint32 MaxActiveObjectCount);
	void Deinit();

	void SetPollFramePeriod(FInternalNetRefIndex InternalIndex, uint8 PollFramePeriod);

	void SetPollWithObject(FInternalNetRefIndex ObjectToPollWithInternalIndex, FInternalNetRefIndex InternalIndex);

	/** 
	* Produces the list of objects that should be polled this frame.
	* This list is composed of relevant objects that are dirty or that hit their poll period this frame.
	*/
	void Update(const FNetBitArrayView& RelevantObjects, const FNetBitArrayView& DirtyObjects, FNetBitArrayView& OutObjectsToPoll);

	/** We use a uint8 to track frames, so the limit is 255 frames.*/
	static constexpr uint32 GetMaxPollingFrames()
	{		
		return static_cast<uint32>(std::numeric_limits<uint8>::max());
	}

private:

	uint32 MaxInternalHandle = 0;
	uint32 FrameIndex = 0;
	// We store the number of frames between updates as a byte to be able to process 16 objects per instruction.
	// This limits polling to at least every 256th frame. At 30Hz this means every 8.5 seconds.
	uint8 FrameIndexOffsets[256] = {};
	TArray<uint8> FramesBetweenUpdates;
	TArray<uint8> FrameCounters;
};


inline void FObjectPollFrequencyLimiter::SetPollFramePeriod(FInternalNetRefIndex InternalIndex, uint8 PollFramePeriod)
{
	MaxInternalHandle = FPlatformMath::Max(MaxInternalHandle, InternalIndex);

	FramesBetweenUpdates[InternalIndex] = PollFramePeriod;
	// Spread the polling of objects with the same frequency so that if you add lots of objects the same frame they won't be polled at the same time. The update loop decrements counters so we need to be careful with how we offset things.
	const uint8 FrameOffset = --FrameIndexOffsets[PollFramePeriod];
	FrameCounters[InternalIndex] = static_cast<uint8>(uint32(~(FrameIndex + FrameOffset)) % uint32(PollFramePeriod + 1U));
}

inline void FObjectPollFrequencyLimiter::SetPollWithObject(FInternalNetRefIndex ObjectToPollWithInternalIndex, FInternalNetRefIndex InternalIndex)
{
	MaxInternalHandle = FPlatformMath::Max(MaxInternalHandle, InternalIndex);

	// Copy state from object to poll with
	FramesBetweenUpdates[InternalIndex] = FramesBetweenUpdates[ObjectToPollWithInternalIndex];
	FrameCounters[InternalIndex] = FrameCounters[ObjectToPollWithInternalIndex];
}

}
