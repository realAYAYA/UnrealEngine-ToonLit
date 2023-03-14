// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UObject/Object.h"
#include "Engine/EngineCustomTimeStep.h"
#include "MovieRenderPipelineDataTypes.h"
#include "MoviePipelineBlueprintLibrary.h"
#if WITH_EDITOR
#include "MovieSceneExportMetadata.h"
#endif
#include "MovieSceneTimeController.h"
#include "Async/Future.h"
#include "MoviePipeline.generated.h"

// Forward Declares
class UMoviePipelineMasterConfig;
class ULevelSequence;
class UMovieSceneSequencePlayer;
class UMoviePipelineCustomTimeStep;
class UEngineCustomTimeStep;
class ALevelSequenceActor;
class UMovieRenderDebugWidget;
class FMovieRenderViewport;
class FMovieRenderViewportClient;
struct FImagePixelPipe;
struct FMoviePipelineTimeController;
class FMoviePipelineOutputMerger;
class IImageWriteQueue;
class UMoviePipelineExecutorJob;
class UMoviePipelineExecutorShot;
class UMoviePipelineSetting;
class UTexture;

typedef TTuple<TFuture<bool>, MoviePipeline::FMoviePipelineOutputFutureData> FMoviePipelineOutputFuture;

DECLARE_MULTICAST_DELEGATE_TwoParams(FMoviePipelineFinishedNative, UMoviePipeline*, bool);
DECLARE_MULTICAST_DELEGATE_OneParam(FMoviePipelineWorkFinishedNative, FMoviePipelineOutputData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMoviePipelineFinished, UMoviePipeline*, MoviePipeline, bool, bFatalError);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMoviePipelineWorkFinished, FMoviePipelineOutputData, Results);

UCLASS(Blueprintable)
class MOVIERENDERPIPELINECORE_API UMoviePipeline : public UObject
{
	GENERATED_BODY()
	
public:
	UMoviePipeline();

	/** 
	* Initialize the movie pipeline with the specified settings. This kicks off the rendering process. 
	* @param InJob	- This contains settings and sequence to render this Movie Pipeline with.
	*/
	UFUNCTION(BlueprintCallable, Category = "Movie Render Pipeline")
	void Initialize(UMoviePipelineExecutorJob* InJob);


	/**
	* Request the movie pipeline to shut down at the next available time. The pipeline will attempt to abandon
	* the current frame (such as if there are more temporal samples pending) but may be forced into finishing if
	* there are spatial samples already submitted to the GPU. The shutdown flow will be run to ensure already
	* completed work is written to disk. This is a non-blocking operation, use Shutdown() instead if you need to
	* block until it is fully shut down.
	*
	* @param bError - Whether this is a request for early shut down due to an error
	* 
	* This function is thread safe.
	*/
	UFUNCTION(BlueprintCallable, Category = "Movie Render Pipeline")
	void RequestShutdown(bool bIsError=false);
	
	/** 
	* Abandons any future work on this Movie Pipeline and runs through the shutdown flow to ensure already
	* completed work is written to disk. This is a blocking-operation and will not return until all outstanding
	* work has been completed.
	*
	* @param bError - Whether this is an early shut down due to an error
	*
	* This function should only be called from the game thread.
	*/
	UFUNCTION(BlueprintCallable, Category = "Movie Render Pipeline")
	void Shutdown(bool bError=false);

	/**
	* Has RequestShutdown() been called?
	*/
	UFUNCTION(BlueprintPure, Category = "Movie Render Pipeline")
	bool IsShutdownRequested() const { return bShutdownRequested; }

	/**
	* Returns the time this movie pipeline was initialized at.
	*/
	UFUNCTION(BlueprintPure, Category = "Movie Render Pipeline")
	FDateTime GetInitializationTime() const { return InitializationTime; }

	/**
	* Override the time this movie pipeline was initialized at. This can be used for render farms
	* to ensure that jobs on all machines use the same date/time instead of each calculating it locally.
	*
	* Needs to be called after ::Initialize(...)
	*
	* @param InDateTime - The DateTime object to return for GetInitializationTime.
	*/
	UFUNCTION(BlueprintCallable, Category = "Movie Render Pipeline")
	void SetInitializationTime(const FDateTime& InDateTime) { InitializationTime = InDateTime; }

