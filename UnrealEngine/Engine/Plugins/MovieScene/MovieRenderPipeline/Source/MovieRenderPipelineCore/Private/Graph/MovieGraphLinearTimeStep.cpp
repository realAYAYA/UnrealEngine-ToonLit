// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/MovieGraphLinearTimeStep.h"
#include "Graph/MovieGraphPipeline.h"
#include "Graph/MovieGraphBlueprintLibrary.h"
#include "Graph/Nodes/MovieGraphOutputSettingNode.h"
#include "MoviePipelineQueue.h"
#include "MovieRenderPipelineCoreModule.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraActor.h"
#include "Interfaces/Interface_PostProcessVolume.h"
#include "GameFramework/WorldSettings.h"
#include "ProfilingDebugging/MiscTrace.h"

UMovieGraphLinearTimeStep::UMovieGraphLinearTimeStep()
{
	CustomTimeStep = CreateDefaultSubobject<UMovieGraphEngineTimeStep>("MovieGraphEngineTimeStep");

	// This set up our internal state to pick up on the first temporal sub-sample in the pattern
	ResetForEndOfOutputFrame();
	CurrentTimeStepData.OutputFrameNumber = 0;
}

void UMovieGraphLinearTimeStep::TickProducingFrames()
{
	int32 CurrentShotIndex = GetOwningGraph()->GetCurrentShotIndex();
	const TArray<TObjectPtr<UMoviePipelineExecutorShot>>& ActiveShotList = GetOwningGraph()->GetActiveShotList();
	UMoviePipelineExecutorShot* CurrentCameraCut = ActiveShotList[CurrentShotIndex];

	// When start up we want to override the engine's Custom Timestep with our own.
	// This gives us the ability to completely control the engine tick/delta time before the frame
	// is started so that we don't have to always be thinking of delta times one frame ahead. We need
	// to do this only once we're ready to set the timestep though, as Initialize can be called as
	// a result of a OnBeginFrame, meaning that Initialize is called on the frame before TickProducingFrames
	// so there would be one frame where it used the custom timestep (after initialize) before TPF was called.
	if (GEngine->GetCustomTimeStep() != CustomTimeStep)
	{
		PrevCustomTimeStep = GEngine->GetCustomTimeStep();
		GEngine->SetCustomTimeStep(CustomTimeStep);
	}

	// We cache the frame metrics for the duration of a single output frame, instead of recalculating them every tick.
	// This is required for stochastic timesteps (which will jump around the actual evaluated time randomly within one
	// output frame), but it helps resolve some of our issues related to time dilation. Historically when doing it every
	// tick, we didn't know if we were going to overrun the end range of time represented until we had actually done it,
	// partway through the frame, at which point we had to backtrack and abandon the work. So now we calculate it all
	// in advance, check if our new time would overrun the end of the current camera cut, and if so, skip actually starting
	// that frame. Unfortunately this does cause some complications with actor time dilations - the time dilation track
	// will be updating each tick within the output sample and trying to change the world time dilation which we don't want.
	
	// ToDo: Still tbd how we'll fix that, currently the plan is to allow custom clocksources to disable world time dilation
	// support, at which point we'll handle the time dilation by affecting the whole engine tick instead of just world tick.
	//if (IsFirstTemporalSample())
	{
		UpdateFrameMetrics();

		// Now that we've calculated the total range of time we're trying to represent, we can check to see
		// if this would put us beyond our range of time this shot is supposed to represent.

	}
	// Handle shot initialization

	if (CurrentCameraCut->ShotInfo.State == EMovieRenderShotState::Uninitialized)
	{
		UE_LOG(LogMovieRenderPipeline, Log, TEXT("MovieGraph Initializing Camera Cut [%d/%d] in [%s] %s."),
			CurrentShotIndex + 1, ActiveShotList.Num(), *CurrentCameraCut->OuterName, *CurrentCameraCut->InnerName);

		// Evaluate the graph so we can fetch values for this shot.
		UMovieGraphConfig* Config = GetOwningGraph()->GetRootGraphForShot(CurrentCameraCut);
		FMovieGraphTraversalContext Context = GetOwningGraph()->GetCurrentTraversalContext();
		CurrentFrameData.EvaluatedConfig = TStrongObjectPtr<UMovieGraphEvaluatedConfig>(Config->CreateFlattenedGraph(Context));
		CurrentFrameData.TemporalSampleIndex = 0;

		// Ensure we've set it in the CurrentTimeStepData so things can fetch from it below.
		CurrentTimeStepData.EvaluatedConfig = TObjectPtr<UMovieGraphEvaluatedConfig>(CurrentFrameData.EvaluatedConfig.Get());


		// Sets up the render state, etc.
		GetOwningGraph()->SetupShot(CurrentCameraCut);

		// Seeks the external data source to match
		GetOwningGraph()->GetDataSourceInstance()->InitializeShot(CurrentCameraCut);

		const bool bIncludeCDO = false;
		UMovieGraphOutputSettingNode* OutputNode = CurrentFrameData.EvaluatedConfig->GetSettingForBranch<UMovieGraphOutputSettingNode>(UMovieGraphNode::GlobalsPinName, bIncludeCDO);

		const FFrameRate TickResolution = GetOwningGraph()->GetDataSourceInstance()->GetTickResolution();
		const FFrameRate SourceFrameRate = GetOwningGraph()->GetDataSourceInstance()->GetDisplayRate();
		const FFrameRate FinalFrameRate = UMovieGraphBlueprintLibrary::GetEffectiveFrameRate(OutputNode, SourceFrameRate);

		FFrameTime FrameTimePerOutputFrame = FFrameRate::TransformTime(FFrameTime(FFrameNumber(1)), FinalFrameRate, TickResolution);

		// Dummy range so that the first rendering frame knows to start at the right time.
		FFrameTime UpperBound = CurrentCameraCut->ShotInfo.CurrentTimeInRoot;
		CurrentFrameData.LastOutputFrameRange = TRange<FFrameTime>(UpperBound - FrameTimePerOutputFrame, UpperBound);
		CurrentFrameData.LastSampleRange = CurrentFrameData.LastOutputFrameRange;

		// We can safely fall through to the below states as they're OK to process the same frame we set up.
		UE_LOG(LogMovieRenderPipeline, Log, TEXT("MovieGraph Finished initializing Camera Cut [%d/%d] in [%s] %s."),
			CurrentShotIndex + 1, ActiveShotList.Num(), *CurrentCameraCut->OuterName, *CurrentCameraCut->InnerName);

		// Temp...
		CurrentCameraCut->ShotInfo.State = EMovieRenderShotState::Rendering;
	}



	if (CurrentCameraCut->ShotInfo.State == EMovieRenderShotState::Rendering)
	{
		if (IsFirstTemporalSample())
		{
			FMovieGraphTraversalContext Context = GetOwningGraph()->GetCurrentTraversalContext();
			UMovieGraphConfig* Config = GetOwningGraph()->GetRootGraphForShot(CurrentCameraCut);
			CurrentFrameData.EvaluatedConfig = TStrongObjectPtr<UMovieGraphEvaluatedConfig>(Config->CreateFlattenedGraph(Context));

			// The temporal sample count can change every frame due to graph evaluations, so when we're on our first temporal
			// sub-sample of the new frame, we need to re-fetch the value.
			UpdateTemporalSampleCount();

			// Re-calculate some timing statistics about the given TS count
			UpdateFrameMetrics();

			// We create a range of time that we want to represent with this frame in absolute root sequence
			// space. This is because each frame can have a different temporal sample count, and when we jump
			// over the time period the shutter is closed, it's no longer an easy fixed number. So instead,
			// we build a time range we wish to represesnt with this frame, and then split that into sub-regions
			// and simply move between pre-calculated regions. This lets us handle the fact that the delta time
			// between output frames is going to be different when moving between different TS counts on frames.
			
			// To figure out what range of time we want to represent, we actually need to look at the last frame's
			// calculated total output range (ignoring all sub-sampling). The new frame is then multiplied by time
			// dilation, so our entire output range is shortened. We can't just jump between whole frames (due to
			// time dilation) so we have to take the last range, figure out how long the new frame is, and then
			// build ontop of that.
			TRangeBound<FFrameTime> EndOfPreviousFrame = CurrentFrameData.LastOutputFrameRange.GetUpperBound();

			// The upper bound is exclusive, so we initialize a new TRange with just the value to start inclusively
			// ToDo: Apply time dilation by multiplying the calculated range by slow-mo duration.
			CurrentFrameData.CurrentOutputFrameRange = TRange<FFrameTime>(EndOfPreviousFrame.GetValue(), EndOfPreviousFrame.GetValue() + CurrentFrameMetrics.FrameTimePerOutputFrame);

			// Now that we've calculated the total range of time we're trying to represent, we can check to see
			// if this would put us beyond our range of time this shot is supposed to represent.
			if (CurrentFrameData.CurrentOutputFrameRange.GetUpperBoundValue() > CurrentCameraCut->ShotInfo.TotalOutputRangeRoot.GetUpperBoundValue())
			{
				// We're going to spend this frame tearing down the shot (not rendering anything), next frame
				// we'll re-enter this loop and pick up the start of the next shot.
				ResetForEndOfOutputFrame();

				// The delta time isn't very relevant here but you must specify at _a_ delta time each frame to ensure
				// we never have a frame that uses a stale delta time.
				CustomTimeStep->SetCachedFrameTiming(UMovieGraphEngineTimeStep::FTimeStepCache(CurrentFrameMetrics.FrameRate.AsInterval()));

				CurrentCameraCut->ShotInfo.State = EMovieRenderShotState::Finished;
				GetOwningGraph()->TeardownShot(CurrentCameraCut);
				return;
			}

			// Figure out ranges for how long the shutter is open and closed
			{
				TArray<TRange<FFrameTime>> SplitRange = CurrentFrameData.CurrentOutputFrameRange.Split(CurrentFrameData.CurrentOutputFrameRange.GetLowerBoundValue() + CurrentFrameMetrics.FrameTimeWhileShutterOpen);
				if (ensure(SplitRange.Num() == 2))
				{
					CurrentFrameData.RangeShutterOpen = SplitRange[0];
					CurrentFrameData.RangeShutterClosed = SplitRange[1];
				}
			}
			
			// Now split the range the shutter is open per temporal sample.
			{
				CurrentFrameData.TemporalRanges.Reset();
				CurrentFrameData.TemporalRanges.Reserve(CurrentFrameData.TemporalSampleCount);
				FFrameTime SplitStartTime = CurrentFrameData.RangeShutterOpen.GetLowerBoundValue();
				TRange<FFrameTime> RemainingRange = CurrentFrameData.RangeShutterOpen;
				for (int32 Index = 0; Index < CurrentFrameData.TemporalSampleCount - 1; Index++)
				{
					SplitStartTime += CurrentFrameMetrics.FrameTimePerTemporalSample;
					TArray<TRange<FFrameTime>> NewRanges = RemainingRange.Split(SplitStartTime);
					if (ensure(NewRanges.Num() == 2))
					{
						CurrentFrameData.TemporalRanges.Add(NewRanges[0]);
						RemainingRange = NewRanges[1];
					}
				}

				// Add the remaining range, it's the leftover from the last split.
				CurrentFrameData.TemporalRanges.Add(RemainingRange);

				//for (int32 Index = 0; Index < CurrentFrameData.TemporalRanges.Num(); Index++)
				//{
				//	UE_LOG(LogTemp, Warning, TEXT("Range: [%s, %s)"),
				//		*LexToString(CurrentFrameData.TemporalRanges[Index].GetLowerBoundValue()),
				//		*LexToString(CurrentFrameData.TemporalRanges[Index].GetUpperBoundValue()));
				//}
			}
		}
		
		// The delta time for this frame is the difference between the current range index, and the last range index.
		const TRange<FFrameTime>& PreviousRange = CurrentFrameData.LastSampleRange;
		const TRange<FFrameTime>& NextRange = CurrentFrameData.TemporalRanges[CurrentFrameData.TemporalSampleIndex];

		FFrameTime FrameDeltaTime = NextRange.GetLowerBoundValue() - PreviousRange.GetLowerBoundValue();

		// ToDo: Propagate delta time multipliers to cloth

		// Because we know what time range we're supposed to represent, we can just assign the CurrentTimeInRoot absolutely,
		// instead of accumulating delta times. // ToDo: shutter timing, motion blur offset?
		CurrentCameraCut->ShotInfo.CurrentTimeInRoot = NextRange.GetLowerBoundValue();

		FFrameTime FinalEvalTime = CurrentCameraCut->ShotInfo.CurrentTimeInRoot + CurrentFrameMetrics.MotionBlurCenteringOffsetTime + CurrentFrameMetrics.ShutterOffsetFrameTime;
		UE_LOG(LogMovieRenderPipeline, VeryVerbose, TEXT("FinalEvalTime: %s (Tick: %s SOffset: %s MOffset: %s)"),
			*LexToString(FinalEvalTime),
			*LexToString(CurrentCameraCut->ShotInfo.CurrentTimeInRoot), 
			*LexToString(CurrentFrameMetrics.ShutterOffsetFrameTime),
			*LexToString(CurrentFrameMetrics.MotionBlurCenteringOffsetTime));


		// Now we need to fill out some of our current timestep data for the renderer portion to use.
		double FrameDeltaTimeAsSeconds = CurrentFrameMetrics.TickResolution.AsSeconds(FrameDeltaTime);
		CurrentTimeStepData.FrameDeltaTime = FrameDeltaTimeAsSeconds;
		CurrentTimeStepData.WorldSeconds = CurrentTimeStepData.WorldSeconds + FrameDeltaTimeAsSeconds;
		CurrentTimeStepData.WorldTimeDilation = 1.0; // Temp

		// The combination of shutter angle percentage, non-uniform render frame delta times and dividing by sample
		// count produce the correct length for motion blur in all cases.
		CurrentTimeStepData.MotionBlurFraction = CurrentFrameMetrics.MotionBlurAmount / CurrentFrameData.TemporalSampleCount;
		CurrentTimeStepData.bIsFirstTemporalSampleForFrame = IsFirstTemporalSample();
		CurrentTimeStepData.bIsLastTemporalSampleForFrame = IsLastTemporalSample();
		CurrentTimeStepData.bRequiresAccumulator = CurrentFrameData.TemporalSampleCount > 1;
		CurrentTimeStepData.OutputFrameNumber = CurrentFrameData.OutputFrameNumber;
		
		CurrentTimeStepData.EvaluatedConfig = TObjectPtr<UMovieGraphEvaluatedConfig>(CurrentFrameData.EvaluatedConfig.Get());

		UE_LOG(LogTemp, Warning, TEXT("F# %d bFirst: %d bLast: %d bReqAc: %d"),
			CurrentTimeStepData.OutputFrameNumber, CurrentTimeStepData.bIsFirstTemporalSampleForFrame,
			CurrentTimeStepData.bIsLastTemporalSampleForFrame, CurrentTimeStepData.bRequiresAccumulator);

		// Set our time step for the next frame. We use the undilated delta time for the Custom Timestep as the engine will
		// apply the time dilation to the world tick for us, so we don't want to double up time dilation.
		double UndilatedDeltaTime = CurrentFrameMetrics.TickResolution.AsSeconds(FrameDeltaTime);
		CustomTimeStep->SetCachedFrameTiming(UMovieGraphEngineTimeStep::FTimeStepCache(UndilatedDeltaTime));
		GetOwningGraph()->GetDataSourceInstance()->SyncDataSourceTime(FinalEvalTime);

		// ToDo: This should be converted back to an 'effective' frame number (source frame in external data asset)
		// so you can line up profiling with the acutal content on screen.
		TRACE_BOOKMARK(TEXT("MoviePipeline - Rendering Frame %d [TS: %d]"), CurrentTimeStepData.OutputFrameNumber, CurrentFrameData.TemporalSampleIndex);

		// Increment various post-frame counters to set them up for the next frame. This is okay
		// because the rest of the Movie Graph Pipeline system is based on CurrentTimeStepData which accurately
		// reflects which frame we're on.
		{
			// Update the last sample range to the one we just rendered.
			CurrentFrameData.LastSampleRange = CurrentFrameData.TemporalRanges[CurrentFrameData.TemporalSampleIndex];
			if (IsLastTemporalSample())
			{
				CurrentFrameData.LastOutputFrameRange = CurrentFrameData.CurrentOutputFrameRange;
				
				// Increment the output frame number only on the first temporal sample.
				CurrentFrameData.OutputFrameNumber++;

			}

			if (CurrentFrameData.TemporalSampleIndex >= CurrentFrameData.TemporalSampleCount - 1)
			{
				// If we've rendered the last temporal sub-sample, we've started a new output frame
				// and we need to reset our temporal sample index.
				CurrentFrameData.TemporalSampleIndex = 0;
			}
			else
			{
				// Each tick we increment the temporal sample we're on.
				CurrentFrameData.TemporalSampleIndex++;
			}
		}


		if (!ensure(CurrentCameraCut->ShotInfo.CurrentTickInRoot < CurrentCameraCut->ShotInfo.TotalOutputRangeRoot.GetUpperBoundValue()))
		{
			UE_LOG(LogMovieRenderPipeline, Error, TEXT("Shot ran past evaluation range, this shouldn't be possible."));
		}
	}

}

