// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/MovieGraphPipeline.h"
#include "Graph/MovieGraphDataTypes.h"
#include "Graph/MovieGraphOutputMerger.h"
#include "Graph/Nodes/MovieGraphFileOutputNode.h"
#include "MovieRenderPipelineCoreModule.h"
#include "Misc/CoreDelegates.h"
#include "MoviePipelineQueue.h"
#include "MovieScene.h"
#include "RenderingThread.h"
#include "ImageWriteQueue.h"
#include "Modules/ModuleManager.h"

// Temp
#include "LevelSequence.h"

UMovieGraphPipeline::UMovieGraphPipeline()
	: CurrentShotIndex(-1)
	, bIsTransitioningState(false)
	, PipelineState(EMovieRenderPipelineState::Uninitialized)
{
	OutputMerger = MakeShared<UE::MovieGraph::FMovieGraphOutputMerger>(this);

	Debug_ImageWriteQueue = &FModuleManager::Get().LoadModuleChecked<IImageWriteQueueModule>("ImageWriteQueue").GetWriteQueue();
}

void UMovieGraphPipeline::Initialize(UMoviePipelineExecutorJob* InJob, const FMovieGraphInitConfig& InitConfig)
{
	UE_LOG(LogMovieRenderPipeline, Log, TEXT("Initializing MovieGraph Render"));
	if (!ensureAlwaysMsgf(InJob, TEXT("MovieGraph cannot be initialized with a null job. Aborting.")))
	{
		//Shutdown(true);
		return;
	}

	if (!ensureAlwaysMsgf(InJob->GetGraphConfig(), TEXT("MoviePipeline cannot be initialized with a null job configuration. Make sure you've created a Graph Config for this job (or use the regular UMoviePipeline instead of UMovieGraphPipeline). Aborting.")))
	{
		//Shutdown(true);
		return;
	}

	// ToDo: If we cache presets into the job, update the job config here. I don't think we want to do that though
	// due to the recursive nature of linked graphs.
	// But we could (should?) at least copy the primary preset asset into the primary config slot.

	if (!ensureAlwaysMsgf(PipelineState == EMovieRenderPipelineState::Uninitialized, TEXT("Pipeline cannot be reused. Create a new pipeline to execute a job.")))
	{
		Shutdown(true);
		return;
	}

	// Ensure this object has the World as part of its Outer (so that it has context to spawn things)
	if (!ensureAlwaysMsgf(GetWorld(), TEXT("Pipeline does not contain the world as an outer.")))
	{
		Shutdown(true);
		return;
	}


	// Register any additional engine callbacks needed.
	{
		// Called before the Custom Timestep is updated. This gives us a chance to calculate
		// what we want the frame to look like and then cache that information so that the
		// Custom Timestep doesn't have to perform its own logic.
		FCoreDelegates::OnBeginFrame.AddUObject(this, &UMovieGraphPipeline::OnEngineTickBeginFrame);
		// Called at the end of the frame after everything has been ticked and rendered for the frame.
		FCoreDelegates::OnEndFrame.AddUObject(this, &UMovieGraphPipeline::OnEngineTickEndFrame);
	}

	// Create instances of our different classes from the InitConfig
	GraphTimeStepInstance = NewObject<UMovieGraphTimeStepBase>(this, InitConfig.TimeStepClass);
	GraphRendererInstance = NewObject<UMovieGraphRendererBase>(this, InitConfig.RendererClass);
	GraphDataSourceInstance = NewObject<UMovieGraphDataSourceBase>(this, InitConfig.DataSourceClass);
	
	CurrentJob = InJob;
	CurrentShotIndex = 0;
	GraphInitializationTime = FDateTime::UtcNow();

	// Now that we've created our various systems, we will start using them. First thing we do is cache data about
	// the world, job, player viewport, etc, before we make any modifications. These will be restored at the end
	// of the render.
	GraphDataSourceInstance->CacheDataPreJob(InitConfig);

	// Construct a debug UI and bind it to this instance.
	// LoadDebugWidget();
	// SetupAudioRendering();

	// Update our list of shots from our data source, and then
	// create our list of active shots, so we don't try to render
	// a shot the user has deactivated.
	BuildShotListFromDataSource();

	//for (ULevelStreaming* Level : GetWorld()->GetStreamingLevels())
	//{
	//	UClass* StreamingClass = Level->GetClass();
	//
	//	if (StreamingClass == ULevelStreamingDynamic::StaticClass())
	//	{
	//		const FString NonPrefixedLevelName = UWorld::StripPIEPrefixFromPackageName(Level->GetWorldAssetPackageName(), GetWorld()->StreamingLevelsPrefix);
	//		UE_LOG(LogMovieRenderPipeline, Warning, TEXT("Sub-level '%s' is set to blueprint streaming and will not be visible during a render unless a Sequencer Visibility Track controls its visibility or you have written other code to handle loading it."),
	//			*NonPrefixedLevelName);
	//	}
	//}

	// ToDo: Print some information, Job Name, Level Sequence, etc.
	UE_LOG(LogMovieRenderPipeline, Log, TEXT("Finished initializing MovieGraph Render"));


	// If the shot mask entirely disabled everything we'll transition directly to finish as there is no work to do.
	if (ActiveShotList.Num() == 0)
	{
		// We have to transition twice as Uninitialized -> n state is a no-op, so the second tick will take us to Finished which shuts down.
		UE_LOG(LogMovieRenderPipeline, Warning, TEXT("MovieGraph No shots detected to render. Either all outside playback range, or disabled via shot mask, bailing."));
	
		TransitionToState(EMovieRenderPipelineState::Export);
		TransitionToState(EMovieRenderPipelineState::Finished);
	}
	else
	{
		TransitionToState(EMovieRenderPipelineState::ProducingFrames);
	}
}

