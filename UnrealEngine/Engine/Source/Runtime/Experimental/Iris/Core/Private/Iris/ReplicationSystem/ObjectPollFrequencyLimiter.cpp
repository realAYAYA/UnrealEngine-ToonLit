// Copyright Epic Games, Inc. All Rights Reserved.

#include "ObjectPollFrequencyLimiter.h"
#include "Iris/Core/IrisProfiler.h"
#include "HAL/IConsoleManager.h"
#include "Math/VectorRegister.h"

namespace UE
{
namespace Net
{
namespace Private
{

// Storing poll frame period as uint8
const int MaxPollFramePeriod = 256;
float PollFrequencyMultiplier = 1.0f;

static FAutoConsoleVariableRef CVarPollFrequencyMultiplier(
		TEXT("net.Iris.PollFrequencyMultiplier"),
		PollFrequencyMultiplier,
		TEXT("Multiplied with the NetUpdateFrequency to decide how often PreReplication is called on an Actor. Default factor is 1.0.")
		);

FObjectPollFrequencyLimiter::FObjectPollFrequencyLimiter()
{
}

void FObjectPollFrequencyLimiter::Init(uint32 MaxActiveObjectCount)
{
	// We want to be able to easily update 32 objects at a time.
	const uint32 StorageObjectCount = (MaxActiveObjectCount + 31U) & ~(MaxActiveObjectCount - 1U);

	// Allocate max amount of memory required.
	FramesBetweenUpdates.SetNumZeroed(StorageObjectCount);
	FrameCounters.SetNumZeroed(StorageObjectCount);
}

void FObjectPollFrequencyLimiter::Deinit()
{
	FramesBetweenUpdates.Empty();
	FrameCounters.Empty();
}

void FObjectPollFrequencyLimiter::Update(const FNetBitArrayView& ScopableObjects, const FNetBitArrayView& DirtyObjects, FNetBitArrayView& OutObjectsToPoll)
{
	IRIS_PROFILER_SCOPE(ObjectPollFrequencyLimiter_Update);

	typedef FNetBitArrayView::StorageWordType WordType;
	constexpr uint32 WordBitCount = FNetBitArrayView::WordBitCount;

	if (!MaxInternalHandle)
	{
		return;
	}

	++FrameIndex;

	uint8* CountersData = FrameCounters.GetData();
	const uint8* FramesBetweenUpdatesData = FramesBetweenUpdates.GetData();

	const WordType* ScopableObjectsData = ScopableObjects.GetData();
	const WordType* DirtyObjectsData = DirtyObjects.GetData();
	WordType* ObjectsToPollData = OutObjectsToPoll.GetData();

	// Decrease counters by one and if they wrap around it's time to poll.
	// Algorithm can easily be throttled to only allow at most N objects to be polled per frame.
#if PLATFORM_ENABLE_VECTORINTRINSICS == 1 && !PLATFORM_ENABLE_VECTORINTRINSICS_NEON
	{
		static_assert(WordBitCount == 32U, "Code assumes a NetBitArray word size of 32 bits.");

		const VectorRegisterInt AllBitsSet = _mm_cmpeq_epi32(_mm_setzero_si128(), _mm_setzero_si128());
		const VectorRegisterInt Zero = _mm_setzero_si128();
		const VectorRegisterInt One = _mm_set1_epi8(char(1));

		for (uint32 ObjectIndex = 0, ObjectEndIndex = MaxInternalHandle;
			ObjectIndex <= ObjectEndIndex; 
			ObjectIndex += WordBitCount, CountersData += WordBitCount, FramesBetweenUpdatesData += WordBitCount, ++ScopableObjectsData, ++DirtyObjectsData, ++ObjectsToPollData)
		{
			// Skip ranges with no scopable objects.
			const WordType ObjectsInScopeWord = *ScopableObjectsData;
			if (!ObjectsInScopeWord)
			{
				continue;
			}

			VectorRegisterInt Values0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(CountersData + 0U));
			VectorRegisterInt Values1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(CountersData + 16U));

			const VectorRegisterInt FramesBetweenUpdates0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(FramesBetweenUpdatesData + 0U));
			const VectorRegisterInt FramesBetweenUpdates1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(FramesBetweenUpdatesData + 16U));