	/** Deprecated. Use OnMoviePipelineWorkFinished() instead. */
	UE_DEPRECATED(4.27, "Use OnMoviePipelineWorkFinished() instead.")
	FMoviePipelineFinishedNative& OnMoviePipelineFinished()
	{
		return OnMoviePipelineFinishedDelegateNative;
	}

	/** Deprecated. Use OnMoviePipelineWorkFinishedDelegate instead */
	UE_DEPRECATED(4.27, "Use OnMoviePipelineWorkFinishedDelegate instead.")
	UPROPERTY(BlueprintAssignable, Category = "Movie Render Pipeline")
	FMoviePipelineFinished OnMoviePipelineFinishedDelegate;

	/**
	* Called when we have completely finished this pipeline. This means that all frames have been rendered,
	* all files written to disk, and any post-finalize exports have finished. This Pipeline will call
	* Shutdown() on itself before calling this delegate to ensure we've unregistered from all delegates
	* and are no longer trying to do anything (even if we still exist).
	*
	* The params struct in the return will have metadata about files written to disk for each shot.
	*/
	FMoviePipelineWorkFinishedNative& OnMoviePipelineWorkFinished()
	{
		return OnMoviePipelineWorkFinishedDelegateNative;
	}
	
	/**
	* Called when we have completely finished this pipeline. This means that all frames have been rendered,
	* all files written to disk, and any post-finalize exports have finished. This Pipeline will call
	* Shutdown() on itself before calling this delegate to ensure we've unregistered from all delegates
	* and are no longer trying to do anything (even if we still exist).
	*
	* The params struct in the return will have metadata about files written to disk for each shot.
	*/
	UPROPERTY(BlueprintAssignable, Category = "Movie Render Pipeline")
	FMoviePipelineWorkFinished OnMoviePipelineWorkFinishedDelegate;

	/**
	* Only called if `IsFlushDiskWritesPerShot()` is set!
	* Called after each shot is finished and files have been flushed to disk. The returned data in
	* the params struct will have only the per-shot metadata for the just finished shot. Use
	* OnMoviePipelineFinished() if you need all ot the metadata.
	*/
	FMoviePipelineWorkFinishedNative& OnMoviePipelineShotWorkFinished()
	{
		return OnMoviePipelineShotWorkFinishedDelegateNative;
	}

	/**
	* Only called if `IsFlushDiskWritesPerShot()` is set!
	* Called after each shot is finished and files have been flushed to disk. The returned data in
	* the params struct will have only the per-shot metadata for the just finished shot. Use
	* OnMoviePipelineFinished() if you need all ot the metadata.
	*/
	UPROPERTY(BlueprintAssignable, Category = "Movie Render Pipeline")
	FMoviePipelineWorkFinished OnMoviePipelineShotWorkFinishedDelegate;

	/**
	* Get the Master Configuration used to render this shot. This contains the global settings for the shot, as well as per-shot
	* configurations which can contain their own settings.
	*/
	UFUNCTION(BlueprintPure, Category = "Movie Render Pipeline")
	UMoviePipelineMasterConfig* GetPipelineMasterConfig() const;

public:
	ULevelSequence* GetTargetSequence() const { return TargetSequence; }

	UFUNCTION(BlueprintPure, Category = "Movie Render Pipeline")
	UTexture* GetPreviewTexture() const { return PreviewTexture; }

	void SetPreviewTexture(UTexture* InTexture) { PreviewTexture = InTexture; }

	const TArray<UMoviePipelineExecutorShot*>& GetActiveShotList() const { return ActiveShotList; }

	int32 GetCurrentShotIndex() const { return CurrentShotIndex; }
	const FMoviePipelineFrameOutputState& GetOutputState() const { return CachedOutputState; }

	UFUNCTION(BlueprintPure, Category = "Movie Render Pipeline")
	UMoviePipelineExecutorJob* GetCurrentJob() const { return CurrentJob; }

	EMovieRenderPipelineState GetPipelineState() const { return PipelineState; }
	FMoviePipelineOutputData GetOutputDataParams();

