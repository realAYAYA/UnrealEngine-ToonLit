// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITORONLY_DATA
#include "DerivedDataRequestOwner.h"
#include "Experimental/Misc/ExecutionResource.h"
#include "Async/AsyncWork.h"
#include "Animation/AnimCompressionTypes.h"
#include "IO/IoHash.h"
#endif // WITH_EDITORONLY_DATA

#include "ProfilingDebugging/CookStats.h"

struct FCompressedAnimSequence;
class UAnimSequence;
namespace UE::DerivedData
{
	struct FCacheGetValueResponse;
	struct FCacheKey;

	template <typename CharType> class TSharedString;
	using FSharedString = TSharedString<TCHAR>;
}

namespace UE::Anim
{
#if ENABLE_COOK_STATS
	namespace AnimSequenceCookStats
	{
		extern FDerivedDataUsageStats UsageStats;
	}
#endif

#if WITH_EDITOR
	class FAnimationSequenceAsyncCacheTask;

	class FAnimationSequenceAsyncBuildWorker : public FNonAbandonableTask
	{
		FAnimationSequenceAsyncCacheTask* Owner;
		FIoHash IoHash;
	public:
		FAnimationSequenceAsyncBuildWorker(
			FAnimationSequenceAsyncCacheTask* InOwner,
			const FIoHash& InIoHash)
			: Owner(InOwner)
			, IoHash(InIoHash)
		{
		}

		FORCEINLINE TStatId GetStatId() const
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(FAnimationSequenceAsyncBuildWorker, STATGROUP_ThreadPoolAsyncTasks);
		}

		void DoWork() const;
	};

	struct FAnimationSequenceAsyncBuildTask : public FAsyncTask<FAnimationSequenceAsyncBuildWorker>
	{
		FAnimationSequenceAsyncBuildTask(
			FAnimationSequenceAsyncCacheTask* InOwner,
			const FIoHash& InIoHash)
			: FAsyncTask<FAnimationSequenceAsyncBuildWorker>(InOwner, InIoHash)
		{
		}
	};
	
	class FAnimationSequenceAsyncCacheTask
	{
	public:
		FAnimationSequenceAsyncCacheTask(const FIoHash& InKeyHash,
			FCompressibleAnimPtr InCompressibleAnimPtr,
			FCompressedAnimSequence* InCompressedData,
			UAnimSequence& InAnimSequence,
			const ITargetPlatform* InTargetPlatform);

		~FAnimationSequenceAsyncCacheTask();

		void Cancel();
		void Wait(bool bPerformWork = true);
		bool Poll() const;
		void Reschedule(FQueuedThreadPool* InThreadPool, EQueuedWorkPriority InPriority) const;
		bool WasCancelled() const { return CompressibleAnimPtr->IsCancelled() || Owner.IsCanceled(); }
	private:
		void BeginCache(const FIoHash& KeyHash);
		void EndCache(UE::DerivedData::FCacheGetValueResponse&& Response);
		bool BuildData() const;
		void LaunchCompressionTask(const UE::DerivedData::FSharedString& Name, const UE::DerivedData::FCacheKey& Key);
		int64 GetRequiredMemoryEstimate() const;

	private:
		friend class FAnimationSequenceAsyncBuildWorker;
		UE::DerivedData::FRequestOwner Owner;

		TRefCountPtr<IExecutionResource> ExecutionResource;
		TUniquePtr<FAnimationSequenceAsyncBuildTask> BuildTask;
		FCompressedAnimSequence* CompressedData;
		TWeakObjectPtr<UAnimSequence> WeakAnimSequence;	
		FCompressibleAnimPtr CompressibleAnimPtr;
		const ITargetPlatform* TargetPlatform;		
		double CompressionStartTime;
		
	};
#endif
}