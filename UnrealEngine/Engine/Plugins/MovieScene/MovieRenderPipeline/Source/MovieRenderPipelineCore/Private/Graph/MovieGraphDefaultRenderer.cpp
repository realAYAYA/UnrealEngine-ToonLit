// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/MovieGraphDefaultRenderer.h"
#include "Graph/MovieGraphPipeline.h"
#include "Graph/MovieGraphConfig.h"
#include "Graph/Nodes/MovieGraphGlobalGameOverrides.h"
#include "Graph/Nodes/MovieGraphRenderLayerNode.h"
#include "Graph/Nodes/MovieGraphRenderPassNode.h"
#include "Graph/Nodes/MovieGraphGlobalGameOverrides.h"
#include "Graph/Nodes/MovieGraphDebugNode.h"
#include "MovieRenderPipelineCoreModule.h"
#include "RenderingThread.h"
#include "Engine/TextureRenderTarget2D.h"
#include "UObject/Package.h"
#include "MoviePipelineSurfaceReader.h"
#include "RenderCaptureInterface.h"

// For flushing async systems
#include "EngineModule.h"
#include "MeshCardRepresentation.h"
#include "ShaderCompiler.h"
#include "EngineUtils.h"
#include "AssetCompilingManager.h"
#include "ContentStreaming.h"
#include "EngineModule.h"
#include "LandscapeSubsystem.h"
#include "Materials/MaterialInterface.h"
#include "RendererInterface.h"

void UMovieGraphDefaultRenderer::SetupRenderingPipelineForShot(UMoviePipelineExecutorShot* InShot)
{
	// Iterate through the graph config and look for Render Layers.
	UMovieGraphConfig* RootGraph = GetOwningGraph()->GetRootGraphForShot(InShot);

	struct FMovieGraphPass
	{
		TSubclassOf<UMovieGraphRenderPassNode> ClassType;

		/** Maps a named branch to the specific render pass node that is assigned to render it. */
		TMap<FMovieGraphRenderDataIdentifier, TWeakObjectPtr<UMovieGraphRenderPassNode>> BranchRenderers;
	};

	TArray<FMovieGraphPass> OutputPasses;

	// Start by getting our root set of branches we should follow
	const FMovieGraphTimeStepData& TimeStepData = GetOwningGraph()->GetTimeStepInstance()->GetCalculatedTimeData();
	UMovieGraphEvaluatedConfig* EvaluatedConfig = TimeStepData.EvaluatedConfig;
	TArray<FName> GraphBranches = EvaluatedConfig->GetBranchNames();

	for (const FName& Branch : GraphBranches)
	{
		// We follow each branch looking for Render Layer nodes to figure out what render layer this should be. We assume a render layer is named
		// after the branch it is on, unless they specifically add a UMovieGraphRenderLayerNode to rename it.
		const bool bIncludeCDOs = false;
		UMovieGraphRenderLayerNode* RenderLayerNode = EvaluatedConfig->GetSettingForBranch<UMovieGraphRenderLayerNode>(Branch, bIncludeCDOs);
			
		// A RenderLayerNode is required for now to indicate you wish to actually render something.
		if (!RenderLayerNode)
		{
			continue;
		}

		// Now we need to figure out which renderers are on this branch.
		const bool bExactMatch = false;
		TArray<UMovieGraphRenderPassNode*> Renderers = EvaluatedConfig->GetSettingsForBranch<UMovieGraphRenderPassNode>(Branch, bIncludeCDOs, bExactMatch);
		if (RenderLayerNode && Renderers.Num() == 0)
		{
			UE_LOG(LogMovieRenderPipeline, Warning, TEXT("Found RenderLayer: \"%s\" but no Renderers defined."), *Branch.ToString());
		}

		for (UMovieGraphRenderPassNode* RenderPassNode : Renderers)
		{

			FMovieGraphPass* ExistingPass = OutputPasses.FindByPredicate([RenderPassNode](const FMovieGraphPass& Pass)
				{ return Pass.ClassType == RenderPassNode->GetClass(); });
			
			if (!ExistingPass)
			{
				ExistingPass = &OutputPasses.AddDefaulted_GetRef();
				ExistingPass->ClassType = RenderPassNode->GetClass();
			}

			FMovieGraphRenderDataIdentifier Identifier;
			Identifier.RootBranchName = Branch;
			Identifier.LayerName = RenderLayerNode->LayerName;
			
			ExistingPass->BranchRenderers.Add(Identifier, RenderPassNode);
		}
	}

	//UE_LOG(LogMovieRenderPipeline, Warning, TEXT("Found: %d Render Passes:"), OutputPasses.Num());
	int32 TotalLayerCount = 0;
	for (const FMovieGraphPass& Pass : OutputPasses)
	{
		// ToDo: This should probably come from the Renderers themselves, as they can internally produce multiple
		// renders (such as ObjectID passes).
		TotalLayerCount += Pass.BranchRenderers.Num();
		
		FMovieGraphRenderPassSetupData SetupData;
		SetupData.Renderer = this;
		//UE_LOG(LogMovieRenderPipeline, Warning, TEXT("\tRenderer Class: %s"), *Pass.ClassType->GetName());
		for (const TTuple<FMovieGraphRenderDataIdentifier, TWeakObjectPtr<UMovieGraphRenderPassNode>>& BranchRenderer : Pass.BranchRenderers)
		{
			FMovieGraphRenderPassLayerData& LayerData = SetupData.Layers.AddDefaulted_GetRef();
			LayerData.BranchName = BranchRenderer.Key.RootBranchName;
			LayerData.LayerName = BranchRenderer.Key.LayerName;
			LayerData.RenderPassNode = BranchRenderer.Value;
			// UE_LOG(LogMovieRenderPipeline, Warning, TEXT("\t\tBranch Name: %s"), *LayerBranchName.ToString());
		}

		UMovieGraphRenderPassNode* RenderPassCDO = Pass.ClassType->GetDefaultObject<UMovieGraphRenderPassNode>();
		RenderPassCDO->Setup(SetupData);

		RenderPassesInUse.Add(RenderPassCDO);
	}

	UE_LOG(LogMovieRenderPipeline, Log, TEXT("Finished initializing %d Render Passes (with %d total layers)."), OutputPasses.Num(), TotalLayerCount);
}