void UMovieGraphPipeline::BuildShotListFromDataSource()
{
	// Synchronize our shot list with our target data source. New shots will be added and outdated shots removed.
	// Shots that are already in the list will be updated but their enable flag will be respected. 
	GraphDataSourceInstance->UpdateShotList();

	for (UMoviePipelineExecutorShot* Shot : GetCurrentJob()->ShotInfo)
	{
		Shot->ShotInfo.CurrentTickInRoot = Shot->ShotInfo.TotalOutputRangeRoot.GetLowerBoundValue();
	}


	// The active shot-list is a subset of the whole shot-list; The ShotInfo contains information about every range it detected to render
	// but if the user has turned the shot off in the UI then we don't want to render it.
	ActiveShotList.Empty();
	for (UMoviePipelineExecutorShot* Shot : GetCurrentJob()->ShotInfo)
	{
		if (Shot->ShouldRender())
		{
			ActiveShotList.Add(Shot);
		}
	}
}

void UMovieGraphPipeline::OnEngineTickBeginFrame()
{
	LLM_SCOPE_BYNAME(TEXT("MovieGraphBeginFrame"));


	UE_LOG(LogMovieRenderPipeline, VeryVerbose, TEXT("MovieGraph OnEngineTickBeginFrame"));
	switch (PipelineState)
	{
	case EMovieRenderPipelineState::Uninitialized:
		// We shouldn't register this delegate until we're initialized.
		check(false);
		break;
	case EMovieRenderPipelineState::ProducingFrames:
		TickProducingFrames();
		break;
	case EMovieRenderPipelineState::Finalize:
		// Don't flush the finalize to keep the UI responsive.
		TickFinalizeOutputContainers(false);
		break;
	case EMovieRenderPipelineState::Export:
		// Don't flush the export to keep the UI responsive.
		TickPostFinalizeExport(false);
		break;
	}
}

void UMovieGraphPipeline::TickProducingFrames()
{
	check(GraphTimeStepInstance);

	// Move any output frames that have been finished from the Output Merger
	// into the actual outputs. This will generate new futures (for actual 
	// disk writes) which we keep track of below.
	ProcessOutstandingFinishedFrames();
	
	// Process any files that have finished writing to disk and push them into our list of 
	// files made by this shot.
	ProcessOutstandingFutures();

	// The callback for this function does not get registered until Initialization has been called, which sets
	// the state to Render. If it's not, we have a initialization order/flow issue!
	//check(PipelineState == EMovieRenderPipelineState::ProducingFrames);

	// We should not be calling this once we have completed all the shots.
	//check(CurrentShotIndex >= 0 && CurrentShotIndex < ActiveShotList.Num());
	//
	//ProcessOutstandingFutures();
	//
	//if (bShutdownRequested)
	//{
	//	UE_LOG(LogMovieRenderPipeline, Log, TEXT("MovieGraph TickProductingFrames: Async Shutdown Requested, abandoning remaining work and moving to Finalize."));
	//	TransitionToState(EMovieRenderPipelineState::Finalize);
	//	return;
	//}

		// When start up we want to override the engine's Custom Timestep with our own.
	// This gives us the ability to completely control the engine tick/delta time before the frame
	// is started so that we don't have to always be thinking of delta times one frame ahead. We need
	// to do this only once we're ready to set the timestep though, as Initialize can be called as
	// a result of a OnBeginFrame, meaning that Initialize is called on the frame before TickProducingFrames
	// so there would be one frame where it used the custom timestep (after initialize) before TPF was called.
	//if (GEngine->GetCustomTimeStep() != CustomTimeStep)
	//{
	//	CachedPrevCustomTimeStep = GEngine->GetCustomTimeStep();
	//	GEngine->SetCustomTimeStep(CustomTimeStep);
	//}


	GraphTimeStepInstance->TickProducingFrames();
}


