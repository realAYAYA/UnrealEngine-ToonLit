// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "Animation/AnimCompressionTypes.h"
#include "TickableEditorObject.h"
#include "UObject/GCObject.h"

class UAnimSequence;
class FDerivedDataAnimationCompression;
struct FCompressibleAnimData;


extern ENGINE_API class FAsyncCompressedAnimationsManagement* GAsyncCompressedAnimationsTracker;

// Animation data that is currently being compressed
struct FActiveAsyncCompressionTask
{
	FActiveAsyncCompressionTask(UAnimSequence* InSequence, FCompressibleAnimPtr InDataToCompress, const FString& InCacheKey, const uint64 InTaskSize, const uint32 InAsyncHandle, bool bInPerformFrameStripping)
		: Sequence(InSequence)
		, DataToCompress(InDataToCompress)
		, TaskSize(InTaskSize)
		, CacheKey(InCacheKey)
		, AsyncHandle(InAsyncHandle)
		, bPerformFrameStripping(bInPerformFrameStripping)
	{}
	UAnimSequence* Sequence;
	FCompressibleAnimPtr DataToCompress;
	uint64 TaskSize;
	FString CacheKey;
	uint32 AsyncHandle;
	bool bPerformFrameStripping;
};

// An animation waiting to be compressed
struct FQueuedAsyncCompressionWork
{
	FQueuedAsyncCompressionWork(FDerivedDataAnimationCompression& InCompressor, UAnimSequence* InAnim, const bool bInPerformFrameStripping)
		: Compressor(InCompressor)
		, Anim(InAnim)
		, bPerformFrameStripping(bInPerformFrameStripping)
	{}

	FDerivedDataAnimationCompression& Compressor;
	UAnimSequence* Anim;
	const bool bPerformFrameStripping;
};


// Manager for Async anim compression.
//   Maintains active compressions
//   tracks memory usage of async compression
//   Gives API for blocking on compression
class ENGINE_API FAsyncCompressedAnimationsManagement : public FTickableEditorObject, public FTickableCookObject, public FGCObject
{
public:
	static FAsyncCompressedAnimationsManagement& Get();

	// Request an async compression of an animation, may not actually run async if memory usage is already high
	// Returns true if an async compression task was allowed.
	bool RequestAsyncCompression(FDerivedDataAnimationCompression& Compressor, UAnimSequence* Anim, const bool bPerformFrameStripping, TArray<uint8>& OutData);

	// Returns the number of remaining compression jobs (used for UI)
	int32 GetNumRemainingJobs() const { return ActiveAsyncCompressionTasks.Num() + QueuedAsyncCompressionWork.Num(); }
	
	// Blocks on compression for the supplied animation. Returns false if there was no compression job to wait on. 
	bool WaitOnExistingCompression(UAnimSequence* Anim, const bool bWantResults);
private:

	void StartAsyncWork(FDerivedDataAnimationCompression& Compressor, UAnimSequence* Anim, const uint64 NewTaskSize, const bool bPerformFrameStripping);

	void StartQueuedTasks(int32 MaxActiveTasks);

	bool WaitOnActiveCompression(UAnimSequence* Anim, bool bWantResults);

	void OnActiveCompressionFinished(int32 ActiveAnimIndex);

	FAsyncCompressedAnimationsManagement();

	void Shutdown();

	//Array of active and queued compression jobs
	TArray<FActiveAsyncCompressionTask> ActiveAsyncCompressionTasks;
	TArray<FQueuedAsyncCompressionWork> QueuedAsyncCompressionWork;
	uint64 ActiveMemoryUsage;

	//Cache the last tick time to keep queue pumping during large loading segments
	double LastTickTime;

	/* Begin FTickableEditorObject */
	virtual void Tick(float DeltaTime) override;
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }
	virtual TStatId GetStatId() const override;
	/* End FTickableEditorObject */

	/* Begin FTickableCookObject */
	virtual void TickCook(float DeltaTime, bool bCookCompete) override;
	/* End FTickableCookObject */

	// FGCObject interface
	void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FAsyncCompressedAnimationsManagement");
	}
	// End of FGCObject interface
};

#endif
