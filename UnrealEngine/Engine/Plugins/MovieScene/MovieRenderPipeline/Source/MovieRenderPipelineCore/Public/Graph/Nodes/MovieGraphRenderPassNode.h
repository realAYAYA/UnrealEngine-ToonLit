// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Graph/MovieGraphNode.h"
#include "Graph/MovieGraphDataTypes.h"
#include "Styling/AppStyle.h"

#include "MovieGraphRenderPassNode.generated.h"

// Forward Declare
struct FMovieGraphRenderPassSetupData;
struct FMovieGraphTimeStepData;

/**
* The UMovieGraphRenderPassNode node defines a render pass that MRQ may produce. This node can be implemented
* in the graph multiple times, and the exact settings it should use can be created out of a mixture of nodes. Because
* of this, when rendering, MRQ will figure out how many layers there are that actually use this CDO and will call
* the function on the CDO once, providing the information about all instances. This will allow the node to create any
* number of instances (decoupled from the number of times the node is used in the graph).
*/
UCLASS(Abstract)
class MOVIERENDERPIPELINECORE_API UMovieGraphRenderPassNode : public UMovieGraphSettingNode
{
	GENERATED_BODY()
public:
	UMovieGraphRenderPassNode()
	{
	}

	/** Get the name of this renderer. Deferred, Path Tracer, Panoramic, etc. Called on the CDO. */
	FString GetRendererName() const { return GetRendererNameImpl(); }
	
	/** Called when this should set up for rendering a new shot. Called on the CDO. */
	void Setup(const FMovieGraphRenderPassSetupData& InSetupData) { SetupImpl(InSetupData); }
	
	/** Called when this should do teardown of resources. FlushRenderingCommands() will have already been called by this point. Called on the CDO. */
	void Teardown() { TeardownImpl(); }
	
	/** Called each tick (once per temporal sample). Called on the CDO. */
	void Render(const FMovieGraphTraversalContext& InFrameTraversalContext, const FMovieGraphTimeStepData& InTimeData) { RenderImpl(InFrameTraversalContext, InTimeData); }

	/** 
	* Called each output frame. Should add a series of FMovieGraphRenderDataIdentifiers to the array, and then when producing frames
	* in Render, the resulting image data should have the matching FMovieGraphRenderDataIdentifiers associated with it. Used by the
	* Output Merger to ensure all of the render data for a given frame has been generated before passing it on to write to disk.
	* Called on the CDO.
	*/
	void GatherOutputPasses(UMovieGraphEvaluatedConfig* InConfig, TArray<FMovieGraphRenderDataIdentifier>& OutExpectedPasses) const { GatherOutputPassesImpl(InConfig, OutExpectedPasses); }

	/** Gets the number of Scene Views (that is, renders of the 3d scene) that this pass will produce. Can be zero for things like UI Renderers,
	* or more than one for things like panoramic or tiling. */
	int32 GetNumSceneViewsRendered() const { return GetNumSceneViewsRenderedImpl(); }

#if WITH_EDITOR
	virtual FText GetMenuCategory() const override
	{
		return NSLOCTEXT("MovieGraphNodes", "RenderPassGraphNode_Category", "Rendering");
	}

	virtual FLinearColor GetNodeTitleColor() const override
	{
		return FLinearColor(0.572f, 0.274f, 1.f);
	}
	
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override
	{
		static const FSlateIcon DeferredRendererIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "SequenceRecorder.TabIcon");
		OutColor = FLinearColor::White;
		return DeferredRendererIcon;
	}
#endif

protected:
	virtual FString GetRendererNameImpl() const { return TEXT("UnnamedRenderPass"); }
	virtual void SetupImpl(const FMovieGraphRenderPassSetupData& InSetupData) {}
	virtual void TeardownImpl() {}
	virtual void RenderImpl(const FMovieGraphTraversalContext& InFrameTraversalContext, const FMovieGraphTimeStepData& InTimeData) {}
	virtual void GatherOutputPassesImpl(UMovieGraphEvaluatedConfig* InConfig, TArray<FMovieGraphRenderDataIdentifier>& OutExpectedPasses) const {}
	virtual int32 GetNumSceneViewsRenderedImpl() const { return 0; }
};