// Copyright Epic Games, Inc. All Rights Reserved.
#include "Animation/AnimCompressionDerivedData.h"
#include "Animation/AnimCompressionDerivedDataPublic.h"
#include "CoreMinimal.h"
#include "DerivedDataCacheInterface.h"
#include "Stats/Stats.h"
#include "Animation/AnimSequence.h"
#include "Serialization/MemoryWriter.h"
#include "AnimationUtils.h"
#include "AnimEncoding.h"
#include "Animation/AnimCompress.h"
#include "Animation/AnimBoneCompressionSettings.h"
#include "Animation/AnimCurveCompressionSettings.h"
#include "AnimationCompression.h"
#include "UObject/Package.h"
#include "Misc/CoreDelegates.h"
#include "ProfilingDebugging/CookStats.h"

#if WITH_EDITOR

DECLARE_CYCLE_STAT(TEXT("Anim Compression (Derived Data)"), STAT_AnimCompressionDerivedData, STATGROUP_Anim);

FDerivedDataAnimationCompression::FDerivedDataAnimationCompression(const TCHAR* InTypeName, const FString& InAssetDDCKey, TSharedPtr<FAnimCompressContext> InCompressContext)
	: TypeName(InTypeName)
	, AssetDDCKey(InAssetDDCKey)
	, CompressContext(InCompressContext)
{

}

FDerivedDataAnimationCompression::~FDerivedDataAnimationCompression()
{
}

const TCHAR* FDerivedDataAnimationCompression::GetVersionString() const
{
	// This is a version string that mimics the old versioning scheme. If you
	// want to bump this version, generate a new guid using VS->Tools->Create GUID and
	// return it here. Ex.
	return TEXT("3A3F9FBB7A4047278CABE35820CC44D7");
}

FString FDerivedDataAnimationCompression::GetDebugContextString() const
{
	check(DataToCompressPtr.IsValid());
	return DataToCompressPtr->FullName;
}

bool FDerivedDataAnimationCompression::Build( TArray<uint8>& OutDataArray )
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FDerivedDataAnimationCompression::Build);

	const double CompressionStartTime = FPlatformTime::Seconds();

	check(DataToCompressPtr.IsValid());
	FCompressibleAnimData& DataToCompress = *DataToCompressPtr.Get();
	FCompressedAnimSequence OutData;

	if (DataToCompress.IsCancelled())
	{
		return false;
	}

	// Update UsageStats only when not running on the game thread since that thread times this at a higher level.
	COOK_STAT(auto Timer = AnimSequenceCookStats::UsageStats.TimeSyncWork());
	COOK_STAT(IsInGameThread() ? Timer.Cancel() : Timer.TrackCyclesOnly());

	SCOPE_CYCLE_COUNTER(STAT_AnimCompressionDerivedData);
	UE_LOG(LogAnimationCompression, Log, TEXT("Building Anim DDC data for %s"), *DataToCompress.FullName);

	FCompressibleAnimDataResult CompressionResult;

	bool bCompressionSuccessful = false;
	{
		DataToCompress.Update(OutData);

		const bool bBoneCompressionOk = FAnimationUtils::CompressAnimBones(DataToCompress, CompressionResult);
		if (DataToCompress.IsCancelled())
		{
			return false;
		}
		const bool bCurveCompressionOk = FAnimationUtils::CompressAnimCurves(DataToCompress, OutData);

#if DO_CHECK
		FString CompressionName = DataToCompress.BoneCompressionSettings->GetFullName();
		const TCHAR* AAC = CompressContext.Get()->bAllowAlternateCompressor ? TEXT("true") : TEXT("false");
		const TCHAR* OutputStr = CompressContext.Get()->bOutput ? TEXT("true") : TEXT("false");
#endif

		bCompressionSuccessful = bBoneCompressionOk && bCurveCompressionOk;

		ensureMsgf(bCompressionSuccessful, TEXT("Anim Compression failed for Sequence '%s' with compression scheme '%s': compressed data empty\n\tAnimIndex: %i\n\tMaxAnim:%i\n\tAllowAltCompressor:%s\n\tOutput:%s"), 
											*DataToCompress.FullName,
											*CompressionName,
											CompressContext.Get()->AnimIndex,
											CompressContext.Get()->MaxAnimations,
											AAC,
											OutputStr);
	}

	bCompressionSuccessful = bCompressionSuccessful && !DataToCompress.IsCancelled();

	if (bCompressionSuccessful)
	{
		const double CompressionEndTime = FPlatformTime::Seconds();
		const double CompressionTime = CompressionEndTime - CompressionStartTime;

		CompressContext->GatherPostCompressionStats(OutData, DataToCompress.BoneData, DataToCompress.AnimFName, CompressionTime, true);

		OutData.CompressedByteStream = MoveTemp(CompressionResult.CompressedByteStream);
		OutData.CompressedDataStructure = MoveTemp(CompressionResult.AnimData);
		OutData.BoneCompressionCodec = CompressionResult.Codec;

		FMemoryWriter Ar(OutDataArray, true);
		OutData.CompressedRawData = DataToCompress.RawAnimationData;
		OutData.OwnerName = DataToCompress.AnimFName;
		OutData.SerializeCompressedData(Ar, true, nullptr, nullptr, DataToCompress.BoneCompressionSettings, DataToCompress.CurveCompressionSettings); //Save out compressed
	}

	return bCompressionSuccessful;
}