void UMovieGraphDefaultRenderer::TeardownRenderingPipelineForShot(UMoviePipelineExecutorShot* InShot)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(MRQ_TeardownRenderingPipeline);

	// Ensure the GPU has actually rendered all of the frames
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(MRQ_WaitForRenderWorkFinished);
		FlushRenderingCommands();
	}

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(MRQ_WaitForGPUReadbackFinished);
		// Make sure all of the data has actually been copied back from the GPU and accumulation tasks started.
		for (const TPair<UE::MovieGraph::DefaultRenderer::FRenderTargetInitParams, FMoviePipelineSurfaceQueuePtr>& SurfaceQueueIt : PooledSurfaceQueues)
		{
			if (SurfaceQueueIt.Value.IsValid())
			{
				SurfaceQueueIt.Value->Shutdown();
			}
		}

	}
	
	// Stall until the task graph has completed any pending accumulations.
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(MRQ_WaitForAccumulationTasksFinished);

		UE::Tasks::Wait(OutstandingTasks);
		OutstandingTasks.Empty();
	}
	

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(MRQ_TeardownRenderPasses);
		
		for (const TObjectPtr<UMovieGraphRenderPassNode>& RenderPass : RenderPassesInUse)
		{
			RenderPass->Teardown();
		}

		RenderPassesInUse.Reset();
	}

	// ToDo: This could probably be preserved across shots to avoid allocations
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(MRQ_ResetPooledData);
		PooledViewRenderTargets.Reset();
		PooledAccumulators.Reset();
		PooledSurfaceQueues.Reset();
	}
}


