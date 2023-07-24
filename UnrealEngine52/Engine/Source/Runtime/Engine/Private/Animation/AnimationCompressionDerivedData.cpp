// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimationCompressionDerivedData.h"

#include "AnimationUtils.h"
#include "Animation/AnimBoneCompressionSettings.h"
#include "Experimental/Misc/ExecutionResource.h"
#include "Animation/AnimationSequenceCompiler.h"
#include "Animation/AnimCompress.h"
#include "DerivedDataCache.h"
#include "Animation/Skeleton.h"
#include "Interfaces/ITargetPlatform.h"

namespace UE::Anim
{	
#if ENABLE_COOK_STATS
	namespace AnimSequenceCookStats
	{
		FDerivedDataUsageStats UsageStats;
		static FCookStatsManager::FAutoRegisterCallback RegisterCookStats([](FCookStatsManager::AddStatFuncRef AddStat)
		{
			UsageStats.LogStats(AddStat, TEXT("AnimSequence.Usage"), TEXT(""));
		});
	}
#endif
	
#if WITH_EDITOR
FAnimationSequenceAsyncCacheTask::FAnimationSequenceAsyncCacheTask(const FIoHash& InKeyHash, FCompressibleAnimPtr InCompressibleAnimPtr, FCompressedAnimSequence* InCompressedData,
	UAnimSequence& InAnimSequence, const ITargetPlatform* InTargetPlatform)
	: Owner(UE::DerivedData::EPriority::Highest)
		, CompressedData(InCompressedData)
		, WeakAnimSequence(&InAnimSequence)
		, CompressibleAnimPtr(InCompressibleAnimPtr)
		, TargetPlatform(InTargetPlatform)
{
	check(!InAnimSequence.IsUnreachable());
	check(!InAnimSequence.GetSkeleton()->IsUnreachable());
		
	BeginCache(InKeyHash);
}

FAnimationSequenceAsyncCacheTask::~FAnimationSequenceAsyncCacheTask()
{
	if (!Poll())
	{
		Cancel();
		Wait(false);
	}
}

void FAnimationSequenceAsyncCacheTask::Cancel()
{
	if (BuildTask)
	{
		BuildTask->Cancel();
	}

	Owner.Cancel();
}

void FAnimationSequenceAsyncCacheTask::Wait(bool bPerformWork /*= true*/)
{
	if (BuildTask != nullptr)
	{
		BuildTask->EnsureCompletion(bPerformWork);
	}

	Owner.Wait();
}

bool FAnimationSequenceAsyncCacheTask::Poll() const
{
	if (BuildTask && !BuildTask->IsDone())
	{
		return false;
	}

	return Owner.Poll();
}

void FAnimationSequenceAsyncCacheTask::Reschedule(FQueuedThreadPool* InThreadPool, EQueuedWorkPriority InPriority) const
{
	if (BuildTask)
	{
		BuildTask->Reschedule(InThreadPool, InPriority);
	}
}

void FAnimationSequenceAsyncCacheTask::BeginCache(const FIoHash& KeyHash)
{
	using namespace UE::DerivedData;
	
	if (const UAnimSequence* AnimSequence = WeakAnimSequence.Get())
	{
		FQueuedThreadPool* ThreadPool = Anim::FAnimSequenceCompilingManager::Get().GetThreadPool();
		const EQueuedWorkPriority BasePriority = Anim::FAnimSequenceCompilingManager::Get().GetBasePriority(AnimSequence);
		
		const int64 AdditiveAnimSize = !AnimSequence->IsValidAdditive() ? 0 : [AnimSequence]()
		{
			if (const UAnimSequence* RefPoseSeq = AnimSequence->RefPoseSeq)
			{
				return RefPoseSeq->GetApproxRawSize();
			}

			return AnimSequence->GetApproxRawSize();
		}();
		
		const int64 RequiredMemory = AnimSequence->GetApproxRawSize() + AdditiveAnimSize;
		if (Compression::FAnimationCompressionMemorySummaryScope::ShouldStoreCompressionResults())
		{
			Compression::FAnimationCompressionMemorySummaryScope::CompressionResultSummary().GatherPreCompressionStats(AnimSequence->GetApproxRawSize(), AnimSequence->GetApproxCompressedSize());
		}
		
		CompressionStartTime = FPlatformTime::Seconds();

		check(BuildTask == nullptr);
		BuildTask = MakeUnique<FAnimationSequenceAsyncBuildTask>(this, KeyHash);
		BuildTask->StartBackgroundTask(ThreadPool, BasePriority, EQueuedWorkFlags::DoNotRunInsideBusyWait, RequiredMemory);
	}
}
	
void FAnimationSequenceAsyncCacheTask::EndCache(UE::DerivedData::FCacheGetValueResponse&& Response)
{
	using namespace UE::DerivedData;
	
	if (Response.Status == EStatus::Ok)
	{
		Owner.LaunchTask(TEXT("AnimationSequenceSerialize"), [this, Value = MoveTemp(Response.Value)]
		{
			// Release execution resource as soon as the task is done
			ON_SCOPE_EXIT { ExecutionResource = nullptr; };
			
			if (UAnimSequence* AnimSequence = WeakAnimSequence.Get())
			{
				COOK_STAT(auto Timer = AnimSequenceCookStats::UsageStats.TimeSyncWork());
				
				const FSharedBuffer RecordData = Value.GetData().Decompress();
				FMemoryReaderView Ar(RecordData, /*bIsPersistent*/ true);
				CompressedData->SerializeCompressedData(Ar, true, AnimSequence, AnimSequence->GetSkeleton(), CompressibleAnimPtr->BoneCompressionSettings, CompressibleAnimPtr->CurveCompressionSettings);

				UE_LOG(LogAnimationCompression, Display, TEXT("Fetched compressed animation data for %s"), *CompressibleAnimPtr->FullName);
				if (Compression::FAnimationCompressionMemorySummaryScope::ShouldStoreCompressionResults())
				{
					const double CompressionEndTime = FPlatformTime::Seconds();
					const double CompressionTime = CompressionEndTime - CompressionStartTime;

					TArray<FBoneData> BoneData;
					FAnimationUtils::BuildSkeletonMetaData(AnimSequence->GetSkeleton(), BoneData);
					Compression::FAnimationCompressionMemorySummaryScope::CompressionResultSummary().GatherPostCompressionStats(*CompressedData, BoneData, AnimSequence->GetFName(), CompressionTime,false);
				}
				
				COOK_STAT(Timer.AddHit(int64(Ar.TotalSize())));
			}
		});
	}
	else if (Response.Status == EStatus::Error)
	{
		Owner.LaunchTask(TEXT("AnimationSequenceCompression"), [this, Name = Response.Name, Key = Response.Key]
		{
			COOK_STAT(auto Timer = AnimSequenceCookStats::UsageStats.TimeSyncWork());
			
			// Release execution resource as soon as the task is done
			ON_SCOPE_EXIT { ExecutionResource = nullptr; };

			if (!BuildData())
			{
				return;
			}
			
			if (const UAnimSequence* AnimSequence = WeakAnimSequence.Get())
			{
				TArray64<uint8> RecordData;
				FMemoryWriter64 Ar(RecordData, /*bIsPersistent*/ true);
				CompressedData->SerializeCompressedData(Ar, true, nullptr, nullptr, CompressibleAnimPtr->BoneCompressionSettings, CompressibleAnimPtr->CurveCompressionSettings);

				UE_LOG(LogAnimationCompression, Display, TEXT("Storing compressed animation data for %s, at %s/%s"), *Name, *FString(Key.Bucket.ToString()), *LexToString(Key.Hash));
				GetCache().PutValue({ {Name, Key, FValue::Compress(MakeSharedBufferFromArray(MoveTemp(RecordData)))} }, Owner);
				
				if (Compression::FAnimationCompressionMemorySummaryScope::ShouldStoreCompressionResults())
				{
					const double CompressionEndTime = FPlatformTime::Seconds();
					const double CompressionTime = CompressionEndTime - CompressionStartTime;
					Compression::FAnimationCompressionMemorySummaryScope::CompressionResultSummary().GatherPostCompressionStats(*CompressedData, CompressibleAnimPtr->BoneData, AnimSequence->GetFName(), CompressionTime, true);
				}
				
				COOK_STAT(Timer.AddMiss(int64(Ar.Tell())));
			}
		});
	}
	else
	{
		// Release execution resource as soon as the task is done
		ExecutionResource = nullptr;
	}
}

bool FAnimationSequenceAsyncCacheTask::BuildData() const
{	
	// This is where we should do the compression parts
	TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*(FString(TEXT("FAnimationSequenceAsyncCacheTask::BuildData ") + CompressibleAnimPtr->Name)));
	UE_LOG(LogAnimationCompression, Display, TEXT("Building compressed animation data for %s"), *CompressibleAnimPtr->FullName);