void UMovieGraphPipeline::TickFinalizeOutputContainers(const bool bInForceFinish)
{
	// Tick all containers until they all report that they have finalized.
	bool bAllContainsFinishedProcessing;

	while(true)
	{
		bAllContainsFinishedProcessing = true;

		for (const TObjectPtr<UMovieGraphFileOutputNode>& Node : GetOutputNodesUsed())
		{
			bAllContainsFinishedProcessing &= Node->IsFinishedWritingToDisk();

		}
	
		// If we aren't forcing a finish, early out after one loop to keep
		// the editor/ui responsive.
		if (!bInForceFinish || bAllContainsFinishedProcessing)
		{
			break;
		}
	
		// If they've reached here, they're forcing them to finish so we'll sleep for a touch to give
		// everyone a chance to actually do work before asking them if they're done.
		FPlatformProcess::Sleep(0.1f);
	
	}

	// If an output container is still working, we'll early out to keep the UI responsive.
	// If they've forced a finish this will have to be true before we can reach this block.
	if (!bAllContainsFinishedProcessing)
	{
		return;
	}

	//TArray<UMoviePipelineOutputBase*> Settings = GetPipelinePrimaryConfig()->GetOutputContainers();
	//Algo::SortBy(Settings, [](const UMoviePipelineOutputBase* Setting) { return Setting->GetPriority(); });
	//for (UMoviePipelineOutputBase* Container : Settings)
	//{
	//	// All containers have finished processing, final shutdown.
	//	Container->Finalize();
	//}

	TransitionToState(EMovieRenderPipelineState::Export);
}

void UMovieGraphPipeline::TickPostFinalizeExport(const bool bInForceFinish)
{
	// This step assumes you have produced data and filled the data structures.
	check(PipelineState == EMovieRenderPipelineState::Export);
	UE_LOG(LogMovieRenderPipeline, Verbose, TEXT("[%d] PostFinalize Export (Start)."), GFrameCounter);

	// ToDo: Loop through any extensions (such as XML export) and let them export using all of the
	// data that was generated during this run such as containers, output names and lengths.
	// Tick all containers until they all report that they have finalized.
	bool bAllContainsFinishedProcessing = true;

	//do
	//{
	//	bAllContainsFinishedProcessing = true;
	//
	//	// Ask the containers if they're all done processing.
	//	for (UMoviePipelineSetting* Setting : GetPipelinePrimaryConfig()->GetAllSettings())
	//	{
	//		bAllContainsFinishedProcessing &= Setting->HasFinishedExporting();
	//	}
	//
	//	// If we aren't forcing a finish, early out after one loop to keep
	//	// the editor/ui responsive.
	//	if (!bInForceFinish || bAllContainsFinishedProcessing)
	//	{
	//		break;
	//	}
	//
	//	// If they've reached here, they're forcing them to finish so we'll sleep for a touch to give
	//	// everyone a chance to actually do work before asking them if they're done.
	//	FPlatformProcess::Sleep(1.f);
	//
	//} while (true);

	UE_LOG(LogMovieRenderPipeline, Verbose, TEXT("[%d] PostFinalize Export (End)."), GFrameCounter);

	// If an output container is still working, we'll early out to keep the UI responsive.
	// If they've forced a finish this will have to be true before we can reach this block.
	if (!bAllContainsFinishedProcessing)
	{
		return;
	}

	TransitionToState(EMovieRenderPipelineState::Finished);
}

void UMovieGraphPipeline::BeginFinalize()
{
	// Notify our output nodes that no more frames will be submitted. This allows
	// them to put fences into queues for file writes, etc.
	for (const TObjectPtr<UMovieGraphFileOutputNode>& Node : GetOutputNodesUsed())
	{
		Node->OnAllFramesSubmitted();
	}
}