const uint64 GigaBytes = 1024 * 1024 * 1024;
const uint64 MAX_ASYNC_COMPRESSION_MEM_USAGE = 3 * GigaBytes;
const int32 MAX_ACTIVE_COMPRESSIONS = 2;

FAsyncCompressedAnimationsManagement* GAsyncCompressedAnimationsTracker = nullptr;

FAsyncCompressedAnimationsManagement& FAsyncCompressedAnimationsManagement::Get()
{
	static FAsyncCompressedAnimationsManagement SingletonInstance;
	GAsyncCompressedAnimationsTracker = &SingletonInstance;
	return SingletonInstance;
}

FAsyncCompressedAnimationsManagement::FAsyncCompressedAnimationsManagement() : ActiveMemoryUsage(0), LastTickTime(0.0)
{
	FCoreDelegates::OnPreExit.AddRaw(this, &FAsyncCompressedAnimationsManagement::Shutdown);
}

void FAsyncCompressedAnimationsManagement::Shutdown()
{
	TArray<uint8> OutData; // For canceling

	for (FActiveAsyncCompressionTask& ActiveTask : ActiveAsyncCompressionTasks)
	{
		ActiveTask.DataToCompress->IsCancelledSignal.Cancel();
		if (ActiveTask.Sequence)
		{
			ActiveTask.Sequence->ApplyCompressedData(FString(), false, OutData); // Clear active compression on Sequence
			ActiveTask.Sequence = nullptr;
		}
	}

	for (const FQueuedAsyncCompressionWork& QueuedTask : QueuedAsyncCompressionWork)
	{
		if (QueuedTask.Anim)
		{
			QueuedTask.Anim->ApplyCompressedData(FString(), false, OutData); // Clear active compression on Sequence
		}
	}

	QueuedAsyncCompressionWork.Reset();

	const double PreExitStartTimer = FPlatformTime::Seconds();
	while(ActiveAsyncCompressionTasks.Num() > 0)
	{
		Tick(0.f); //Give active tasks that have already started a chance to finish
		
		const double PreExitDuration = FPlatformTime::Seconds() - PreExitStartTimer;

		if (PreExitDuration > 20.0)
		{
			UE_LOG(LogAnimationCompression, Warning, TEXT("Async Compression Pre Init Timer hit, Active Compressions:%i"), ActiveAsyncCompressionTasks.Num());
			return;
		}
		FPlatformProcess::Sleep(1.f);
	} 
}

void FAsyncCompressedAnimationsManagement::OnActiveCompressionFinished(int32 ActiveAnimIndex)
{
	COOK_STAT(auto Timer = AnimSequenceCookStats::UsageStats.TimeSyncWork());

	FDerivedDataCacheInterface& DerivedDataCache = GetDerivedDataCacheRef();

	FActiveAsyncCompressionTask& Task = ActiveAsyncCompressionTasks[ActiveAnimIndex];

	TArray<uint8> OutData;
	bool bBuiltLocally = false;
	const bool bOk = DerivedDataCache.GetAsynchronousResults(Task.AsyncHandle, OutData, &bBuiltLocally);

	COOK_STAT(using EHitOrMiss = FCookStats::CallStats::EHitOrMiss);
	COOK_STAT(Timer.AddHitOrMiss(bBuiltLocally ? EHitOrMiss::Miss : EHitOrMiss::Hit, OutData.Num()));

	if (Task.Sequence)
	{
		if (bOk)
		{
			Task.Sequence->ApplyCompressedData(Task.CacheKey, Task.bPerformFrameStripping, OutData);
		}
		else
		{
			UE_LOG(LogAnimationCompression, Fatal, TEXT("Failed to get async compressed animation data for anim '%s'"), *Task.Sequence->GetName());
			Task.Sequence->ApplyCompressedData(FString(), false, OutData); // Clear active compression on Sequence
		}
	}

	ActiveMemoryUsage -= Task.TaskSize;
	ActiveAsyncCompressionTasks.RemoveAtSwap(ActiveAnimIndex, 1, false);
}

