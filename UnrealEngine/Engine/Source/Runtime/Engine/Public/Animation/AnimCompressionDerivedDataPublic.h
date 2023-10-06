// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "Animation/AnimCompressionTypes.h"
#include "TickableEditorObject.h"
#include "UObject/GCObject.h"

class UAnimSequence;
class FDerivedDataAnimationCompression;
struct FCompressibleAnimData;

UE_DEPRECATED(5.2, "GAsyncCompressedAnimationsTracker has been deprecated")
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
class UE_DEPRECATED(5.2, "FAsyncCompressedAnimationsManagement has been deprecated") FAsyncCompressedAnimationsManagement;
class FAsyncCompressedAnimationsManagement
{
public:
	static FAsyncCompressedAnimationsManagement& Get()
	{
		static FAsyncCompressedAnimationsManagement Management;
		return Management;
	}

	// Request an async compression of an animation, may not actually run async if memory usage is already high
	// Returns true if an async compression task was allowed.
	UE_DEPRECATED(5.2, "RequestAsyncCompression has been deprecated")
	bool RequestAsyncCompression(FDerivedDataAnimationCompression& Compressor, UAnimSequence* Anim, const bool bPerformFrameStripping, TArray<uint8>& OutData) { return false; }

	// Returns the number of remaining compression jobs (used for UI)
	UE_DEPRECATED(5.2, "GetNumRemainingJobs has been deprecated")
	int32 GetNumRemainingJobs() const { return 0; }
	
	// Blocks on compression for the supplied animation. Returns false if there was no compression job to wait on.
	UE_DEPRECATED(5.2, "WaitOnExistingCompression has been deprecated") 
	bool WaitOnExistingCompression(UAnimSequence* Anim, const bool bWantResults) { return false; }
};

#endif