void UMovieGraphPipeline::SetupShot(UMoviePipelineExecutorShot* InShot)
{
	// Set the new shot as the active shot. This enables the specified shot section and disables all other shot sections.
	// SetSoloShot(InShot);

	// Loop through just our primary settings and let them know which shot we're about to start.
	//TArray<UMoviePipelineSetting*> Settings = GetPipelinePrimaryConfig()->GetAllSettings();
	//Algo::SortBy(Settings, [](const UMoviePipelineSetting* Setting) { return Setting->GetPriority(); });
	//for (UMoviePipelineSetting* Setting : Settings)
	//{
	//	Setting->OnSetupForShot(InShot);
	//}
	//
	//if (InShot->GetShotOverrideConfiguration() != nullptr)
	//{
	//	// Any shot-specific overrides haven't had first time initialization. So we'll do that now.
	//	TArray<UMoviePipelineSetting*> ShotSettings = InShot->GetShotOverrideConfiguration()->GetUserSettings();
	//	Algo::SortBy(ShotSettings, [](const UMoviePipelineSetting* Setting) { return Setting->GetPriority(); });
	//	for (UMoviePipelineSetting* Setting : ShotSettings)
	//	{
	//		Setting->OnMoviePipelineInitialized(this);
	//	}
	//}

	// Setup required rendering architecture for all passes in this shot.
	GraphRendererInstance->SetupRenderingPipelineForShot(InShot);
}

void UMovieGraphPipeline::TeardownShot(UMoviePipelineExecutorShot* InShot)
{
	// Teardown any rendering architecture for this shot. This needs to happen first because it'll flush outstanding rendering commands
	GraphRendererInstance->TeardownRenderingPipelineForShot(InShot);

	// some other stuff

	CurrentShotIndex++;

	// Check to see if this was the last shot in the Pipeline, otherwise on the next
	// tick the new shot will be initialized and processed.
	if (CurrentShotIndex >= ActiveShotList.Num())
	{
		UE_LOG(LogMovieRenderPipeline, Log, TEXT("MovieGraph Finished rendering last shot. Moving to Finalize to finish writing items to disk."));
		TransitionToState(EMovieRenderPipelineState::Finalize);
	}
}

void UMovieGraphPipeline::OnEngineTickEndFrame()
{
	LLM_SCOPE_BYNAME(TEXT("MovieGraphEndFrame"));

	// Don't try to submit anything to the renderer if the shot isn't initialized yet, or has
	// finished. We tick the engine when transitioning between shot states.
	EMovieRenderShotState CurrentShotState = GetActiveShotList()[GetCurrentShotIndex()]->ShotInfo.State;
	if (CurrentShotState == EMovieRenderShotState::Uninitialized || 
		CurrentShotState == EMovieRenderShotState::Finished)
	{
		return;
	}

	UE_LOG(LogMovieRenderPipeline, VeryVerbose, TEXT("MovieGraph OnEngineTickEndFrame (Start)"));

	// ProcessAudioTick();
	RenderFrame();

	UE_LOG(LogMovieRenderPipeline, VeryVerbose, TEXT("MovieGraph OnEngineTickEndFrame (End)"));
}

void UMovieGraphPipeline::RenderFrame()
{
	check(GraphRendererInstance);
	const FMovieGraphTimeStepData& TimeStepData = GraphTimeStepInstance->GetCalculatedTimeData();

	GraphRendererInstance->Render(TimeStepData);
}

void UMovieGraphPipeline::RequestShutdownImpl(bool bIsError)
{
	// It's possible for a previous call to RequestionShutdown to have set an error before this call that may not
	// We don't want to unset a previously set error state
	if (bIsError)
	{
		bShutdownSetErrorFlag = true;
	}

	// The user has requested a shutdown, it will be read the next available chance and possibly acted on.
	bShutdownRequested = true;
	switch (PipelineState)
	{
		// It is valid to call Shutdown at any point during these two states.
	case EMovieRenderPipelineState::Uninitialized:
	case EMovieRenderPipelineState::ProducingFrames:
		break;
		// You can call Shutdown during these two, but they won't do anything as we're already shutting down at that point.
	case EMovieRenderPipelineState::Finalize:
	case EMovieRenderPipelineState::Export:
		UE_LOG(LogMovieRenderPipeline, Log, TEXT("MovieGraph Async Shutdown Requested, ignoring due to already being on the way to shutdown."));
		break;
	}
}

