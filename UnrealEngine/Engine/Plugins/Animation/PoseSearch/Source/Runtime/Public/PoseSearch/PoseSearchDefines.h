// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/ParallelFor.h"
#include "Logging/LogMacros.h"
#include "Animation/AnimTrace.h"

// Enable this if object tracing is enabled, mimics animation tracing
#define UE_POSE_SEARCH_TRACE_ENABLED ANIM_TRACE_ENABLED

#ifndef UE_POSE_SEARCH_FORCE_SINGLE_THREAD
#define UE_POSE_SEARCH_FORCE_SINGLE_THREAD 0
#endif

#if UE_POSE_SEARCH_FORCE_SINGLE_THREAD
constexpr EParallelForFlags ParallelForFlags = EParallelForFlags::ForceSingleThread;
#else
constexpr EParallelForFlags ParallelForFlags = EParallelForFlags::None;
#endif // UE_POSE_SEARCH_FORCE_SINGLE_THREAD

DECLARE_LOG_CATEGORY_EXTERN(LogPoseSearch, Log, All);

namespace UE::PoseSearch
{
static constexpr int8 RootSchemaBoneIdx = 0;
static constexpr FBoneIndexType RootBoneIndexType = 0;
static constexpr FBoneIndexType ComponentSpaceIndexType = FBoneIndexType(-1);
static constexpr FBoneIndexType WorldSpaceIndexType = FBoneIndexType(-2);

static constexpr int32 PreallocatedRolesNum = 4;
static constexpr int32 PreallocatedCachedQueriesNum = 8;
static constexpr int32 PreallocatedCachedChannelDataNum = 64;
static constexpr float FiniteDelta = 1 / 60.0f; // Time delta used for computing pose derivatives
static constexpr int32 MaxNumberOfCollectedPoseCandidatesPerDatabase = 200;

template<uint32 Alignment = DEFAULT_ALIGNMENT> class TMemStackSetAllocator : public TSetAllocator<TSparseArrayAllocator<TMemStackAllocator<Alignment>, TMemStackAllocator<Alignment>>, TMemStackAllocator<Alignment>> {};

constexpr int32 TMax(int32 A, int32 B) { return (A > B ? A : B); }
template<typename ElementType> constexpr int32 TAlignOf() { return TMax(alignof(ElementType), 16); }
template<typename ElementType> using TAlignedArray = TArray<ElementType, TAlignedHeapAllocator<TAlignOf<ElementType>()>>;
template<typename ElementType> using TStackAlignedArray = TArray<ElementType, TMemStackAllocator<TAlignOf<ElementType>()>>;

#if ENABLE_ANIM_DEBUG

	template<bool bIsRead = 0>
	struct FDebugThreadSafetyChecker
	{
		// if any read is in progress Counter > 0
		// if any write is in progress Counter < 0
		explicit FDebugThreadSafetyChecker(FThreadSafeCounter& InCounter)
			: Counter(InCounter)
		{
			if (bIsRead)
			{
				const int32 PreviousCounter = Counter.Add(1);
				if (PreviousCounter < 0)
				{
					UE_LOG(LogPoseSearch, Error, TEXT("Non thread safe call! Read detected a Write in progress"));
				}
			}
			else
			{
				const int32 PreviousCounter = Counter.Set(-1);
				if (PreviousCounter > 0)
				{
					UE_LOG(LogPoseSearch, Error, TEXT("Non thread safe call! Write detected %d Read(s) in progress"), PreviousCounter);
				}
			}
		}

		~FDebugThreadSafetyChecker()
		{
			if (bIsRead)
			{
				const int32 PreviousCounter = Counter.Add(-1);
				if (PreviousCounter <= 0)
				{
					UE_LOG(LogPoseSearch, Error, TEXT("Non thread safe call! Read detected a Write in progress"));
				}
			}
			else
			{
				const int32 PreviousCounter = Counter.Set(0);
				if (PreviousCounter >= 0)
				{
					UE_LOG(LogPoseSearch, Error, TEXT("Non thread safe call! Write detected %d Read(s) in progress"), PreviousCounter);
				}
			}
		}

		FThreadSafeCounter& Counter;
	};

#define CheckThreadSafetyRead(ThreadSafeCounter) FDebugThreadSafetyChecker<1> DebugThreadSafetyChecker##ThreadSafeCounter(ThreadSafeCounter)
#define CheckThreadSafetyWrite(ThreadSafeCounter) FDebugThreadSafetyChecker DebugThreadSafetyChecker##ThreadSafeCounter(ThreadSafeCounter)

#else // ENABLE_ANIM_DEBUG

	#define CheckThreadSafetyRead(ThreadSafeCounter)
	#define CheckThreadSafetyWrite(ThreadSafeCounter)

#endif // ENABLE_ANIM_DEBUG

}



