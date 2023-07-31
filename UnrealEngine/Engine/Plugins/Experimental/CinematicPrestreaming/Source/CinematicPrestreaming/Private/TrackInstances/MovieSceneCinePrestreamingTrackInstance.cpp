// Copyright Epic Games, Inc. All Rights Reserved.

#include "TrackInstances/MovieSceneCinePrestreamingTrackInstance.h"

#include "Algo/Sort.h"
#include "CinePrestreamingData.h"
#include "CinePrestreamingLog.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "EngineModule.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "EntitySystem/MovieSceneInstanceRegistry.h"
#include "IMovieScenePlayer.h"
#include "MovieScene.h"
#include "MovieSceneTimeHelpers.h"
#include "RendererInterface.h"
#include "Sections/MovieSceneCinePrestreamingSection.h"
#include "Tracks/MovieSceneCinePrestreamingTrack.h"

DECLARE_CYCLE_STAT(TEXT("Cinematic PrestreamingTrack Animate"), MovieSceneEval_CinePrestreamingTrack_Animate, STATGROUP_MovieSceneEval);

static TAutoConsoleVariable<int32> CVarCinematicPreStreamQualityLevel(
	TEXT("MovieScene.PreStream.QualityLevel"),
	2,
	TEXT("We discard prestreaming sections with QualityLevel greater than this."),
	ECVF_ReadOnly
);

void UMovieSceneCinePrestreamingTrackInstance::OnInputAdded(const FMovieSceneTrackInstanceInput& InInput)
{
	// Trigger async load for the asset associated with this input.
	const UMovieSceneCinePrestreamingSection* Section = Cast<const UMovieSceneCinePrestreamingSection>(InInput.Section);
	if (Section != nullptr)
	{
		const int32 QualityLevel = Section->GetQualityLevel();
		if (CVarCinematicPreStreamQualityLevel.GetValueOnGameThread() < QualityLevel)
		{
			return;
		}

		TSoftObjectPtr<UCinePrestreamingData> AssetSoftPtr = Section->GetPrestreamingAsset();
		if (!AssetSoftPtr.IsNull())
		{
			if (PrestreamingAssetMap.Find(InInput) == nullptr)
			{
				TWeakObjectPtr<UMovieSceneCinePrestreamingTrackInstance> WeakThis = this;
				FMovieSceneTrackInstanceInput Input = InInput;

				FStreamableDelegate OnLoadedDelegate;
				OnLoadedDelegate.BindLambda(
					[WeakThis, Input, AssetSoftPtr]()
					{
						if (UMovieSceneCinePrestreamingTrackInstance* StrongThis = WeakThis.Get())
						{
							if (UCinePrestreamingData* AssetPtr = AssetSoftPtr.Get())
							{
								StrongThis->PrestreamingAssetMap.Add(Input, AssetPtr);
								StrongThis->LoadHandleMap.Remove(Input);
							}
						}
					});
		
				FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
				TSharedPtr<FStreamableHandle> LoadHandle = StreamableManager.RequestAsyncLoad(AssetSoftPtr.ToSoftObjectPath(), OnLoadedDelegate);
				LoadHandleMap.Add(InInput, LoadHandle);
			}
		}
	}
}

void UMovieSceneCinePrestreamingTrackInstance::OnInputRemoved(const FMovieSceneTrackInstanceInput& InInput)
{
	const UMovieSceneCinePrestreamingSection* Section = Cast<const UMovieSceneCinePrestreamingSection>(InInput.Section);
	if (Section != nullptr)
	{
		// Remove asset reference should lead to asset unload at some future time.
		if (PrestreamingAssetMap.Remove(InInput) == 0)
		{
			// Didn't remove, so maybe we still have an outstanding load handle.
			TSharedPtr<FStreamableHandle>* LoadHandlePtr = LoadHandleMap.Find(InInput);
			if (LoadHandlePtr != nullptr)
			{
				FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
				(*LoadHandlePtr)->CancelHandle();
				LoadHandleMap.Remove(InInput);
			}
		}
	}
}