void UMovieGraphPipeline::ShutdownImpl(bool bIsError)
{
	check(IsInGameThread());

	// We flag this so you can check if the shutdown was requested even when we do a stall-stop.
	bShutdownRequested = true;

	// It's possible for a previous call to RequestShutdown to have set an error before this call and
	// we don't want to just blow away the error flag even if this function was then called normally.
	// (ie: Don't accidnetally unset error state)
	if (bIsError)
	{
		bShutdownSetErrorFlag = true;
	}

	// This is a blocking operation which abandons any outstanding work to be submitted but finishes
	// the existing work already processed.
	if (PipelineState == EMovieRenderPipelineState::Uninitialized)
	{
		// If initialize is not called, no need to do anything.
		return;
	}

	if (PipelineState == EMovieRenderPipelineState::Finished)
	{
		// If already shut down, no need to do anything.
		return;
	}

	if (PipelineState == EMovieRenderPipelineState::ProducingFrames)
	{
		// Teardown the currently active shot (if there is one). This will flush any outstanding rendering
		// work that has already submitted - it cannot be canceled, so we may as well execute it and save the results.
		TransitionToState(EMovieRenderPipelineState::Finalize);

		// Abandon the current frame. When using temporal sampling we may had canceled mid-frame, so the rendering
		// commands were never submitted, thus the output builder will still be expecting a frame to come in.
		//if (CachedOutputState.TemporalSampleCount > 1)
		//{
		//	OutputBuilder->AbandonOutstandingWork();
		//}
	}

	if (PipelineState == EMovieRenderPipelineState::Finalize)
	{
		// We were either in the middle of writing frames to disk, or we have moved to Finalize as a result of the above block.
		// Tick output containers until they report they have finished writing to disk. This is a blocking operation. 
		// Finalize automatically switches our state to Export so no need to manually transition afterwards.
		//TickFinalizeOutputContainers(true);
	}

	if (PipelineState == EMovieRenderPipelineState::Export)
	{
		// All frames have been written to disk but we're doing a post-export step (such as encoding). Flush this operation as well.
		// Export automatically switches our state to Finished so no need to manually transition afterwards.
		//TickPostFinalizeExport(true);
	}
}