	void GetSidecarCameraData(UMoviePipelineExecutorShot* InShot, int32 InCameraIndex, FMinimalViewInfo& OutViewInfo, class UCameraComponent** OutCameraComponent) const;
	bool GetSidecarCameraViewPoints(UMoviePipelineExecutorShot* InShot, TArray<FVector>& OutSidecarViewLocations, TArray<FRotator>& OutSidecarViewRotations) const;

#if WITH_EDITOR
	const FMovieSceneExportMetadata& GetOutputMetadata() const { return OutputMetadata; }
#endif

public:
#if WITH_EDITOR
	void AddFrameToOutputMetadata(const FString& ClipName, const FString& ImageSequenceFileName, const FMoviePipelineFrameOutputState& FrameOutputState, const FString& Extension, const bool bHasAlpha);
#endif
	void AddOutputFuture(TFuture<bool>&& OutputFuture, const MoviePipeline::FMoviePipelineOutputFutureData& InData);

	void ProcessOutstandingFinishedFrames();
	void ProcessOutstandingFutures();
	void OnSampleRendered(TUniquePtr<FImagePixelData>&& OutputSample);
	const MoviePipeline::FAudioState& GetAudioState() const { return AudioState; }
	void SetFlushDiskWritesPerShot(const bool bFlushWrites) { bFlushDiskWritesPerShot = bFlushWrites; }
	bool IsFlushDiskWritesPerShot() const { return bFlushDiskWritesPerShot; }
public:
	template<typename SettingType>
	SettingType* FindOrAddSettingForShot(const UMoviePipelineExecutorShot* InShot) const
	{
		return (SettingType*)UMoviePipelineBlueprintLibrary::FindOrGetDefaultSettingForShot(SettingType::StaticClass(), GetPipelineMasterConfig(), InShot);
	}

	template<typename SettingType>
	TArray<SettingType*> FindSettingsForShot(const UMoviePipelineExecutorShot* InShot) const
	{
		TArray<SettingType*> Result;
		TArray<UMoviePipelineSetting*> FoundSettings = FindSettingsForShot(SettingType::StaticClass(), InShot);
		Result.Reserve(FoundSettings.Num());
		for (UMoviePipelineSetting* Setting : FoundSettings)
		{
			Result.Add(CastChecked<SettingType>(Setting));
		}
		return Result;
	}

	TArray<UMoviePipelineSetting*> FindSettingsForShot(TSubclassOf<UMoviePipelineSetting> InSetting, const UMoviePipelineExecutorShot* InShot) const;

	/**
	* Resolves the provided InFormatString by converting {format_strings} into settings provided by the master config.
	* @param	InFormatString		A format string (in the form of "{format_key1}_{format_key2}") to resolve.
	* @param	InFormatOverrides	A series of Key/Value pairs to override particular format keys. Useful for things that
	*								change based on the caller such as filename extensions.
	* @return	OutFinalPath			The final filepath based on a combination of the format string, the format overrides, and the current output state.
	* @return	OutFinalFormatArgs	The format arguments that were actually used to fill the format string (including file metadata)
	*
	* @param	InOutputState		(optional) The output state for frame information.
	* @param	InFrameNumberOffset	(optional) Frame offset of the frame we want the filename for, if not the current frame
	*								as specified in InOutputState
	*/
	void ResolveFilenameFormatArguments(const FString& InFormatString, const TMap<FString, FString>& InFormatOverrides, FString& OutFinalPath, FMoviePipelineFormatArgs& OutFinalFormatArgs, const FMoviePipelineFrameOutputState* InOutputState=nullptr, const int32 InFrameNumberOffset=0) const;

	/**
	* Allows initialization of some viewport-related arguments that aren't related to the job. Needs to
	* be called before the Initialize function. Optional.
	*/
	void SetViewportInitArgs(const UE::MoviePipeline::FViewportArgs& InArgs) { ViewportInitArgs = InArgs; }

protected:
	/**
	* This function should be called by the Executor when execution has finished (this should still be called in the event of an error)
	*/
	UFUNCTION(BlueprintCallable, Category = "Movie Render Pipeline")
	virtual void OnMoviePipelineFinishedImpl();
private:

	/** Instantiate our Debug UI Widget and initialize it to ourself. */
	void LoadDebugWidget();
	
	/** Called before the Engine ticks for the given frame. We use this to calculate delta times that the frame should use. */
	void OnEngineTickBeginFrame();
	
	/** Called after the Engine has ticked for a given frame. Everything in the world has been updated by now so we can submit things to render. */
	void OnEngineTickEndFrame();

	void ValidateSequenceAndSettings() const;


	/** Runs the per-tick logic when doing the ProducingFrames state. */
	void TickProducingFrames();