void UMovieSceneCinePrestreamingTrackInstance::OnAnimate()
{
	SCOPE_CYCLE_COUNTER(MovieSceneEval_CinePrestreamingTrack_Animate);

	using namespace UE::MovieScene;

	const FInstanceRegistry* InstanceRegistry = GetLinker()->GetInstanceRegistry();

	// Gather active prestreaming sections.
	FCinePrestreamingVTData AccumulatedVirtualTextureData;
	FCinePrestreamingNaniteData AccumulatedNaniteData;

	for (const FMovieSceneTrackInstanceInput& Input : GetInputs())
	{
		const FSequenceInstance& SequenceInstance = InstanceRegistry->GetInstance(Input.InstanceHandle);
		const FMovieSceneContext& Context = SequenceInstance.GetContext();

		// Get start/current/end time.
		const TRange<FFrameNumber> SectionRange(Input.Section->GetTrueRange());
		const FFrameNumber LocalStartTime = SectionRange.HasLowerBound() ? SectionRange.GetLowerBoundValue() : FFrameNumber(-MAX_int32);
		const FFrameNumber LocalEndTime = SectionRange.HasUpperBound() ? SectionRange.GetUpperBoundValue() : FFrameNumber(MAX_int32);
		
		// Get the time relative to the start of the section.
		const FFrameTime LocalContextTime = Context.GetTime();
		const FFrameNumber LocalEvalFrame = LocalContextTime.FloorToFrame().Value - LocalStartTime;

		// Apply an offset to start evaluation early (extends into PreRoll so must be less than number of PreRoll frames).
		const int32 OffsetTime = Input.Section->GetOffsetTime().Get(0).GetFrame().Value;
		const FFrameRate DisplayRate = Input.Section->GetTypedOuter<UMovieScene>()->GetDisplayRate();
		const int32 OffsetTimeFrames = FFrameRate::TransformTime(FFrameTime(OffsetTime), DisplayRate, Context.GetFrameRate()).FloorToFrame().Value;
		const int32 PreRollFrames = Input.Section->GetPreRollFrames();
		const FFrameNumber StartOffsetFrames = FMath::Clamp(OffsetTimeFrames, 0, PreRollFrames);

		// Early out if we're out of evaluation range.
		// This can be in the PreRoll, or in the dead space at the end of the shot caused by the OffsetTime.
		if ((LocalEvalFrame + StartOffsetFrames).Value < 0 || (LocalEvalFrame + StartOffsetFrames) > (LocalEndTime - LocalStartTime))
		{
			continue;
		}

		// Early out if this input is stripped by QualityLevel setting.
		const UMovieSceneCinePrestreamingSection* Section = Cast<const UMovieSceneCinePrestreamingSection>(Input.Section);
		const int32 QualityLevel = Section->GetQualityLevel();
		if (CVarCinematicPreStreamQualityLevel.GetValueOnGameThread() < QualityLevel)
		{
			continue;
		}

		// Get (async loaded) asset data.
		TObjectPtr<UCinePrestreamingData>* PrestreamingDataPtr = PrestreamingAssetMap.Find(Input);
		
		// PreRoll needs to be long enough to cover any async loading before evaluation starts.
		// In editor this can happen if we are scrubbing the track. But that case is fine.
		if (!Section->GetPrestreamingAsset().IsNull() && PrestreamingDataPtr == nullptr)
		{
			UE_LOG(LogCinePrestreaming, Verbose, TEXT("Prestreaming asset %s didn't load in time. Consider using a longer PreRoll."), *Section->GetPrestreamingAsset().GetAssetName());
		}

		UCinePrestreamingData const* PrestreamingData = PrestreamingDataPtr != nullptr ? PrestreamingDataPtr->Get() : nullptr;
		if (PrestreamingData == nullptr || PrestreamingData->Times.Num() == 0)
		{
			continue;
		}

		// Deal with offset due to internal start time of recorded data.
		const FFrameNumber DataOffsetFrames = PrestreamingData->Times[0];

		// Find the closest (floored) keyframe time.
		const int32 Index = FMath::Max(0, Algo::UpperBound(PrestreamingData->Times, (LocalEvalFrame + StartOffsetFrames + DataOffsetFrames).Value) - 1);

		// Fetch the data from this keyframe.
		// todo: Fetch the data from some time range of keyframes?
		AccumulatedVirtualTextureData.PageIds.Append(PrestreamingData->VirtualTextureDatas[Index].PageIds);
		AccumulatedNaniteData.RequestData.Append(PrestreamingData->NaniteDatas[Index].RequestData);
	}

	// Submit virtual texture prestreaming data to the renderer.
	if (AccumulatedVirtualTextureData.PageIds.Num() > 0)
	{
		ENQUEUE_RENDER_COMMAND(RequestTiles)([PageIds = MoveTemp(AccumulatedVirtualTextureData.PageIds)](FRHICommandListImmediate& RHICmdList) mutable
		{
			GetRendererModule().RequestVirtualTextureTiles(MoveTemp(PageIds));
		});
	}

	if (AccumulatedNaniteData.RequestData.Num() > 0)
	{
		ENQUEUE_RENDER_COMMAND(RequestNanitePages)([RequestData = MoveTemp(AccumulatedNaniteData.RequestData)](FRHICommandListImmediate& RHICmdList) mutable
		{
			GetRendererModule().RequestNanitePages(MoveTemp(RequestData));
		});
	}
}
