// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryCacheTrackUSD.h"

#include "USDLog.h"
#include "UsdWrappers/SdfLayer.h"
#include "UsdWrappers/UsdStage.h"

#include "GeometryCacheUSDStream.h"
#include "IGeometryCacheStreamer.h"

UGeometryCacheTrackUsd::UGeometryCacheTrackUsd()
: StartFrameIndex(0)
, EndFrameIndex(0)
{
}

void UGeometryCacheTrackUsd::BeginDestroy()
{
	UnloadUsdStage();

	IGeometryCacheStreamer::Get().UnregisterTrack( this );
	UsdStream.Reset();

	Super::BeginDestroy();
}

const bool UGeometryCacheTrackUsd::UpdateMeshData(const float Time, const bool bLooping, int32& InOutMeshSampleIndex, FGeometryCacheMeshData*& OutMeshData)
{
	const int32 SampleIndex = FindSampleIndexFromTime(Time, bLooping);

	// If InOutMeshSampleIndex equals -1 (first creation) update the OutVertices and InOutMeshSampleIndex
	// Update the Vertices and Index if SampleIndex is different from the stored InOutMeshSampleIndex
	if (InOutMeshSampleIndex == -1 || SampleIndex != InOutMeshSampleIndex)
	{
		if (GetMeshData(SampleIndex, MeshData))
		{
			OutMeshData = &MeshData;
			InOutMeshSampleIndex = SampleIndex;
			return true;
		}
	}
	return false;
}

const bool UGeometryCacheTrackUsd::UpdateBoundsData(const float Time, const bool bLooping, const bool bIsPlayingBackward, int32& InOutBoundsSampleIndex, FBox& OutBounds)
{
	const int32 SampleIndex = FindSampleIndexFromTime(Time, bLooping);

	const FGeometryCacheTrackSampleInfo& SampledInfo = GetSampleInfo(Time, bLooping);
	if (InOutBoundsSampleIndex != SampleIndex)
	{
		OutBounds = SampledInfo.BoundingBox;
		InOutBoundsSampleIndex = SampleIndex;
		return true;
	}
	return false;
}

const int32 UGeometryCacheTrackUsd::FindSampleIndexFromTime(const float Time, const bool bLooping) const
{
	// Treat the time as the frame index
	int32 FrameIndex = (int32) Time;
	return FMath::Clamp(FrameIndex, StartFrameIndex, EndFrameIndex - 1);
}

const FGeometryCacheTrackSampleInfo& UGeometryCacheTrackUsd::GetSampleInfo(float Time, bool bLooping)
{
	if (SampleInfos.Num() == 0)
	{
		if (Duration > 0.f)
		{
			// Duration is the number of frames
			SampleInfos.SetNum((int32)Duration);
		}
		else
		{
			return FGeometryCacheTrackSampleInfo::EmptySampleInfo;
		}
	}

	// The sample info index must start from 0, while the sample index is between the range of the animation
	const int32 SampleIndex = FindSampleIndexFromTime( Time, bLooping );
	const int32 SampleInfoIndex = SampleIndex - StartFrameIndex;

	FGeometryCacheTrackSampleInfo& CurrentSampleInfo = SampleInfos[SampleInfoIndex];

	if (CurrentSampleInfo.SampleTime == 0.0f && CurrentSampleInfo.NumVertices == 0 && CurrentSampleInfo.NumIndices == 0)
	{
		FGeometryCacheMeshData TempMeshData;
		if (GetMeshData(SampleIndex, TempMeshData))
		{
			CurrentSampleInfo = FGeometryCacheTrackSampleInfo(
				Time,
				(FBox) TempMeshData.BoundingBox,
				TempMeshData.Positions.Num(),
				TempMeshData.Indices.Num()
			);
		}
		else
		{
			// This shouldn't really happen but if it does make sure this is initialized,
			// as it can crash/throw ensures depending on the uninitialized memory values,
			// while it will only be slightly visually glitchy with a zero bounding box
			CurrentSampleInfo.BoundingBox.Init();
		}
	}

	return CurrentSampleInfo;
}