void UMovieGraphDefaultRenderer::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(InThis, Collector);
	UMovieGraphDefaultRenderer* This = CastChecked<UMovieGraphDefaultRenderer>(InThis);

	// Can't be a const& due to AddStableReference API
	for (TPair<UE::MovieGraph::DefaultRenderer::FMovieGraphImagePreviewDataPoolParams, TObjectPtr<UTextureRenderTarget2D>>& KVP : This->PooledViewRenderTargets)
	{
		Collector.AddStableReference(&KVP.Value);
	}
}

void UMovieGraphDefaultRenderer::Render(const FMovieGraphTimeStepData& InTimeStepData)
{
	// the render thread uses it.
	FlushAsyncEngineSystems(InTimeStepData.EvaluatedConfig);

	// Housekeeping: Clean up any tasks that were completed since the last frame. This lets us have a 
	// better idea of how much work we're concurrently doing. 
	{
		// Don't iterate through the array if someone is actively modifying it.
		TRACE_CPUPROFILER_EVENT_SCOPE(MRQ_RemoveCompletedOutstandingTasks);
		FScopeLock ScopeLock(&OutstandingTasksMutex);
		for (int32 Index = 0; Index < OutstandingTasks.Num(); Index++)
		{
			const UE::Tasks::FTask& Task = OutstandingTasks[Index];
			if (Task.IsCompleted())
			{
				OutstandingTasks.RemoveAtSwap(Index);
		
				// We swapped the end array element into the current spot,
				// so we need to check this element again, otherwise we skip
				// checking some things.
				Index--;
			}
		}
	}

	// Hide the progress widget before we render anything. This allows widget captures to not include the progress bar.
	GetOwningGraph()->SetPreviewWidgetVisible(false);

	if (InTimeStepData.bIsFirstTemporalSampleForFrame)
	{
		// If this is the first sample for this output frame, then we need to 
		// talk to all of our render passes and ask them for what data they will
		// produce, and set the Output Merger up with that knowledge.
		UE::MovieGraph::FMovieGraphOutputMergerFrame& NewOutputFrame = GetOwningGraph()->GetOutputMerger()->AllocateNewOutputFrame_GameThread(InTimeStepData.RenderedFrameNumber);

		// Get the Traversal Context (not specific to any render pass) at the first sample. This is so
		// we can easily fetch things that are shared between all render layers later.
		NewOutputFrame.TraversalContext = GetOwningGraph()->GetCurrentTraversalContext();
		NewOutputFrame.EvaluatedConfig = TStrongObjectPtr<UMovieGraphEvaluatedConfig>(InTimeStepData.EvaluatedConfig);

		for (const TObjectPtr<UMovieGraphRenderPassNode>& RenderPass : RenderPassesInUse)
		{
			RenderPass->GatherOutputPasses(InTimeStepData.EvaluatedConfig, NewOutputFrame.ExpectedRenderPasses);
		}

		// Register the frame with our render statistics as being worked on

		UE::MovieGraph::FRenderTimeStatistics* TimeStats = GetRenderTimeStatistics(NewOutputFrame.TraversalContext.Time.RenderedFrameNumber);
		if (ensure(TimeStats))
		{
			TimeStats->StartTime = FDateTime::UtcNow();
		}
	}

	// There is some work we need to signal to the renderer for only the first view of a frame,
	// so we have to track when any of our *FSceneView* render passes submit stuff (UI renderers don't count)
	bHasRenderedFirstViewThisFrame = false;

	// Support for RenderDoc captures of just the MRQ work
#if WITH_EDITOR && !UE_BUILD_SHIPPING
	TUniquePtr<RenderCaptureInterface::FScopedCapture> ScopedGPUCapture;
	{
		UMovieGraphDebugSettingNode* DebugSettings = InTimeStepData.EvaluatedConfig->GetSettingForBranch<UMovieGraphDebugSettingNode>(UMovieGraphNode::GlobalsPinName);
		if (DebugSettings && DebugSettings->bCaptureFramesWithRenderDoc)
		{
			ScopedGPUCapture = MakeUnique<RenderCaptureInterface::FScopedCapture>(true, *FString::Printf(TEXT("MRQ Frame: %d"), InTimeStepData.RootFrameNumber.Value));
		}
	}
#endif

	// Workaround for UE-202937, we need to detect when there will be multiple scene views rendered for a given frame
	// to have grooms handle motion blur correctly when there are multiple views being rendered.
	int32 NumSceneViewsRendered = 0;
	for (const TObjectPtr<UMovieGraphRenderPassNode>& RenderPass : RenderPassesInUse)
	{
		NumSceneViewsRendered += RenderPass->GetNumSceneViewsRendered();
	}

	if (NumSceneViewsRendered > 1)
	{
		IConsoleVariable* HairStrandsCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.HairStrands.Strands.MotionVectorCheckViewID"));
		if (HairStrandsCVar)
		{
			HairStrandsCVar->SetWithCurrentPriority(0);
		}
	}

	for (const TObjectPtr<UMovieGraphRenderPassNode>& RenderPass : RenderPassesInUse)
	{
		// Pass in a copy of the traversal context so the renderer can decide what to do with it.
		UE::MovieGraph::FMovieGraphOutputMergerFrame& OutputFrame = GetOwningGraph()->GetOutputMerger()->GetOutputFrame_GameThread(InTimeStepData.RenderedFrameNumber);
		RenderPass->Render(OutputFrame.TraversalContext, InTimeStepData);
	}

	if (NumSceneViewsRendered > 1)
	{
		// Restore the default value afterwards.
		IConsoleVariable* HairStrandsCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.HairStrands.Strands.MotionVectorCheckViewID"));
		if (HairStrandsCVar)
		{
			HairStrandsCVar->SetWithCurrentPriority(1);
		}
	}

	// Re-enable the progress widget so when the player viewport is drawn to the preview window, it shows.
	GetOwningGraph()->SetPreviewWidgetVisible(true);
}