bool UMovieGraphLinearTimeStep::IsFirstTemporalSample() const
{
	return CurrentFrameData.TemporalSampleIndex == 0;
}

bool UMovieGraphLinearTimeStep::IsLastTemporalSample() const
{
	return CurrentFrameData.TemporalSampleIndex == CurrentFrameData.TemporalSampleCount - 1;
}

void UMovieGraphLinearTimeStep::ResetForEndOfOutputFrame()
{
	CurrentFrameData.TemporalSampleIndex = 0;
}

void UMovieGraphLinearTimeStep::UpdateTemporalSampleCount()
{
	// ToDo: This needs to come from the config.
	CurrentFrameData.TemporalSampleCount = 1;
}

void UMovieGraphLinearTimeStep::Shutdown()
{
	// Shut down our custom timestep which reqstores some world settings we modified.

	// ToDo: This takes 200ms because of an arbitrary sleep inside of SetCustomTimeStep when shutting down
	// but that requires some bigger changes to custom timesteps to fix.
	GEngine->SetCustomTimeStep(PrevCustomTimeStep);
}

void UMovieGraphLinearTimeStep::UpdateFrameMetrics()
{
	FOutputFrameMetrics FrameData;

	// We inherit a tick resolution from the level sequence so that we can represent the same
	// range of time that the level sequence does.
	FrameData.TickResolution = GetOwningGraph()->GetDataSourceInstance()->GetTickResolution();
	FrameData.FrameRate = GetOwningGraph()->GetDataSourceInstance()->GetDisplayRate(); // ToDo, needs to come from config (config can override)
	FrameData.FrameTimePerOutputFrame = FFrameRate::TransformTime(FFrameTime(FFrameNumber(1)), FrameData.FrameRate, FrameData.TickResolution);

	// Manually perform blending of the Post Process Volumes/Camera/Camera Modifiers to match what the renderer will do.
	// This uses the primary camera specified by the PlayerCameraManager to get the motion blur amount so in the event of
	// multi-camera rendering, all cameras will end up using the same motion blur amount defined by the primary camera).
	FrameData.MotionBlurAmount = GetBlendedMotionBlurAmount();

	// Calculate how long of a duration we want to represent where the camera shutter is open.
	FrameData.FrameTimeWhileShutterOpen = FrameData.FrameTimePerOutputFrame * FrameData.MotionBlurAmount;

	// Now that we know how long the shutter is open, figure out how long each temporal sub-sample gets.
	FrameData.FrameTimePerTemporalSample = FrameData.FrameTimeWhileShutterOpen / CurrentFrameData.TemporalSampleCount;

	// The amount of time closed + time open should add up to exactly how long a frame is.
	FrameData.FrameTimeWhileShutterClosed = FrameData.FrameTimePerOutputFrame - FrameData.FrameTimeWhileShutterOpen;

	// Shutter timing is a bias applied to the final evaluation time to let us change what we consider a frame
	// ie: Do we consider a frame the start of the timespan we captured? Or is the frame the end of the timespan?
	// We default to Centered so that the center of your evaluated time is what you see in Level Sequences.
	// ToDo: Shutter Timing Offset
	//switch (CameraSettings->ShutterTiming)
	//{
	//	// Subtract the entire time the shutter is open.
	//case EMoviePipelineShutterTiming::FrameClose:
	//	Output.ShutterOffsetTicks = -Output.TicksWhileShutterOpen;
	//	break;
	//	// Only subtract half the time the shutter is open.
	//case EMoviePipelineShutterTiming::FrameCenter:
	FrameData.ShutterOffsetFrameTime = -FrameData.FrameTimeWhileShutterOpen / 2.0;
	//	break;
	//	// No offset needed
	//case EMoviePipelineShutterTiming::FrameOpen:
	//	break;
	//}

	// Then, calculate our motion blur offset. Motion Blur in the engine is always
	// centered around the object so we offset our time sampling by half of the
	// motion blur distance so that the distance blurred represents that time.
	FrameData.MotionBlurCenteringOffsetTime = FrameData.FrameTimePerTemporalSample / 2.0;

	CurrentFrameMetrics = FrameData;
}