	void ProcessEndOfCameraCut(UMoviePipelineExecutorShot* InCameraCut);


	/** Called once when first moving to the Finalize state. */
	void BeginFinalize();
	/** Called once when first moving to the Export state. */
	void BeginExport();

	/** Attempts to start an Unreal Insights capture to a file on disk adjacent to the movie output. */
	void StartUnrealInsightsCapture();
	/** Attempts to stop an already started Unreal Insights capture. */
	void StopUnrealInsightsCapture();

	/** 
	* Runs the per-tick logic when doing the Finalize state.
	*
	* @param bInForceFinish		If true, this function will not return until all Output Containers say they have finalized.
	*/
	void TickFinalizeOutputContainers(const bool bInForceFinish);
	/** 
	* Runs the per-tick logic when doing the Export state. This is spread over multiple ticks to allow non-blocking background 
	* processes (such as extra encoding).
	*
	* @param bInForceFinish		If true, this function will not return until all exports say they have finished.
	*/
	void TickPostFinalizeExport(const bool bInForceFinish);

	/** Return true if we should early out of the TickProducingFrames function. Decrements the remaining number of steps when false. */
	bool DebugFrameStepPreTick();
	/** Returns true if we are idling because of debug frame stepping. */
	bool IsDebugFrameStepIdling() const;

	/** Debugging/Information. Don't hinge any logic on this as it will get called multiple times per frame in some cases. */
	void OnSequenceEvaluated(const UMovieSceneSequencePlayer& Player, FFrameTime CurrentTime, FFrameTime PreviousTime);

	/** Set up per-shot state for the specific shot, tearing down old state (if it exists) */
	void InitializeShot(UMoviePipelineExecutorShot* InShot);
	void TeardownShot(UMoviePipelineExecutorShot* InShot);

	/** Initialize the rendering pipeline for the given shot. This should not get called if rendering work is still in progress for a previous shot. */
	void SetupRenderingPipelineForShot(UMoviePipelineExecutorShot* InShot);
	/** Deinitialize the rendering pipeline for the given shot. */
	void TeardownRenderingPipelineForShot(UMoviePipelineExecutorShot* InShot);

	/** Flush any async resources in the engine that need to be finalized before submitting anything to the GPU, ie: Streaming Levels and Shaders */
	void FlushAsyncEngineSystems();

	/** If the log verbosity is high enough, prints out the files specified in the shot output data. */
	void PrintVerboseLogForFiles(const TArray<FMoviePipelineShotOutputData>& InOutputData) const;

	/** Tell our submixes to start capturing the data they are generating. Should only be called once output frames are being produced. */
	void StartAudioRecording();
	/** Tell our submixes to stop capturing the data, and then store a copy of it. */
	void StopAudioRecording();
	/** Attempt to process the audio thread work. This is complicated by our non-linear time steps */
	void ProcessAudioTick();
	void SetupAudioRendering();
	void TeardownAudioRendering();
	
	/** 
	* Renders the next frame in the Pipeline. This updates/ticks all scene view render states
	* and produces data. This may not result in an output frame due to multiple renders 
	* accumulating together to produce an output. frame.
	* Should not be called if we're idling (debug), not initialized yet, or finalizing/exporting. 
	*/
	void RenderFrame();

	/** Allow any Settings to modify the (already duplicated) sequence. This allows inserting automatic pre-roll, etc. */
	void ModifySequenceViaExtensions(ULevelSequence* InSequence);

	/**
	* Should the Progress UI be visible on the player's screen?
	*/
	void SetProgressWidgetVisible(bool bVisible);

	/** Returns list of render passes for a given shot */
	TArray<UMoviePipelineRenderPass*> GetAllRenderPasses(const UMoviePipelineExecutorShot* InShot);


private:
	/** Iterates through the changes we've made to a shot and applies the original settings. */
	void RestoreTargetSequenceToOriginalState();

	/** Initialize a new Level Sequence Actor to evaluate our target sequence. Disables any existing Level Sequences pointed at our original sequence. */
	void InitializeLevelSequenceActor();

	/** This builds the shot list from the target sequence, and expands Playback Bounds to cover any future evaluation we may need. */
	void BuildShotListFromSequence();