void UMovieGraphDefaultRenderer::FlushAsyncEngineSystems(const TObjectPtr<UMovieGraphEvaluatedConfig>& InConfig) const
{
	const UMovieGraphGlobalGameOverridesNode* GameOverrides = InConfig->GetSettingForBranch<UMovieGraphGlobalGameOverridesNode>(UMovieGraphNode::GlobalsPinName);
	if (!GameOverrides)
	{
		return;
	}

	// Block until level streaming is completed, we do this at the end of the frame
	// so that level streaming requests made by Sequencer level visibility tracks are
	// accounted for.
	if (GameOverrides->bFlushLevelStreaming && GetOwningGraph()->GetWorld())
	{
		GetOwningGraph()->GetWorld()->BlockTillLevelStreamingCompleted();
	}

	// Flush all assets still being compiled asynchronously.
	// A progress bar is already in place so the user can get feedback while waiting for everything to settle.
	if (GameOverrides->bFlushAssetCompiler)
	{
		FAssetCompilingManager::Get().FinishAllCompilation();
	}

	// Ensure we have complete shader maps for all materials used by primitives in the world.
	// This way we will never render with the default material.
	if (GameOverrides->bFlushShaderCompiler)
	{
		UMaterialInterface::SubmitRemainingJobsForWorld(GetOwningGraph()->GetWorld());
	}

	// Flush virtual texture tile calculations.
	// In its own scope just to minimize the duration FSyncScope has a lock.
	{
		UE::RenderCommandPipe::FSyncScope SyncScope;
		
		ERHIFeatureLevel::Type FeatureLevel = GetWorld()->GetFeatureLevel();
		ENQUEUE_RENDER_COMMAND(VirtualTextureSystemFlushCommand)(
			[FeatureLevel](FRHICommandListImmediate& RHICmdList)
			{
				GetRendererModule().LoadPendingVirtualTextureTiles(RHICmdList, FeatureLevel);
			});
	}

	// Flush any outstanding work waiting in Streaming Manager implementations (texture streaming, nanite, etc.)
	// Note: This isn't a magic fix for gpu-based feedback systems, if the work hasn't made it to the streaming
	// manager, it can't flush it. This just ensures that work that has been requested is done before we render.
	if (GameOverrides->bFlushStreamingManagers)
	{
		FStreamingManagerCollection& StreamingManagers = IStreamingManager::Get();
		constexpr bool bProcessEverything = true;
		StreamingManagers.UpdateResourceStreaming(GetOwningGraph()->GetWorld()->GetDeltaSeconds(), bProcessEverything);
		StreamingManagers.BlockTillAllRequestsFinished();
	}

	// If there are async tasks to build more grass, wait for them to finish so there aren't missing patches
	// of grass. If you have way too dense grass this option can cause you to OOM.
	if (GameOverrides->bFlushGrassStreaming)
	{
		if (ULandscapeSubsystem* LandscapeSubsystem = GetWorld()->GetSubsystem<ULandscapeSubsystem>())
		{
			constexpr bool bFlushGrass = false; // Flush means a different thing to grass system
			constexpr bool bInForceSync = true;

			TArray<FVector> CameraLocations;
			GetCameraLocationsForFrame(CameraLocations);

			LandscapeSubsystem->RegenerateGrass(bFlushGrass, bInForceSync, MakeArrayView(CameraLocations));
		}
	}
}