float FormatBytes(uint64 Bytes, const TCHAR*& OutPostFix)
{
	uint64 Size = GigaBytes;

	if (Bytes > Size)
	{
		OutPostFix = TEXT("GB");
		return (float)Bytes / (float)Size;
	}

	Size = Size / 1024;
	if (Bytes > Size)
	{
		OutPostFix = TEXT("MB");
		return (float)Bytes / (float)Size;
	}

	Size = Size / 1024;
	if (Bytes > Size)
	{
		OutPostFix = TEXT("KB");
		return (float)Bytes / (float)Size;
	}

	OutPostFix = TEXT("B");
	return (float)Bytes;
}

#define ASYNC_MEM_LOG 0

void FAsyncCompressedAnimationsManagement::Tick(float DeltaTime)
{
	const double MaxProcessingTime = 0.1; // try not to hang the editor too much
	const double EndTime = FPlatformTime::Seconds() + MaxProcessingTime;
	const double StartTime = FPlatformTime::Seconds();

#if ASYNC_MEM_LOG
	const TCHAR* BytesPostFix = TEXT("ERROR");
	const float FormatSize = FormatBytes(ActiveMemoryUsage, BytesPostFix);
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FAsyncCompressedAnimationsManagement::Tick\n\tDelta:%.2f TimeSinceLastRun:%.2f\n\tActive:%i Queued:%i\n\tMemUsage:%.2f%s\n"), DeltaTime, StartTime - LastTickTime, ActiveAsyncCompressionTasks.Num(), QueuedAsyncCompressionWork.Num(), FormatSize, BytesPostFix);
#endif

	LastTickTime = StartTime;

	FDerivedDataCacheInterface& DerivedDataCache = GetDerivedDataCacheRef();

	for (int32 ActiveAnim = ActiveAsyncCompressionTasks.Num() - 1; ActiveAnim >= 0; --ActiveAnim)
	{
		const FActiveAsyncCompressionTask& Task = ActiveAsyncCompressionTasks[ActiveAnim];

		if (DerivedDataCache.PollAsynchronousCompletion(Task.AsyncHandle))
		{
			OnActiveCompressionFinished(ActiveAnim);
		}

		if (FPlatformTime::Seconds() > EndTime)
		{
			return; // Finish for this tick
		}
	}

	const bool bHasQueuedTasks = QueuedAsyncCompressionWork.Num() > 0;

	StartQueuedTasks(MAX_ACTIVE_COMPRESSIONS);

	if (bHasQueuedTasks && QueuedAsyncCompressionWork.Num() == 0)
	{
		QueuedAsyncCompressionWork.Empty(); //free memory
	}
}

void FAsyncCompressedAnimationsManagement::TickCook(float DeltaTime, bool bCookCompete)
{
	Tick(DeltaTime);
}

void FAsyncCompressedAnimationsManagement::StartQueuedTasks(int32 MaxActiveTasks)
{
	while (ActiveAsyncCompressionTasks.Num() < MaxActiveTasks)
	{
		if (QueuedAsyncCompressionWork.Num() == 0)
		{
			break;
		}
		FQueuedAsyncCompressionWork NewTask = QueuedAsyncCompressionWork.Pop(false);
		StartAsyncWork(NewTask.Compressor, NewTask.Anim, NewTask.Compressor.GetMemoryUsage(), NewTask.bPerformFrameStripping);
	}
}

void FAsyncCompressedAnimationsManagement::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (const FActiveAsyncCompressionTask& Task : ActiveAsyncCompressionTasks)
	{
		Task.DataToCompress->AddReferencedObjects(Collector);
	}

	for (const FQueuedAsyncCompressionWork& QueuedTask : QueuedAsyncCompressionWork)
	{
		QueuedTask.Compressor.GetCompressibleData()->AddReferencedObjects(Collector);
	}
}

TStatId FAsyncCompressedAnimationsManagement::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncCompressedAnimationsTracker, STATGROUP_Tickables);
}