void UMovieGraphPipeline::TransitionToState(const EMovieRenderPipelineState InNewState)
{
	// No re-entrancy. This isn't an error as tearing down a shot may try to move to
	// Finalize on its own, but we don't want that.
	if (bIsTransitioningState)
	{
		return;
	}

	TGuardValue<bool> StateTransitionGuard(bIsTransitioningState, true);

	bool bInvalidTransition = true;
	switch (PipelineState)
	{
	case EMovieRenderPipelineState::Uninitialized:
		PipelineState = InNewState;
		bInvalidTransition = false;
		break;
	case EMovieRenderPipelineState::ProducingFrames:
		if (InNewState == EMovieRenderPipelineState::Finalize)
		{
			bInvalidTransition = false;

			// If we had naturally finished the last shot before doing this transition it will have
			// already been torn down, so this only catches mid-shot transitions to ensure teardown.
			if (CurrentShotIndex < ActiveShotList.Num())
			{
				// Ensures all in-flight work for that shot is handled.
				TeardownShot(ActiveShotList[CurrentShotIndex]);
			}

			// Unregister our OnEngineTickEndFrame delegate. We don't unregister BeginFrame as we need
			// to continue to call it to allow ticking the Finalization stage.
			FCoreDelegates::OnEndFrame.RemoveAll(this);

			// Restore any custom Time Step that may have been set before. We do this here
			// because the TimeStepInstance is only expected to be having to calculate times
			// during ProducingFrames.
			GetTimeStepInstance()->Shutdown();

			// Ensure all frames have been processed by the GPU and sent to the Output Merger
			FlushRenderingCommands();

			// And then make sure all frames are sent to the Output Containers before we finalize.
			ProcessOutstandingFinishedFrames();

			// PreviewTexture = nullptr;

			// This is called once notifying output containers that all frames that will be submitted have been submitted.
			PipelineState = EMovieRenderPipelineState::Finalize;
			BeginFinalize();
		}
		break;
	case EMovieRenderPipelineState::Finalize:
		if (InNewState == EMovieRenderPipelineState::Export)
		{
			bInvalidTransition = false;

			// This is called once notifying our export step that they can begin the export.
			PipelineState = EMovieRenderPipelineState::Export;



			// Restore the sequence so that the export processes can operate on the original sequence. 
			// This is also done in the finished state because it's not guaranteed that the Export state 
			// will be set when the render is canceled early
			// LevelSequenceActor->GetSequencePlayer()->Stop();
			// RestoreTargetSequenceToOriginalState();

			// Ensure all of our Futures have been converted to the GeneratedOutputData. This has to happen
			// after finalize finishes, because the futures won't be available until actually written to disk.
			ProcessOutstandingFutures();

			//BeginExport();
		}
		break;
	case EMovieRenderPipelineState::Export:
		if (InNewState == EMovieRenderPipelineState::Finished)
		{
			bInvalidTransition = false;
			PipelineState = EMovieRenderPipelineState::Finished;

			// Uninitialize our primary config settings. Reverse sorted so settings that cached values restore correctly.
			//TArray<UMoviePipelineSetting*> Settings = GetPipelinePrimaryConfig()->GetAllSettings();
			//Algo::SortBy(Settings, [](const UMoviePipelineSetting* Setting) { return Setting->GetPriority(); }, TLess<int32>());
			//for (UMoviePipelineSetting* Setting : Settings)
			//{
			//	Setting->OnMoviePipelineShutdown(this);
			//}



			// Ensure our delegates don't get called anymore as we're going to become null soon.
			FCoreDelegates::OnBeginFrame.RemoveAll(this);
			FCoreDelegates::OnEndFrame.RemoveAll(this);

			//if (DebugWidget)
			//{
			//	DebugWidget->RemoveFromParent();
			//	DebugWidget = nullptr;
			//}

			//TArray<UMoviePipelineOutputBase*> ContainerSettings = GetPipelinePrimaryConfig()->GetOutputContainers();
			//Algo::SortBy(ContainerSettings, [](const UMoviePipelineOutputBase* Setting) { return Setting->GetPriority(); });
			//for (UMoviePipelineOutputBase* Setting : ContainerSettings)
			//{
			//	Setting->OnPipelineFinished();
			//}
			//
			//TeardownAudioRendering();
			//LevelSequenceActor->GetSequencePlayer()->Stop();
			//RestoreTargetSequenceToOriginalState();
			//
			//if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
			//{
			//	Viewport->bDisableWorldRendering = false;
			//}

			// Because the render target pool is shared, if you had a high-resolution render in editor the entire gbuffer
			// has been resized up to match the new maximum extent. This console command will reset the size of the pool
			// and cause it to re-allocate at the currrent size on the next render request, which is likely to be the size
			// of the PIE window (720p) or the Viewport itself.
			//UKismetSystemLibrary::ExecuteConsoleCommand(GetWorld(), TEXT("r.ResetRenderTargetsExtent"), nullptr);
			//CustomTimeStep->RestoreCachedWorldSettings();

			//GAreScreenMessagesEnabled = bPrevGScreenMessagesEnabled;

			//UE_LOG(LogMovieRenderPipeline, Log, TEXT("Movie Pipeline completed. Duration: %s"), *(FDateTime::UtcNow() - InitializationTime).ToString());

			//UMoviePipelineDebugSettings* DebugSetting = FindOrAddSettingForShot<UMoviePipelineDebugSettings>(nullptr);
			//if (DebugSetting)
			//{
			//	if (DebugSetting->bCaptureUnrealInsightsTrace)
			//	{
			//		StopUnrealInsightsCapture();
			//	}
			//}

			OutputNodesDataSentTo.Reset();

			OnMoviePipelineFinishedImpl();
			UE_LOG(LogMovieRenderPipeline, Log, TEXT("Movie Graph Render completed. Duration: %s"), *(FDateTime::UtcNow() - GraphInitializationTime).ToString());
		}
		break;
	}

	if (!ensureAlwaysMsgf(!bInvalidTransition, TEXT("MovieGraph An invalid transition was requested (from: %d to: %d), ignoring transition request."),
		PipelineState, InNewState))
	{
		return;
	}
}

void UMovieGraphPipeline::OnMoviePipelineFinishedImpl()
{
	// Broadcast to both Native and Python/BP
	//
	// Generate a params struct containing the data generated by this job.
	FMoviePipelineOutputData Params;
	//Params.Pipeline = this;
	//Params.Job = GetCurrentJob();
	//Params.bSuccess = !bFatalError;
	//Params.ShotData = GeneratedShotOutputData;
	//
	//UE_LOG(LogMovieRenderPipelineIO, Verbose, TEXT("Files written to disk for entire sequence:"));
	//PrintVerboseLogForFiles(GeneratedShotOutputData);
	//UE_LOG(LogMovieRenderPipelineIO, Verbose, TEXT("Completed outputting files written to disk."));
	//
	OnMoviePipelineWorkFinishedDelegateNative.Broadcast(Params);
	// OnMoviePipelineWorkFinishedDelegate.Broadcast(Params);
}

