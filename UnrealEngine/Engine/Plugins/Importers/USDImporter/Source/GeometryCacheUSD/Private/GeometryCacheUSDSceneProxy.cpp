// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryCacheUSDSceneProxy.h"
#include "GeometryCacheUSDComponent.h"
#include "GeometryCacheTrackUSD.h"

FGeometryCacheUsdSceneProxy::FGeometryCacheUsdSceneProxy(UGeometryCacheUsdComponent* Component)
: FGeometryCacheSceneProxy(Component, [this]() { return new FGeomCacheTrackUsdProxy(GetScene().GetFeatureLevel()); })
{
}

bool FGeomCacheTrackUsdProxy::UpdateMeshData(float Time, bool bLooping, int32& InOutMeshSampleIndex, FGeometryCacheMeshData& OutMeshData)
{
	if (UGeometryCacheTrackUsd* UsdTrack = Cast<UGeometryCacheTrackUsd>(Track))
	{
		FGeometryCacheMeshData* PtrMeshData = nullptr;
		bool bResult = UsdTrack->UpdateMeshData(Time, bLooping, InOutMeshSampleIndex, PtrMeshData);
		if (bResult)
		{
			OutMeshData = *PtrMeshData;
		}
		return bResult;
	}
	return false;
}

bool FGeomCacheTrackUsdProxy::GetMeshData(int32 SampleIndex, FGeometryCacheMeshData& OutMeshData)
{
	if (UGeometryCacheTrackUsd* UsdTrack = Cast<UGeometryCacheTrackUsd>(Track))
	{
		return UsdTrack->GetMeshData(SampleIndex, OutMeshData);
	}
	return false;
}

bool FGeomCacheTrackUsdProxy::IsTopologyCompatible(int32 SampleIndexA, int32 SampleIndexB)
{
	if (UGeometryCacheTrackUsd* UsdTrack = Cast<UGeometryCacheTrackUsd>(Track))
	{
		// The boolean argument is not actually used
		const int32 NumVerticesA = UsdTrack->GetSampleInfo(SampleIndexA, false).NumVertices;
		const int32 NumVerticesB = UsdTrack->GetSampleInfo(SampleIndexB, false).NumVertices;

		return NumVerticesA == NumVerticesB;
	}
	return false;
}

const FVisibilitySample& FGeomCacheTrackUsdProxy::GetVisibilitySample(float Time, const bool bLooping) const
{
	// Assume the track is visible for its whole duration
	return FVisibilitySample::VisibleSample;
}

void FGeomCacheTrackUsdProxy::FindSampleIndexesFromTime(float Time, bool bLooping, bool bIsPlayingBackwards, int32 &OutFrameIndex, int32 &OutNextFrameIndex, float &InInterpolationFactor)
{
	if (UGeometryCacheTrackUsd* UsdTrack = Cast<UGeometryCacheTrackUsd>(Track))
	{
		int32 ThisFrameIndex = UsdTrack->FindSampleIndexFromTime(Time, bLooping);
		OutFrameIndex = ThisFrameIndex;
		OutNextFrameIndex = OutFrameIndex + 1;
		// Clamp the Time (which is the FrameNumber) to the range of the USD GeometryCache track
		// EndFrameIndex - 1 since last frame index is not included
		InInterpolationFactor = FMath::Clamp(Time, float(UsdTrack->GetStartFrameIndex()), float(UsdTrack->GetEndFrameIndex() - 1)) - ThisFrameIndex;

		// If playing backwards the logical order of previous and next is reversed
		if (bIsPlayingBackwards)
		{
			Swap(OutFrameIndex, OutNextFrameIndex);
			InInterpolationFactor = 1.0f - InInterpolationFactor;
		}
	}
}
