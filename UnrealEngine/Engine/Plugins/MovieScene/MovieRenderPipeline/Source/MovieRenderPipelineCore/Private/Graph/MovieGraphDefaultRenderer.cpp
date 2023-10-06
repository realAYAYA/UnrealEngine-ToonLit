// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/MovieGraphDefaultRenderer.h"
#include "Graph/MovieGraphPipeline.h"
#include "Graph/MovieGraphConfig.h"
#include "Graph/Nodes/MovieGraphRenderLayerNode.h"
#include "Graph/Nodes/MovieGraphRenderPassNode.h"
#include "MovieRenderPipelineCoreModule.h"
#include "RenderingThread.h"
#include "Engine/TextureRenderTarget2D.h"
#include "UObject/Package.h"
#include "MoviePipelineSurfaceReader.h"

void UMovieGraphDefaultRenderer::SetupRenderingPipelineForShot(UMoviePipelineExecutorShot* InShot)
{
	// Iterate through the graph config and look for Render Layers.
	UMovieGraphConfig* RootGraph = GetOwningGraph()->GetRootGraphForShot(InShot);

	struct FMovieGraphPass
	{
		TSubclassOf<UMovieGraphRenderPassNode> ClassType;

		/** Maps a named branch to the specific render pass node that is assigned to render it. */
		TMap<FName, TWeakObjectPtr<UMovieGraphRenderPassNode>> BranchRenderers;
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
		FString RenderLayerName = Branch.ToString();
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
			UE_LOG(LogMovieRenderPipeline, Warning, TEXT("Found RenderLayer: \"%s\" but no Renderers defined."), *RenderLayerName);
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
			
			ExistingPass->BranchRenderers.Add(Branch, RenderPassNode);
		}
	}

	UE_LOG(LogMovieRenderPipeline, Warning, TEXT("Found: %d Render Passes:"), OutputPasses.Num());
	int32 TotalLayerCount = 0;
	for (const FMovieGraphPass& Pass : OutputPasses)
	{
		// ToDo: This should probably come from the Renderers themselves, as they can internally produce multiple
		// renders (such as ObjectID passes).
		TotalLayerCount += Pass.BranchRenderers.Num();
		
		FMovieGraphRenderPassSetupData SetupData;
		SetupData.Renderer = this;
		UE_LOG(LogMovieRenderPipeline, Warning, TEXT("\tRenderer Class: %s"), *Pass.ClassType->GetName());
		for (const TTuple<FName, TWeakObjectPtr<UMovieGraphRenderPassNode>>& BranchRenderer : Pass.BranchRenderers)
		{
			FMovieGraphRenderPassLayerData& LayerData = SetupData.Layers.AddDefaulted_GetRef();
			LayerData.BranchName = BranchRenderer.Key;
			LayerData.RenderPassNode = BranchRenderer.Value;

			UE_LOG(LogMovieRenderPipeline, Warning, TEXT("\t\tBranch Name: %s"), *BranchRenderer.Key.ToString());
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
	for (TPair<UE::MovieGraph::DefaultRenderer::FRenderTargetInitParams, TObjectPtr<UTextureRenderTarget2D>>& KVP : This->PooledViewRenderTargets)
	{
		Collector.AddStableReference(&KVP.Value);
	}
}

void UMovieGraphDefaultRenderer::Render(const FMovieGraphTimeStepData& InTimeStepData)
{
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

	if (InTimeStepData.bIsFirstTemporalSampleForFrame)
	{
		// If this is the first sample for this output frame, then we need to 
		// talk to all of our render passes and ask them for what data they will
		// produce, and set the Output Merger up with that knowledge.
		UE::MovieGraph::FMovieGraphOutputMergerFrame& NewOutputFrame = GetOwningGraph()->GetOutputMerger()->AllocateNewOutputFrame_GameThread(InTimeStepData.OutputFrameNumber);

		// Get the Traversal Context (not specific to any render pass) at the first sample. This is so
		// we can easily fetch things that are shared between all render layers later.
		NewOutputFrame.TraversalContext = GetOwningGraph()->GetCurrentTraversalContext();
		NewOutputFrame.EvaluatedConfig = TStrongObjectPtr<UMovieGraphEvaluatedConfig>(InTimeStepData.EvaluatedConfig);

		for (const TObjectPtr<UMovieGraphRenderPassNode>& RenderPass : RenderPassesInUse)
		{
			RenderPass->GatherOutputPasses(NewOutputFrame.ExpectedRenderPasses);
		}

		// Register the frame with our render statistics as being worked on

		UE::MovieGraph::FRenderTimeStatistics* TimeStats = GetRenderTimeStatistics(NewOutputFrame.TraversalContext.Time.OutputFrameNumber);
		if (ensure(TimeStats))
		{
			TimeStats->StartTime = FDateTime::UtcNow();
		}
	}

	for (TObjectPtr<UMovieGraphRenderPassNode> RenderPass : RenderPassesInUse)
	{
		// Pass in a copy of the traversal context so the renderer can decide what to do with it.
		UE::MovieGraph::FMovieGraphOutputMergerFrame& OutputFrame = GetOwningGraph()->GetOutputMerger()->GetOutputFrame_GameThread(InTimeStepData.OutputFrameNumber);
		RenderPass->Render(OutputFrame.TraversalContext, InTimeStepData);
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
		CameraInfo.CameraName = TEXT("Unnamed_Camera"); // ToDo: This eventually needs to come from Level Sequences
	}
	else
	{
		UE_LOG(LogMovieRenderPipeline, Error, TEXT("Failed to find Local Player Controller/Camera Manager to get viewpoint!"));
	}
	
	return CameraInfo;
}

UTextureRenderTarget2D* UMovieGraphDefaultRenderer::GetOrCreateViewRenderTarget(const UE::MovieGraph::DefaultRenderer::FRenderTargetInitParams& InInitParams)
{
	if (const TObjectPtr<UTextureRenderTarget2D>* ExistingViewRenderTarget = PooledViewRenderTargets.Find(InInitParams))
	{
		return *ExistingViewRenderTarget;
	}

	const TObjectPtr<UTextureRenderTarget2D> NewViewRenderTarget = CreateViewRenderTarget(InInitParams);
	PooledViewRenderTargets.Emplace(InInitParams, NewViewRenderTarget);

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

namespace UE::MovieGraph::DefaultRenderer
{
FSurfaceAccumulatorPool::FInstancePtr FSurfaceAccumulatorPool::BlockAndGetAccumulator_GameThread(int32 InFrameNumber, const FMovieGraphRenderDataIdentifier& InPassIdentifier)
{
	FScopeLock ScopeLock(&CriticalSection);

	int32 AvailableIndex = INDEX_NONE;
	while (AvailableIndex == INDEX_NONE)
	{
		for (int32 Index = 0; Index < Accumulators.Num(); Index++)
		{
			if (InFrameNumber == Accumulators[Index]->ActiveFrameNumber && InPassIdentifier == Accumulators[Index]->ActivePassIdentifier)
			{
				AvailableIndex = Index;
				break;
			}
		}

		if (AvailableIndex == INDEX_NONE)
		{
			// If we don't have an accumulator already working on it let's look for a free one.
			for (int32 Index = 0; Index < Accumulators.Num(); Index++)
			{
				if (!Accumulators[Index]->IsActive())
				{
					// Found a free one, tie it to this output frame.
					Accumulators[Index]->ActiveFrameNumber = InFrameNumber;
					Accumulators[Index]->ActivePassIdentifier = InPassIdentifier;
					Accumulators[Index]->bIsActive = true;
					Accumulators[Index]->TaskPrereq = UE::Tasks::FTask();
					AvailableIndex = Index;
					break;
				}
			}
		}
	}

	return Accumulators[AvailableIndex];
}
}
