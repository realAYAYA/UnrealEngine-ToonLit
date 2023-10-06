// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Graph/Nodes/MovieGraphRenderPassNode.h"
#include "Graph/MovieGraphDefaultRenderer.h" // For CameraInfo
#include "Graph/MovieGraphDataTypes.h"
#include "SceneTypes.h"
#include "Camera/CameraTypes.h"
#include "MovieRenderPipelineDataTypes.h"
#include "Engine/EngineTypes.h"
#include "Async/TaskGraphFwd.h"
#include "Styling/AppStyle.h"
#include "Tasks/Task.h"
#include "MovieGraphDeferredRenderPassNode.generated.h"

// Forward Declares
class UMovieGraphDefaultRenderer;
namespace UE::MovieGraph::DefaultRenderer
{
	struct FRenderTargetInitParams;
	struct IMovieGraphOutputMerger;
}
struct FMovieGraphRenderPassSetupData;
struct FMovieGraphRenderPassLayerData;
struct FImageOverlappedAccumulator;

// For FViewFamilyContextInitData
class FRenderTarget;
class UWorld;
class FSceneView;
class AActor;
class FSceneViewFamilyContext;
class FCanvas;

namespace UE::MovieGraph
{
	struct FMovieGraphRenderDataAccumulationArgs
	{
	public:
		TWeakPtr<FImageOverlappedAccumulator, ESPMode::ThreadSafe> ImageAccumulator;
		TWeakPtr<IMovieGraphOutputMerger, ESPMode::ThreadSafe> OutputMerger;

		// If it's the first sample then we will reset the accumulator to a clean slate before accumulating into it.
		bool bIsFirstSample;
		// If it's the last sample, then we will trigger moving the data to the output merger. It can be both the first and last sample at the same time.
		bool bIsLastSample;
	};

	void AccumulateSample_TaskThread(TUniquePtr<FImagePixelData>&& InPixelData, const UE::MovieGraph::FMovieGraphSampleState InSampleState, const UE::MovieGraph::FMovieGraphRenderDataAccumulationArgs& InAccumulationParams);
}

UCLASS()
class MOVIERENDERPIPELINECORE_API UMovieGraphDeferredRenderPassNode : public UMovieGraphRenderPassNode
{
	GENERATED_BODY()
public:
#if WITH_EDITOR
	virtual FText GetNodeTitle(const bool bGetDescriptive = false) const override
	{
		return NSLOCTEXT("MovieGraphNodes", "DeferredRenderPassGraphNode_Description", "Deferred Renderer");
	}
	
	FLinearColor GetNodeTitleColor() const
	{
		return FLinearColor(0.572f, 0.274f, 1.f);
	}

	FSlateIcon GetIconAndTint(FLinearColor& OutColor) const
	{
		static const FSlateIcon DeferredRendererIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "SequenceRecorder.TabIcon");

		OutColor = FLinearColor::White;
		return DeferredRendererIcon;
	}
#endif
protected:
	// UMovieGraphRenderPassNode Interface
	virtual FString GetRendererNameImpl() const override { return TEXT("Deferred"); }
	virtual void SetupImpl(const FMovieGraphRenderPassSetupData& InSetupData) override;
	virtual void TeardownImpl() override;
	virtual void RenderImpl(const FMovieGraphTraversalContext& InFrameTraversalContext, const FMovieGraphTimeStepData& InTimeData) override;
	virtual void GatherOutputPassesImpl(TArray<FMovieGraphRenderDataIdentifier>& OutExpectedPasses) const override;
	// ~UMovieGraphRenderPassNode Interface

	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

	struct FViewFamilyContextInitData
	{
		FViewFamilyContextInitData()
			: RenderTarget(nullptr)
			, World(nullptr)
			, SceneCaptureSource(ESceneCaptureSource::SCS_MAX)
			, bWorldIsPaused(false)
			, GlobalScreenPercentageFraction(1.0f)
			, OverscanFraction(0.f)
			, FrameIndex(-1)
			, bCameraCut(false)
			, AntiAliasingMethod(EAntiAliasingMethod::AAM_None)
			, View(nullptr)
			, SceneViewStateReference(nullptr)
		{
		}

		class FRenderTarget* RenderTarget;
		class UWorld* World;
		FMovieGraphTimeStepData TimeData;
		ESceneCaptureSource SceneCaptureSource;
		bool bWorldIsPaused;
		float GlobalScreenPercentageFraction;
		float OverscanFraction;
		int32 FrameIndex;
		bool bCameraCut;
		EAntiAliasingMethod AntiAliasingMethod;

		// Ownership of this will be passed to the FSceneViewFamilyContext
		FSceneView* View;

		FSceneViewStateInterface* SceneViewStateReference;

		// Camera Setup
		UE::MovieGraph::DefaultRenderer::FCameraInfo CameraInfo;

	};

	struct FMovieGraphDeferredRenderPass
	{
	public:
		void Setup(TWeakObjectPtr<UMovieGraphDefaultRenderer> InRenderer, TWeakObjectPtr<UMovieGraphDeferredRenderPassNode> InRenderPassNode, const FMovieGraphRenderPassLayerData& InLayer);
		void Teardown();
		void Render(const FMovieGraphTraversalContext& InFrameTraversalContext, const FMovieGraphTimeStepData& InTimeData);
		void GatherOutputPassesImpl(TArray<FMovieGraphRenderDataIdentifier>& OutExpectedPasses) const;
		void AddReferencedObjects(FReferenceCollector& Collector);

	protected:
		TSharedRef<FSceneViewFamilyContext> AllocateSceneViewFamilyContext(const FViewFamilyContextInitData& InInitData);
		FSceneView* AllocateSceneView(TSharedPtr<FSceneViewFamilyContext> InViewFamilyContext, FViewFamilyContextInitData& InInitData) const;
		void ApplyMoviePipelineOverridesToViewFamily(TSharedRef<FSceneViewFamilyContext> InOutFamily, const FViewFamilyContextInitData& InInitData);
		void PostRendererSubmission(const UE::MovieGraph::FMovieGraphSampleState& InSampleState, const UE::MovieGraph::DefaultRenderer::FRenderTargetInitParams& InRenderTargetInitParams, FCanvas& InCanvas);


	protected:
		FMovieGraphRenderPassLayerData LayerData;

		/** Unique identifier passed in GatherOutputPasses and with each render that identifies the data produced by this renderer. */
		FMovieGraphRenderDataIdentifier RenderDataIdentifier;

		// Scene View history used by the renderer 
		FSceneViewStateReference SceneViewState;

		TWeakObjectPtr<class UMovieGraphDefaultRenderer> Renderer;
		TWeakObjectPtr<class UMovieGraphDeferredRenderPassNode> RenderPassNode;
	};

	TArray<TUniquePtr<FMovieGraphDeferredRenderPass>> CurrentInstances;
};