	/** 
	* Modifies the TargetSequence to ensure that only the specified Shot has it's associated Cinematic Shot Section enabled.
	* This way when Handle Frames are enabled and the sections are expanded, we don't end up evaluating the previous shot. 
	*/
	void SetSoloShot(UMoviePipelineExecutorShot* InShot);

	/* Expands the specified shot (and contained camera cuts)'s ranges for the given settings. */
	void ExpandShot(UMoviePipelineExecutorShot* InShot, const int32 InNumHandleFrames, const bool bIsPrePass);

	/** Calculates lots of useful numbers used in timing based off of the current shot. These are constant for a given shot. */
	MoviePipeline::FFrameConstantMetrics CalculateShotFrameMetrics(const UMoviePipelineExecutorShot* InShot) const;

	/** It can be useful to know where the data we're generating was relative to the original Timeline, so this calculates that. */
	void CalculateFrameNumbersForOutputState(const MoviePipeline::FFrameConstantMetrics& InFrameMetrics, const UMoviePipelineExecutorShot* InCameraCut, FMoviePipelineFrameOutputState& InOutOutputState) const;

	/** Handles transitioning between states, preventing reentrancy. Normal state flow should be respected, does not handle arbitrary x to y transitions. */
	void TransitionToState(const EMovieRenderPipelineState InNewState);

	void SetSkeletalMeshClothSubSteps(const int32 InSubdivisionCount);
	void RestoreSkeletalMeshClothSubSteps();

private:
	/** Custom TimeStep used to drive the engine while rendering. */
	UPROPERTY(Transient, Instanced)
	TObjectPtr<UMoviePipelineCustomTimeStep> CustomTimeStep;

	/** Custom Time Controller for the Sequence Player, used to match Custom TimeStep without any floating point accumulation errors. */
	TSharedPtr<FMoviePipelineTimeController> CustomSequenceTimeController;

	/** Hold a reference to the existing custom time step (if any) so we can restore it after we're done using our custom one. */
	UPROPERTY(Transient)
	TObjectPtr<UEngineCustomTimeStep> CachedPrevCustomTimeStep;

	/** This is our duplicated sequence that we're rendering. This will get modified throughout the rendering process. */
	UPROPERTY(Transient)
	TObjectPtr<ULevelSequence> TargetSequence;

	/** The Level Sequence Actor we spawned to play our TargetSequence. */
	UPROPERTY(Transient)
	TObjectPtr<ALevelSequenceActor> LevelSequenceActor;

	/** The Debug UI Widget that is spawned and placed on the player UI */
	UPROPERTY(Transient)
	TObjectPtr<UMovieRenderDebugWidget> DebugWidget;

	UPROPERTY(Transient)
	TObjectPtr<UTexture> PreviewTexture;

	/** A list of all of the shots we are going to render out from this sequence. */
	TArray<UMoviePipelineExecutorShot*> ActiveShotList;

	/** What state of the overall flow are we in? See enum for specifics. */
	EMovieRenderPipelineState PipelineState;

	/** What is the index of the shot we are working on. -1 if not initialized, may be greater than ShotList.Num() if we've reached the end. */
	int32 CurrentShotIndex;

	/** The time (in UTC) that Initialize was called. Used to track elapsed time. */
	FDateTime InitializationTime;

	FMoviePipelineFrameOutputState CachedOutputState;

	MoviePipeline::FAudioState AudioState;

	/** Cached state of GAreScreenMessagesEnabled. We disable them since some messages are written to the FSceneView directly otherwise. */
	bool bPrevGScreenMessagesEnabled;

	/** 
	* Have we hit the callback for the BeginFrame at least once? This solves an issue where the delegates
	* get registered mid-frame so you end up calling EndFrame before BeginFrame which is undesirable.
	*/
	bool bHasRunBeginFrameOnce;
	/** Should we pause the game at the end of the frame? Used to implement frame step debugger. */
	bool bPauseAtEndOfFrame;

	/** Should we flush outstanding work between each shot, ensuring all files are written to disk before we move on? */
	bool bFlushDiskWritesPerShot;

	/** True if RequestShutdown() was called. At the start of the next frame we will stop producing frames (if needed) and start shutting down. */
	FThreadSafeBool bShutdownRequested;

	/** True if an error or other event occured which halted frame production prematurely. */
	FThreadSafeBool bFatalError;

	/** True if we're in a TransitionToState call. Used to prevent reentrancy. */
	bool bIsTransitioningState;