bool UGeometryCacheTrackUsd::GetMeshDataAtTime(float Time, FGeometryCacheMeshData& OutMeshData)
{
	const bool bLooping = true;
	const int32 SampleIndex = FindSampleIndexFromTime(Time, bLooping);
	return GetMeshData(SampleIndex, OutMeshData);
}

bool UGeometryCacheTrackUsd::GetMeshData(int32 SampleIndex, FGeometryCacheMeshData& OutMeshData)
{
	if (IGeometryCacheStreamer::Get().IsTrackRegistered(this))
	{
		return IGeometryCacheStreamer::Get().TryGetFrameData(this, SampleIndex, OutMeshData);
	}
	return false;
}

bool UGeometryCacheTrackUsd::LoadUsdStage()
{
	if ( CurrentStagePinned )
	{
		// Already loaded
		return true;
	}

	if ( CurrentStageWeak )
	{
		// Upgrade our weak pointer if its not invalid already
		CurrentStagePinned = CurrentStageWeak;
		return true;
	}
	else if ( !StageRootLayerPath.IsEmpty() )
	{
		UE_LOG( LogUsd, Warning, TEXT( "UGeometryCacheTrackUsd is reopening the stage '%s' to stream in frames for the geometry cache generated for prim '%s'" ), *StageRootLayerPath, *PrimPath );

		// Reopen the stage. If our weak pointer is no longer valid then nothing cared about keeping that
		// stage alive anyway, so it's likely not a problem if we start reading frames from the reopened stage
		// and abandon any previous in-memory changes we had, if any.
		// Keep in mind currently it's effectively impossible to get in here at all anyway, as we'll only
		// have UGeometryCacheTrackUsd streaming stuff if a stage actor caused it to stream more frames,
		// and in that case that stage actor would have kept our stage opened.
		// Not using the stage cache here because there's currently no way of knowing when to erase it from the cache
		// after we're done with it, and the UGeometryCacheTrackUsd shouldn't have authority to blindly just remove it
		// from the cache as the user may have place it there intentionally
		const bool bUseStageCache = false;
		CurrentStagePinned = UnrealUSDWrapper::OpenStage( *StageRootLayerPath, EUsdInitialLoadSet::LoadAll, bUseStageCache );
		CurrentStageWeak = CurrentStagePinned;
		return true;
	}

	UE_LOG( LogUsd, Warning, TEXT( "UGeometryCacheTrackUsd track failed to access USD stage to stream requested frames" ) );
	return false;
}

void UGeometryCacheTrackUsd::UnloadUsdStage()
{
	CurrentStagePinned = UE::FUsdStage();
}

void UGeometryCacheTrackUsd::Initialize(
	const UE::FUsdStage& InStage,
	const FString& InPrimPath,
	const FName& InRenderContext,
	const TMap< FString, TMap< FString, int32 > >& InMaterialToPrimvarToUVIndex,
	int32 InStartFrameIndex,
	int32 InEndFrameIndex,
	FReadUsdMeshFunction InReadFunc
)
{
	CurrentStagePinned = InStage;
	CurrentStageWeak = CurrentStagePinned;
	StageRootLayerPath = CurrentStagePinned ? CurrentStagePinned.GetRootLayer().GetRealPath() : FString();

	PrimPath = InPrimPath;
	RenderContext = InRenderContext;
	MaterialToPrimvarToUVIndex = InMaterialToPrimvarToUVIndex;
	StartFrameIndex = InStartFrameIndex;
	EndFrameIndex = InEndFrameIndex;

	Duration = ( float ) ( EndFrameIndex - StartFrameIndex );

	UsdStream.Reset(new FGeometryCacheUsdStream(this, InReadFunc));
	IGeometryCacheStreamer::Get().RegisterTrack(this, UsdStream.Get());
	UsdStream->Prefetch(StartFrameIndex);
}

void UGeometryCacheTrackUsd::UpdateTime(float Time, bool bLooping)
{
	if (UsdStream)
	{
		int32 FrameIndex = FindSampleIndexFromTime(Time, bLooping);
		UsdStream->UpdateCurrentFrameIndex(FrameIndex);
	}
}