float UMovieGraphLinearTimeStep::GetBlendedMotionBlurAmount()
{
	// 0.5f is the default engine motion blur in the event no Post Process/Camera overrides it.
	float FinalMotionBlurAmount = 0.5f;

	APlayerCameraManager* PlayerCameraManager = GetWorld()->GetFirstPlayerController()->PlayerCameraManager;
	if (PlayerCameraManager)
	{
		// Apply any motion blur settings from post process volumes in the world
		FVector ViewLocation = PlayerCameraManager->GetCameraLocation();
		for (IInterface_PostProcessVolume* PPVolume : GetWorld()->PostProcessVolumes)
		{
			const FPostProcessVolumeProperties VolumeProperties = PPVolume->GetProperties();

			// Skip any volumes which are either disabled or don't modify blur amount
			if (!VolumeProperties.bIsEnabled || !VolumeProperties.Settings->bOverride_MotionBlurAmount)
			{
				continue;
			}

			float LocalWeight = FMath::Clamp(VolumeProperties.BlendWeight, 0.0f, 1.0f);

			if (!VolumeProperties.bIsUnbound)
			{
				float DistanceToPoint = 0.0f;
				PPVolume->EncompassesPoint(ViewLocation, 0.0f, &DistanceToPoint);

				if (DistanceToPoint >= 0 && DistanceToPoint < VolumeProperties.BlendRadius)
				{
					LocalWeight *= FMath::Clamp(1.0f - DistanceToPoint / VolumeProperties.BlendRadius, 0.0f, 1.0f);
				}
				else
				{
					LocalWeight = 0.0f;
				}
			}

			if (LocalWeight > 0.0f)
			{
				FinalMotionBlurAmount = FMath::Lerp(FinalMotionBlurAmount, VolumeProperties.Settings->MotionBlurAmount, LocalWeight);
			}
		}

		// Now try from the camera, which takes priority over post processing volumes.
		ACameraActor* CameraActor = Cast<ACameraActor>(PlayerCameraManager->GetViewTarget());
		if (CameraActor)
		{
			UCameraComponent* CameraComponent = CameraActor->GetCameraComponent();
			if (CameraComponent && CameraComponent->PostProcessSettings.bOverride_MotionBlurAmount)
			{
				FinalMotionBlurAmount = FMath::Lerp(FinalMotionBlurAmount, CameraComponent->PostProcessSettings.MotionBlurAmount, CameraComponent->PostProcessBlendWeight);
			}
		}

		// Apply any motion blur settings from post processing blends attached to the camera manager
		TArray<FPostProcessSettings> const* CameraAnimPPSettings;
		TArray<float> const* CameraAnimPPBlendWeights;
		PlayerCameraManager->GetCachedPostProcessBlends(CameraAnimPPSettings, CameraAnimPPBlendWeights);
		for (int32 PPIdx = 0; PPIdx < CameraAnimPPBlendWeights->Num(); ++PPIdx)
		{
			if ((*CameraAnimPPSettings)[PPIdx].bOverride_MotionBlurAmount)
			{
				FinalMotionBlurAmount = FMath::Lerp(FinalMotionBlurAmount, (*CameraAnimPPSettings)[PPIdx].MotionBlurAmount, (*CameraAnimPPBlendWeights)[PPIdx]);
			}
		}
	}

	return FinalMotionBlurAmount;
}