void UMovieGraphDefaultRenderer::GetCameraLocationsForFrame(TArray<FVector>& OutLocations) const
{
	// ToDo: Multi-camera support
	if (const APlayerController* LocalPlayerController = GetOwningGraph()->GetWorld()->GetFirstPlayerController())
	{
		FVector PrimaryCameraLoc;
		FRotator PrimaryCameraRot;

		LocalPlayerController->GetPlayerViewPoint(PrimaryCameraLoc, PrimaryCameraRot);
		OutLocations.Add(PrimaryCameraLoc);
	}
}

void UMovieGraphDefaultRenderer::AddOutstandingRenderTask_AnyThread(UE::Tasks::FTask InTask)
{
	// We might be looping through the array to remove previously completed tasks,
	// so don't modify until that is completed.
	FScopeLock ScopeLock(&OutstandingTasksMutex);
	OutstandingTasks.Add(MoveTemp(InTask));
}

UE::MovieGraph::DefaultRenderer::FCameraInfo UMovieGraphDefaultRenderer::GetCameraInfo(const FGuid& InCameraIdentifier) const
{
	UE::MovieGraph::DefaultRenderer::FCameraInfo CameraInfo;

	// We only support the primary camera right now
	APlayerController* LocalPlayerController = GetWorld()->GetFirstPlayerController();
	if (LocalPlayerController && LocalPlayerController->PlayerCameraManager)
	{
		CameraInfo.ViewInfo = LocalPlayerController->PlayerCameraManager->GetCameraCacheView();
		CameraInfo.ViewActor = LocalPlayerController->GetViewTarget();
		CameraInfo.CameraName = TEXT("Unsupported"); // ToDo: This eventually needs to come from Level Sequences
	}
	else
	{
		UE_LOG(LogMovieRenderPipeline, Error, TEXT("Failed to find Local Player Controller/Camera Manager to get viewpoint!"));
	}
	
	return CameraInfo;
}

UTextureRenderTarget2D* UMovieGraphDefaultRenderer::GetOrCreateViewRenderTarget(const UE::MovieGraph::DefaultRenderer::FRenderTargetInitParams& InInitParams, const FMovieGraphRenderDataIdentifier& InIdentifier)
{
	UE::MovieGraph::DefaultRenderer::FMovieGraphImagePreviewDataPoolParams CombinedParams;
	CombinedParams.RenderInitParams = InInitParams;
	CombinedParams.Identifier = InIdentifier;

	if (const TObjectPtr<UTextureRenderTarget2D>* ExistingViewRenderTarget = PooledViewRenderTargets.Find(CombinedParams))
	{
		return *ExistingViewRenderTarget;
	}

	const TObjectPtr<UTextureRenderTarget2D> NewViewRenderTarget = CreateViewRenderTarget(InInitParams);
	PooledViewRenderTargets.Emplace(CombinedParams, NewViewRenderTarget);

	return NewViewRenderTarget.Get();
}