//void UMovieGraphPipeline::PrintVerboseLogForFiles(const TArray<FMoviePipelineShotOutputData>& InOutputData) const
//{
//	for (const FMoviePipelineShotOutputData& OutputData : InOutputData)
//	{
//		const UMoviePipelineExecutorShot* Shot = OutputData.Shot.Get();
//		if (Shot)
//		{
//			UE_LOG(LogMovieRenderPipelineIO, Verbose, TEXT("Shot: %s [%s]"), *Shot->OuterName, *Shot->InnerName);
//		}
//		for (const TPair<FMoviePipelinePassIdentifier, FMoviePipelineRenderPassOutputData>& Pair : OutputData.RenderPassData)
//		{
//			UE_LOG(LogMovieRenderPipelineIO, Verbose, TEXT("Render Pass: %s"), *Pair.Key.Name);
//			for (const FString& FilePath : Pair.Value.FilePaths)
//			{
//				UE_LOG(LogMovieRenderPipelineIO, Verbose, TEXT("\t\t%s"), *FilePath);
//			}
//		}
//	}
//}

void UMovieGraphPipeline::ProcessOutstandingFinishedFrames()
{
	while (!OutputMerger->GetFinishedFrames().IsEmpty())
	{
		UE::MovieGraph::FMovieGraphOutputMergerFrame OutputFrame;
		OutputMerger->GetFinishedFrames().Dequeue(OutputFrame);

		UE::MovieGraph::FRenderTimeStatistics* TimeStats = GetRendererInstance()->GetRenderTimeStatistics(OutputFrame.TraversalContext.Time.OutputFrameNumber);
		if (ensure(TimeStats))
		{
			TimeStats->EndTime = FDateTime::UtcNow();

			int32 FrameNumber = OutputFrame.TraversalContext.Time.OutputFrameNumber;
			FString DurationTimeStr = (TimeStats->EndTime - TimeStats->StartTime).ToString();
			// UE_LOG(LogTemp, Log, TEXT("Frame: %d Duration: %s"), FrameNumber, *DurationTimeStr);
		}

		// How we choose which image gets sent to which output container is a little tricky. We use the CDO of each node type on purpose
		// as the graph can change node instances every frame. If a output type node is in the "Globals" graph, we assume that we should
		// send all rendered layers to the image type. However we should also allow placing an output of a given type in a render layer,
		// so you could choose to send the layer to only some output types (ie: only send the "beauty" layer to the .jpeg container).
		// Because our data is always treated as a block of all image data (and is non-copyable) we instead generate both a list of
		// output containers for the data in this output frame, and a mask which lets that container know if it should skip a layer.
		TMap<UMovieGraphFileOutputNode*, TSet<FMovieGraphRenderDataIdentifier>> MaskData;

		for (TPair<FMovieGraphRenderDataIdentifier, TUniquePtr<FImagePixelData>>& RenderData : OutputFrame.ImageOutputData)
		{
			UE::MovieGraph::FMovieGraphSampleState* Payload = RenderData.Value->GetPayload<UE::MovieGraph::FMovieGraphSampleState>();
			if (!ensure(Payload))
			{
				continue;
			}

			if (!ensure(OutputFrame.EvaluatedConfig))
			{
				continue;
			}

			// Get a list of all output nodes for this particular render layer. We specifically skip CDOs here because we're just trying
			// to find out which output nodes the user _wanted_ to place items onto, we later collect only the CDOs so that we have
			// a central point for actually handling the file writing.
			const bool bIncludeCDOs = false;
			const FName BranchName = RenderData.Key.RootBranchName;
			TArray<UMovieGraphFileOutputNode*> OutputNodeInstances = OutputFrame.EvaluatedConfig->GetSettingsForBranch<UMovieGraphFileOutputNode>(BranchName, bIncludeCDOs);
			for (const UMovieGraphFileOutputNode* Instance : OutputNodeInstances)
			{
				UMovieGraphFileOutputNode* CDO = Instance->GetClass()->GetDefaultObject<UMovieGraphFileOutputNode>();
				MaskData.FindOrAdd(CDO).Add(RenderData.Key);
			}
		}

		// Now that we've looped through the above, we have the total list of which output formats are being used by the graph for
		// all of the render layers given. We also have a list of which identifiers should go into each one. So we can loop through
		// the CDO instances and pass the data to them.
		UE_LOG(LogMovieRenderPipeline, Warning, TEXT("File Outputs:"));
		for (const TPair<UMovieGraphFileOutputNode*, TSet<FMovieGraphRenderDataIdentifier>>& Pair : MaskData)
		{
			UE_LOG(LogMovieRenderPipeline, Warning, TEXT("\tNode: %s"), *Pair.Key->GetClass()->GetName());
			for (const FMovieGraphRenderDataIdentifier& ID : Pair.Value)
			{
				UE_LOG(LogMovieRenderPipeline, Warning, TEXT("\t\tBranch: %s:"), *ID.RootBranchName.ToString());
			}

			Pair.Key->OnReceiveImageData(this, &OutputFrame, Pair.Value);

			// Ensure we keep track of which nodes we actually sent data to during this render
			// so that we can call BeginFinalize/IsFinishedWritingToDisk on them when shutting down.
			OutputNodesDataSentTo.Add(Pair.Key);
		}
	}
}