UMovieGraphEngineTimeStep::UMovieGraphEngineTimeStep()
	: PrevMinUndilatedFrameTime(0.f)
	, PrevMaxUndilatedFrameTime(0.f)
{
}

bool UMovieGraphEngineTimeStep::Initialize(UEngine* InEngine)
{
	// We cache the Min/Max times the world has because we modify these during
	// renders to match our sample length, otherwise stuff gets out of sync.
	if (AWorldSettings* WorldSettings = GetWorld()->GetWorldSettings())
	{
		PrevMinUndilatedFrameTime = WorldSettings->MinUndilatedFrameTime;
		PrevMaxUndilatedFrameTime = WorldSettings->MaxUndilatedFrameTime;
	}

	return true;
}

void UMovieGraphEngineTimeStep::Shutdown(UEngine* InEngine)
{
	if (AWorldSettings* WorldSettings = GetWorld()->GetWorldSettings())
	{
		WorldSettings->MinUndilatedFrameTime = PrevMinUndilatedFrameTime;
		WorldSettings->MaxUndilatedFrameTime = PrevMaxUndilatedFrameTime;
	}
}

void UMovieGraphEngineTimeStep::SetCachedFrameTiming(const FTimeStepCache& InTimeCache)
{
	if (ensureMsgf(!FMath::IsNearlyZero(InTimeCache.UndilatedDeltaTime), TEXT("An incorrect or uninitialized time step was used! Delta Time of 0 isn't allowed.")))
	{
		TimeCache = InTimeCache;
	}
	else
	{
		UE_LOG(LogMovieRenderPipeline, Error, TEXT("SetCachedFrameTiming called with zero delta time, falling back to 1/24"));
		TimeCache = FTimeStepCache(1 / 24.0);
	}
}