TObjectPtr<UTextureRenderTarget2D> UMovieGraphDefaultRenderer::CreateViewRenderTarget(const UE::MovieGraph::DefaultRenderer::FRenderTargetInitParams& InInitParams) const
{
	TObjectPtr<UTextureRenderTarget2D> NewTarget = NewObject<UTextureRenderTarget2D>(GetTransientPackage());
	NewTarget->ClearColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
	NewTarget->TargetGamma = InInitParams.TargetGamma;
	NewTarget->InitCustomFormat(InInitParams.Size.X, InInitParams.Size.Y, InInitParams.PixelFormat, false);
	int32 ResourceSizeBytes = NewTarget->GetResourceSizeBytes(EResourceSizeMode::Type::EstimatedTotal);
	UE_LOG(LogMovieRenderPipeline, Log, TEXT("Allocated a View Render Target sized: (%d, %d), Bytes: %d"), InInitParams.Size.X, InInitParams.Size.Y, ResourceSizeBytes);

	return NewTarget;
}

FMoviePipelineSurfaceQueuePtr UMovieGraphDefaultRenderer::GetOrCreateSurfaceQueue(const UE::MovieGraph::DefaultRenderer::FRenderTargetInitParams& InInitParams)
{
	if (const FMoviePipelineSurfaceQueuePtr* ExistingSurfaceQueue = PooledSurfaceQueues.Find(InInitParams))
	{
		return *ExistingSurfaceQueue;
	}

	const FMoviePipelineSurfaceQueuePtr NewSurfaceQueue = CreateSurfaceQueue(InInitParams);
	PooledSurfaceQueues.Emplace(InInitParams, NewSurfaceQueue);

	return NewSurfaceQueue;
}

FMoviePipelineSurfaceQueuePtr UMovieGraphDefaultRenderer::CreateSurfaceQueue(const UE::MovieGraph::DefaultRenderer::FRenderTargetInitParams& InInitParams) const
{
	// ToDo: Refactor these to be dynamically sized, but also allow putting a cap on them. We need at least enough to submit everything for one render
	FMoviePipelineSurfaceQueuePtr SurfaceQueue = MakeShared<FMoviePipelineSurfaceQueue, ESPMode::ThreadSafe>(InInitParams.Size, InInitParams.PixelFormat, 6, true);
	
	UE_LOG(LogMovieRenderPipeline, Log, TEXT("Allocated a Surface Queue sized: (%d, %d)"), InInitParams.Size.X, InInitParams.Size.Y);
	return SurfaceQueue;
}

UE::MovieGraph::FRenderTimeStatistics* UMovieGraphDefaultRenderer::GetRenderTimeStatistics(const int32 InFrameNumber)
{
	return &RenderTimeStatistics.FindOrAdd(InFrameNumber);
}

TArray<FMovieGraphImagePreviewData> UMovieGraphDefaultRenderer::GetPreviewData() const
{ 
	TArray<FMovieGraphImagePreviewData> Results;

	for (const TPair<UE::MovieGraph::DefaultRenderer::FMovieGraphImagePreviewDataPoolParams, TObjectPtr<UTextureRenderTarget2D>>& RenderTarget : PooledViewRenderTargets)
	{
		FMovieGraphImagePreviewData& Data = Results.AddDefaulted_GetRef();
		Data.Identifier = RenderTarget.Key.Identifier;
		Data.Texture = RenderTarget.Value.Get();
	}

	return Results;
}