bool FAsyncCompressedAnimationsManagement::RequestAsyncCompression(FDerivedDataAnimationCompression& Compressor, UAnimSequence* Anim, const bool bPerformFrameStripping, TArray<uint8>& OutData)
{
	const uint64 NewTaskSize = Compressor.GetMemoryUsage();

	bool bWasAsync = true;

	const bool bOutOfMemForQueue = (ActiveMemoryUsage + NewTaskSize) >= MAX_ASYNC_COMPRESSION_MEM_USAGE;
	const bool bGameThreadStarved = FPlatformTime::Seconds() - LastTickTime > 2.f;

	//Boost Max active compressions in the case of game thread not running for extended periods (assumes heavy loading situation)
	// so that memory usage of async compressions does not grow too large
	const int32 MaxActiveCompressions = bGameThreadStarved ? (MAX_ACTIVE_COMPRESSIONS * 6) : MAX_ACTIVE_COMPRESSIONS;

	if (bOutOfMemForQueue || bGameThreadStarved)
	{
		Tick(0.f); // Try to free up some memory
	}

	const bool bCanRunASync = (ActiveMemoryUsage + NewTaskSize < MAX_ASYNC_COMPRESSION_MEM_USAGE);
	const bool bForceSync = false; //debugging override


	if (bCanRunASync && !bForceSync)
	{
		//Queue Async
		ActiveMemoryUsage += NewTaskSize;

		if(ActiveAsyncCompressionTasks.Num() < MaxActiveCompressions)
		{
			StartAsyncWork(Compressor, Anim, NewTaskSize, bPerformFrameStripping);
		}
		else
		{
			QueuedAsyncCompressionWork.Emplace(Compressor, Anim, bPerformFrameStripping);
		}
	}
	else
	{
		//Do in place
		GetDerivedDataCacheRef().GetSynchronous(&Compressor, OutData);
		bWasAsync = false;
	}

	StartQueuedTasks(MaxActiveCompressions);

	return bWasAsync;
}

void FAsyncCompressedAnimationsManagement::StartAsyncWork(FDerivedDataAnimationCompression& Compressor, UAnimSequence* Anim, const uint64 NewTaskSize, const bool bPerformFrameStripping)
{
	const FString CacheKey = Compressor.GetPluginSpecificCacheKeySuffix();
	FCompressibleAnimPtr SourceData = Compressor.GetCompressibleData();
	uint32 AsyncHandle = GetDerivedDataCacheRef().GetAsynchronous(&Compressor);
	ActiveAsyncCompressionTasks.Emplace(Anim, SourceData, CacheKey, NewTaskSize, AsyncHandle, bPerformFrameStripping);
}

bool FAsyncCompressedAnimationsManagement::WaitOnActiveCompression(UAnimSequence* Anim, bool bWantResults)
{
	for (int32 ActiveIndex = 0; ActiveIndex < ActiveAsyncCompressionTasks.Num(); ++ActiveIndex)
	{
		FActiveAsyncCompressionTask& Task = ActiveAsyncCompressionTasks[ActiveIndex];
		if (Task.Sequence == Anim)
		{
			if(bWantResults)
			{
				GetDerivedDataCacheRef().WaitAsynchronousCompletion(Task.AsyncHandle);
				OnActiveCompressionFinished(ActiveIndex);
			}
			else
			{
				Task.DataToCompress->IsCancelledSignal.Cancel();
				Task.Sequence = nullptr;
			}
			return true; // Was active
		}
	}
	return false;
}

bool FAsyncCompressedAnimationsManagement::WaitOnExistingCompression(UAnimSequence* Anim, const bool bWantResults)
{
	if(!WaitOnActiveCompression(Anim, bWantResults))
	{
		//Check if we have a queued task
		for (int32 QueuedIndex = 0; QueuedIndex < QueuedAsyncCompressionWork.Num(); ++QueuedIndex)
		{
			FQueuedAsyncCompressionWork& Task = QueuedAsyncCompressionWork[QueuedIndex];
			if (Task.Anim == Anim)
			{
				if (bWantResults)
				{
					StartAsyncWork(Task.Compressor, Task.Anim, Task.Compressor.GetMemoryUsage(), Task.bPerformFrameStripping);
				}
				else
				{
					delete &Task.Compressor;
				}
				QueuedAsyncCompressionWork.RemoveAtSwap(QueuedIndex, 1, false);
			}
		}
		return bWantResults ? WaitOnActiveCompression(Anim, bWantResults) : false;
	}
	return true;
}

#endif	//WITH_EDITOR
