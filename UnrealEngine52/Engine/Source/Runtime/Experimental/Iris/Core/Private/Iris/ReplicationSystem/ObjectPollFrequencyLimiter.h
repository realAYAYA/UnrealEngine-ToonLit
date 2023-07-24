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

extern IRISCORE_API float PollFrequencyMultiplier;
extern IRISCORE_API const int MaxPollFramePeriod;

class FObjectPollFrequencyLimiter
{
public:
	FObjectPollFrequencyLimiter();

	void Init(uint32 MaxActiveObjectCount);
	void Deinit();

	void SetPollFramePeriod(FInternalNetRefIndex InternalIndex, uint8 PollFramePeriod);

	void SetPollWithObject(FInternalNetRefIndex ObjectToPollWithInternalIndex, FInternalNetRefIndex InternalIndex);

	void Update(const FNetBitArrayView& ScopableObjects, const FNetBitArrayView& DirtyObjects, FNetBitArrayView& OutObjectsToPoll);

private:
	uint32 GetPollFramePeriodForFrequency(float PollFrequency) const;

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
	// Spread the polling of objects with the same frequency so that if you add lots of objects the same frame they won't be polled at the same time.
	const uint32 FrameOffset = FrameIndexOffsets[PollFramePeriod]++;
	FrameCounters[InternalIndex] = FrameOffset % (uint32(PollFramePeriod) + 1U);
}

inline void FObjectPollFrequencyLimiter::SetPollWithObject(FInternalNetRefIndex ObjectToPollWithInternalIndex, FInternalNetRefIndex InternalIndex)
{
	MaxInternalHandle = FPlatformMath::Max(MaxInternalHandle, InternalIndex);

	// Copy state from object to poll with
	FramesBetweenUpdates[InternalIndex] = FramesBetweenUpdates[ObjectToPollWithInternalIndex];
	FrameCounters[InternalIndex] = FrameCounters[ObjectToPollWithInternalIndex];
}

}