void UMovieGraphPipeline::AddOutputFuture(TFuture<bool>&& InOutputFuture, const UE::MovieGraph::FMovieGraphOutputFutureData& InData)
{
	OutstandingOutputFutures.Add(
		UE::MovieGraph::FMovieGraphOutputFuture(MoveTemp(InOutputFuture), InData)
	);
}

void UMovieGraphPipeline::ProcessOutstandingFutures()
{
	// Check if any frames failed to output
	TArray<int32> CompletedOutputFutures;
	for (int32 Index = 0; Index < OutstandingOutputFutures.Num(); ++Index)
	{
		// Output futures are pushed in order to the OutputFutures array. However they are
		// completed asyncronously, so we don't process any futures after a not-yet-ready one
		// otherwise we push into the GeneratedShotOutputData array out of order.
		const UE::MovieGraph::FMovieGraphOutputFuture& OutputFuture = OutstandingOutputFutures[Index];
		if (!OutputFuture.Get<0>().IsReady())
		{
			break;
		}

		CompletedOutputFutures.Add(Index);

		const UE::MovieGraph::FMovieGraphOutputFutureData& FutureData = OutputFuture.Get<1>();

		// The future was completed, time to add it to our shot output data.
		FMovieGraphRenderOutputData* ShotOutputData = nullptr;
		for (int32 OutputDataIndex = 0; OutputDataIndex < GeneratedOutputData.Num(); OutputDataIndex++)
		{
			if (FutureData.Shot == GeneratedOutputData[OutputDataIndex].Shot)
			{
				ShotOutputData = &GeneratedOutputData[OutputDataIndex];
			}
		}

		if (!ShotOutputData)
		{
			GeneratedOutputData.Add(FMovieGraphRenderOutputData());
			ShotOutputData = &GeneratedOutputData.Last();
			ShotOutputData->Shot = FutureData.Shot;
		}

		// Add the filepath to the renderpass data.
		ShotOutputData->RenderPassData.FindOrAdd(FutureData.DataIdentifier).FilePaths.Add(FutureData.FilePath);

		// Sometime futures can be completed, but will be set to an error state (such as we couldn't write to the specified disk path.)
		if (!OutputFuture.Get<0>().Get())
		{
			UE_LOG(LogMovieRenderPipeline, Error, TEXT("Error exporting frame, canceling movie export."));
			RequestShutdown(true);
			break;
		}
	}

	// Remove any output futures that have been completed now.
	for (int32 Index = CompletedOutputFutures.Num() - 1; Index >= 0; --Index)
	{
		OutstandingOutputFutures.RemoveAt(CompletedOutputFutures[Index]);
	}
}

UMovieGraphConfig* UMovieGraphPipeline::GetRootGraphForShot(UMoviePipelineExecutorShot* InShot) const
{
	if (GetCurrentJob())
	{
		for (UMoviePipelineExecutorShot* Shot : GetCurrentJob()->ShotInfo)
		{
			if (Shot == InShot)
			{
				if (Shot->GetGraphPreset())
				{
					return Shot->GetGraphPreset();
				}
				else if (Shot->GetGraphConfig())
				{
					return Shot->GetGraphConfig();
				}
			}

		}

		// If the shot hasn't overwritten the preset then we return the root one for the whole job.
		if (GetCurrentJob()->GetGraphPreset())
		{
			return GetCurrentJob()->GetGraphPreset();
		}
		else if (GetCurrentJob()->GetGraphConfig())
		{
			return GetCurrentJob()->GetGraphConfig();
		}
	}

	return nullptr;
}

FMovieGraphTraversalContext UMovieGraphPipeline::GetCurrentTraversalContext() const
{
	FMovieGraphTraversalContext CurrentContext;
	CurrentContext.ShotIndex = GetCurrentShotIndex();
	CurrentContext.ShotCount = GetActiveShotList().Num();
	CurrentContext.Job = GetCurrentJob();
	CurrentContext.RootGraph = GetRootGraphForShot(GetActiveShotList()[GetCurrentShotIndex()]);
	CurrentContext.Time = GetTimeStepInstance()->GetCalculatedTimeData();

	return CurrentContext;
}

const TSet<TObjectPtr<UMovieGraphFileOutputNode>> UMovieGraphPipeline::GetOutputNodesUsed() const
{
	return OutputNodesDataSentTo;
}