	/** When using temporal sub-frame stepping common counts (such as 3) don't result in whole ticks. We keep track of how many ticks we lose so we can add them the next time there's a chance. */
	float AccumulatedTickSubFrameDeltas;

	/** Deprecated. */
	FMoviePipelineFinishedNative OnMoviePipelineFinishedDelegateNative;
	/** Called when we have completely finished. This object will call Shutdown before this and stop ticking. */
	FMoviePipelineWorkFinishedNative OnMoviePipelineWorkFinishedDelegateNative;

	/** Called when each shot has finished work if IsFlushDiskWritesPerShot() is set. */
	FMoviePipelineWorkFinishedNative OnMoviePipelineShotWorkFinishedDelegateNative;

	/**
	 * We have to apply camera motion vectors manually. So we keep the current and previous frame's camera view and rotation.
	 * Then we render a sequence of the same movement, and update after running the game sim.
	 **/
	MoviePipeline::FMoviePipelineFrameInfo FrameInfo;

public:

	/** This gathers all of the produced data for an output frame (which may come in async many frames later) before passing them onto the Output Containers. */
	TSharedPtr<FMoviePipelineOutputMerger, ESPMode::ThreadSafe> OutputBuilder;

	/** A debug image sequence writer in the event they want to dump every sample generated on its own. */
	IImageWriteQueue* ImageWriteQueue;

	/** Optional widget for feedback during render */
	UE_DEPRECATED(5.1, "Use SetViewportInitArgs instead.")
	UPROPERTY(Transient)
	TSubclassOf<UMovieRenderDebugWidget> DebugWidgetClass;

private:
	/** Keep track of which job we're working on. This holds our Configuration + which shots we're supposed to render from it. */
	UPROPERTY(Transient)
	TObjectPtr<UMoviePipelineExecutorJob> CurrentJob;

#if WITH_EDITOR
	/** Keep track of clips we've exported, for building FCPXML and other project files */
	FMovieSceneExportMetadata OutputMetadata;
#endif
	/** Keeps track of files written for each shot so it can be retrieved later via scripting for post-processing. */
	TArray<FMoviePipelineShotOutputData> GeneratedShotOutputData;

	/** Files that we've requested be written to disk but have not yet finished writing. */
	TArray<FMoviePipelineOutputFuture> OutputFutures;

	UE::MoviePipeline::FViewportArgs ViewportInitArgs;

	TSharedPtr<MoviePipeline::FCameraCutSubSectionHierarchyNode> CachedSequenceHierarchyRoot;

	struct FClothSimSettingsCache
	{
		int32 NumSubSteps;
	};

	TMap<TWeakObjectPtr<class UClothingSimulationInteractor>, FClothSimSettingsCache> ClothSimCache;

	struct FRenderTimeStatistics
	{
		FDateTime StartTime;
		FDateTime EndTime;
	};

	TMap<int32, FRenderTimeStatistics> RenderTimeFrameStatistics;

public:
	static FString DefaultDebugWidgetAsset;
};

UCLASS()
class UMoviePipelineCustomTimeStep : public UEngineCustomTimeStep
{
	GENERATED_BODY()

public:
	// UEngineCustomTimeStep Interface
	virtual bool Initialize(UEngine* InEngine) override { return true; }
	virtual void Shutdown(UEngine* InEngine) override {}
	virtual bool UpdateTimeStep(UEngine* InEngine) override;
	virtual ECustomTimeStepSynchronizationState GetSynchronizationState() const override { return ECustomTimeStepSynchronizationState::Synchronized; }
	// ~UEngineCustomTimeStep Interface

	void SetCachedFrameTiming(const MoviePipeline::FFrameTimeStepCache& InTimeCache);

private:
	/** We don't do any thinking on our own, instead we just spit out the numbers stored in our time cache. */
	MoviePipeline::FFrameTimeStepCache TimeCache;
};

struct FMoviePipelineTimeController : public FMovieSceneTimeController
{
	virtual FFrameTime OnRequestCurrentTime(const FQualifiedFrameTime& InCurrentTime, float InPlayRate) override;
	void SetCachedFrameTiming(const FQualifiedFrameTime& InTimeCache) { TimeCache = InTimeCache; }

private:
	/** Simply store the number calculated and return it when requested. */
	FQualifiedFrameTime TimeCache;
};