	check(CompressibleAnimPtr.IsValid());
	FCompressibleAnimData& DataToCompress = *CompressibleAnimPtr.Get();
	FCompressedAnimSequence& OutData = *CompressedData;

	if (Owner.IsCanceled())
	{
		return false;
	}
	
	FCompressibleAnimDataResult CompressionResult;
	DataToCompress.FetchData(TargetPlatform);
	DataToCompress.Update(OutData);
		
	const bool bBoneCompressionOk = FAnimationUtils::CompressAnimBones(DataToCompress, CompressionResult);
	if (Owner.IsCanceled())
	{
		return false;
	}
	const bool bCurveCompressionOk = FAnimationUtils::CompressAnimCurves(DataToCompress, OutData);
				
	const bool bCompressionSuccessful = bBoneCompressionOk && bCurveCompressionOk;
	const FString CompressionName = DataToCompress.BoneCompressionSettings->GetFullName();
	
	if (bCompressionSuccessful && !Owner.IsCanceled())
	{
		OutData.CompressedByteStream = MoveTemp(CompressionResult.CompressedByteStream);
		OutData.CompressedDataStructure = MoveTemp(CompressionResult.AnimData);
		OutData.BoneCompressionCodec = CompressionResult.Codec;
		OutData.CompressedRawData = DataToCompress.RawAnimationData;
		OutData.OwnerName = DataToCompress.AnimFName;
		
		return true;
	}
	else
	{
		UE_LOG(LogAnimationCompression, Error, TEXT("Failed to generate compressed animation data for %s with compression scheme %s for target platform %s"), *CompressibleAnimPtr->FullName, *CompressionName, *TargetPlatform->DisplayName().ToString());
	}
	
	return false;
}
	
int32 GSkipDDC = 0;
static FAutoConsoleVariableRef CVarSkipDDC(
	TEXT("a.SkipDDC"),
	GSkipDDC,
	TEXT("1 = Skip DDC during compression. 0 = Include DDC results during compression "));

void FAnimationSequenceAsyncBuildWorker::DoWork() const
{
	using namespace UE::DerivedData;
	if (const UAnimSequence* AnimSequence = Owner->WeakAnimSequence.Get())
	{
		// Grab any execution resources currently assigned to this worker so that we maintain
		// concurrency limit and memory pressure until the whole multi-step task is done.
		Owner->ExecutionResource = FExecutionResourceContext::Get();

		const ECachePolicy Policy = GSkipDDC ? ECachePolicy::None : ECachePolicy::Default;
		static const FCacheBucket Bucket("AnimationSequence");
		GetCache().GetValue({ {{AnimSequence->GetPathName()}, {Bucket, IoHash}, Policy} }, Owner->Owner,
			  [Task = Owner](FCacheGetValueResponse&& Response) { Task->EndCache(MoveTemp(Response)); });
	}
}
#endif // WITH_EDITOR
}