			Values0 = _mm_sub_epi8(Values0, One);
			const VectorRegisterInt MaskToUpdate0 = _mm_cmpeq_epi8(Values0, AllBitsSet);
			Values0 = VectorIntSelect(MaskToUpdate0, FramesBetweenUpdates0, Values0);

			Values1 = _mm_sub_epi8(Values1, One);
			const VectorRegisterInt MaskToUpdate1 = _mm_cmpeq_epi8(Values1, AllBitsSet);
			Values1 = VectorIntSelect(MaskToUpdate1, FramesBetweenUpdates1, Values1);

			const uint32 ObjectsToPoll0 = _mm_movemask_epi8(MaskToUpdate0);
			const uint32 ObjectsToPoll1 = _mm_movemask_epi8(MaskToUpdate1);

			_mm_storeu_si128(reinterpret_cast<__m128i*>(CountersData + 0U), Values0);
			_mm_storeu_si128(reinterpret_cast<__m128i*>(CountersData + 16U), Values1);

			// Poll objects that have been set to dirty or are due.
			const WordType ObjectsToPoll = (ObjectsToPoll1 << 16U) | ObjectsToPoll0;
			const WordType DirtyObjectWord = *DirtyObjectsData;
			*ObjectsToPollData = (ObjectsToPoll | DirtyObjectWord) & ObjectsInScopeWord;
		}
	}
#else
	// Slower path using scalar integer instructions.
	{
		for (uint32 ObjectIndex = 0, ObjectEndIndex = MaxInternalHandle;
			ObjectIndex <= ObjectEndIndex;
			ObjectIndex += WordBitCount, CountersData += WordBitCount, FramesBetweenUpdatesData += WordBitCount, ++ScopableObjectsData, ++DirtyObjectsData, ++ObjectsToPollData)
		{
			// Skip ranges with no scopable objects.
			WordType ObjectsInScopeWord = *ScopableObjectsData;
			if (!ObjectsInScopeWord)
			{
				continue;
			}

			WordType ObjectsToPoll = 0;
			for (SIZE_T It = 0, IndexOffset = 0; It < 8; ++It, IndexOffset += 4U, ObjectsInScopeWord >>= 4U)
			{
				if (!(ObjectsInScopeWord & 15U))
				{
					continue;
				}

				uint8 Counter0 = CountersData[IndexOffset + 0];
				uint8 Counter1 = CountersData[IndexOffset + 1];
				uint8 Counter2 = CountersData[IndexOffset + 2];
				uint8 Counter3 = CountersData[IndexOffset + 3];

				const uint8 FramesBetweenUpdates0 = FramesBetweenUpdatesData[IndexOffset + 0];
				const uint8 FramesBetweenUpdates1 = FramesBetweenUpdatesData[IndexOffset + 1];
				const uint8 FramesBetweenUpdates2 = FramesBetweenUpdatesData[IndexOffset + 2];
				const uint8 FramesBetweenUpdates3 = FramesBetweenUpdatesData[IndexOffset + 3];

				--Counter0;
				--Counter1;
				--Counter2;
				--Counter3;

				const uint32 Mask0 = Counter0 == 255 ? 1 : 0;
				const uint32 Mask1 = Counter1 == 255 ? 1 : 0;
				const uint32 Mask2 = Counter2 == 255 ? 1 : 0;
				const uint32 Mask3 = Counter3 == 255 ? 1 : 0;

				Counter0 = Mask0 ? FramesBetweenUpdates0 : Counter0;
				Counter1 = Mask1 ? FramesBetweenUpdates1 : Counter1;
				Counter2 = Mask2 ? FramesBetweenUpdates2 : Counter2;
				Counter3 = Mask3 ? FramesBetweenUpdates3 : Counter3;

				const uint32 Mask = (Mask3 << 3U) | (Mask2 << 2U) | (Mask1 << 1U) | Mask0;
				ObjectsToPoll |= (Mask << IndexOffset);

				CountersData[IndexOffset + 0] = Counter0;
				CountersData[IndexOffset + 1] = Counter1;
				CountersData[IndexOffset + 2] = Counter2;
				CountersData[IndexOffset + 3] = Counter3;
			}

			const WordType DirtyObjectWord = *DirtyObjectsData;
			*ObjectsToPollData = (ObjectsToPoll | DirtyObjectWord) & ObjectsInScopeWord;
		}
	}
#endif
}

}
}
}
