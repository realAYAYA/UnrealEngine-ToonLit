// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IGeometryCacheStream.h"
#include <atomic>

struct FGeometryCacheStreamReadRequest;

/* Details about the animation to be streamed */
struct FGeometryCacheStreamDetails
{
	int32 NumFrames = 0;
	float Duration = 0.f;
	float SecondsPerFrame = 1.f / 24.f;
	int32 StartFrameIndex = 0;
	int32 EndFrameIndex = 0;
};

/**
 * Base class for GeometryCache stream for use with the GeometryCacheStreamer
 * Besides implementing the basic functionalities expected of the stream,
 * it implements basic memory statistics and management for use by the streamer.
 * Derived classes need to implement a way to retrieve the mesh data for a frame
 * through GetMeshData.
 */
class GEOMETRYCACHESTREAMER_API FGeometryCacheStreamBase : public IGeometryCacheStream
{
public:
	FGeometryCacheStreamBase(int32 ReadConcurrency, FGeometryCacheStreamDetails&& Details);
	virtual ~FGeometryCacheStreamBase();

	//~ Begin IGeometryCacheStream Interface
	virtual void Prefetch(int32 StartFrameIndex, int32 NumFrames = 0) override;
	virtual uint32 GetNumFramesNeeded() override;
	virtual bool RequestFrameData() override;
	virtual void UpdateRequestStatus(TArray<int32>& OutFramesCompleted) override;
	virtual bool GetFrameData(int32 FrameIndex, FGeometryCacheMeshData& OutMeshData) override;
	virtual int32 CancelRequests() override;
	virtual const FGeometryCacheStreamStats& GetStreamStats() const override;
	virtual void SetLimits(float MaxMemoryAllowed, float MaxCachedDuration) override;
	//~ End IGeometryCacheStream Interface

	/* Updates the current position in the stream */
	void UpdateCurrentFrameIndex(int32 FrameIndex);

protected:

	/* Derived class must provide a way to get the mesh data for the given FrameIndex */
	virtual void GetMeshData(int32 FrameIndex, int32 ReadConcurrencyIndex, FGeometryCacheMeshData& OutMeshData) = 0;

	void LoadFrameData(int32 FrameIndex);
	void UpdateFramesNeeded(int32 StartIndex, int32 NumFrames);
	void IncrementMemoryStat(const FGeometryCacheMeshData& MeshData);
	void DecrementMemoryStat(const FGeometryCacheMeshData& MeshData);

	TArray<int32> ReadIndices;
	TArray<FGeometryCacheStreamReadRequest*> ReadRequestsPool;

	TArray<int32> FramesNeeded;
	TArray<int32> FramesToBeCached;
	TArray<FGeometryCacheStreamReadRequest*> FramesRequested;

	using FFrameIndexToMeshData = TMap<int32, FGeometryCacheMeshData*>;
	FFrameIndexToMeshData FramesAvailable;
	FRWLock FramesAvailableLock;

	FGeometryCacheStreamDetails Details;
	mutable FGeometryCacheStreamStats Stats;
	int32 CurrentFrameIndex;
	int32 MaxCachedFrames;
	float MaxCachedDuration;
	float MaxMemAllowed;
	float MemoryUsed;

	std::atomic<bool> bCancellationRequested;
	bool bCacheNeedsUpdate;
};