bool UMovieGraphEngineTimeStep::UpdateTimeStep(UEngine* /*InEngine*/)
{
	if (ensureMsgf(!FMath::IsNearlyZero(TimeCache.UndilatedDeltaTime), TEXT("An incorrect or uninitialized time step was used! Delta Time of 0 isn't allowed.")))
	{
		FApp::UpdateLastTime();
		FApp::SetDeltaTime(TimeCache.UndilatedDeltaTime);
		FApp::SetCurrentTime(FApp::GetCurrentTime() + FApp::GetDeltaTime());

		// The UWorldSettings can clamp the delta time inside the Level Tick function, this creates a mismatch between component
		// velocity and render thread velocity and becomes an issue at high temporal sample counts. 
		if (AWorldSettings* WorldSettings = GetWorld()->GetWorldSettings())
		{
			WorldSettings->MinUndilatedFrameTime = TimeCache.UndilatedDeltaTime;
			WorldSettings->MaxUndilatedFrameTime = TimeCache.UndilatedDeltaTime;
		}
	}

	// Clear our cached time to ensure we're always explicitly telling this what to do and never relying on the last set value.
	// (This will cause the ensure above to check on the next tick if someone didn't reset our value.)
	TimeCache = FTimeStepCache();

	// Return false so the engine doesn't run its own logic to overwrite FApp timings.
	return false;
}
