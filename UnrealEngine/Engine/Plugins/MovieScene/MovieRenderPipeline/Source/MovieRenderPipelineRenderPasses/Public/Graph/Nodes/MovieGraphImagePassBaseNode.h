// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Graph/MovieGraphNode.h"
#include "Graph/MovieGraphDataTypes.h"
#include "Graph/MovieGraphConfig.h"
#include "Graph/Renderers/MovieGraphImagePassBase.h"
#include "Graph/Renderers/MovieGraphShowFlags.h"
#include "Graph/Nodes/MovieGraphRenderPassNode.h"
#include "MoviePipelineDeferredPasses.h"
#include "MovieGraphImagePassBaseNode.generated.h"

// Forward Declare
struct FMovieGraphRenderPassSetupData;
struct FMovieGraphTimeStepData;

/**
* The UMovieGraphImagePassBaseNode is an abstract base-class for render nodes that wish to create
* renders of the 3d scene. You are not required to inherit from this node (can inherit from
* UMovieGraphRenderPassNode), but this node provides a helpful set of functions and default values
* for constructing the required matrices and settings for viewport-like renders.
*/
UCLASS(Abstract)
class MOVIERENDERPIPELINERENDERPASSES_API UMovieGraphImagePassBaseNode : public UMovieGraphRenderPassNode
{
	GENERATED_BODY()
public:
	UMovieGraphImagePassBaseNode();

	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
		
	/** Gets the view mode index that should be active for this renderer. */
	virtual EViewModeIndex GetViewModeIndex() const;

	/** Gets the show flags that should be active for this renderer. */
	virtual FEngineShowFlags GetShowFlags() const;

	/** Gets any show flags that should be applied as defaults, before user changes are applied. */
	virtual TArray<uint32> GetDefaultShowFlags() const;

	/** Should each individual sample rendered be written out for debugging? */
	virtual bool GetWriteAllSamples() const { return false; }

	/** Get an array of user-added post-process materials for the render */
	virtual TArray<FMoviePipelinePostProcessPass> GetAdditionalPostProcessMaterials() const { return {}; }

	/** How many spatial samples should be rendered each frame? */
	virtual int32 GetNumSpatialSamples() const { return 1; }

	/** How many spatial samples should be used during warm-up frames? */
	virtual int32 GetNumSpatialSamplesDuringWarmUp() const { return GetNumSpatialSamples(); }

	/** Should the tone curve be disabled while rendering? Allows for linear values in exrs but changes the look of the final image. */
	virtual bool GetDisableToneCurve() const { return false; }

	/** Should the output file be allowed to apply an OCIO transform on this render? */
	virtual bool GetAllowOCIO() const { return true; }

	/** Should the denoiser be run on the resulting image (only has any effect with the Path Tracer) */
	virtual bool GetAllowDenoiser() const { return true; }

	/** Which AA Method should be used? */
	virtual EAntiAliasingMethod GetAntiAliasingMethod() const { return EAntiAliasingMethod::AAM_None; }

protected:
	// Note: Since *individual* show flags are overridden instead of the entire ShowFlags property, manually set to
	// overridden so the traversal picks the changes up (otherwise they will be ignored).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_ShowFlags : 1 = 1; //-V570

	/** The show flags that should be active during a render for this node. */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Show Flags")
	TObjectPtr<UMovieGraphShowFlags> ShowFlags;

	TArray<TUniquePtr<UE::MovieGraph::Rendering::FMovieGraphImagePassBase>> CurrentInstances;

	static FString DefaultDepthAsset;
	static FString DefaultMotionVectorsAsset;

protected:
	// UMovieGraphRenderPassNode Interface
	virtual FString GetRendererNameImpl() const { return TEXT("UnnamedImageRenderPass"); }
	virtual void SetupImpl(const FMovieGraphRenderPassSetupData& InSetupData) override;
	virtual void TeardownImpl() override;
	virtual void RenderImpl(const FMovieGraphTraversalContext& InFrameTraversalContext, const FMovieGraphTimeStepData& InTimeData) override;
	virtual void GatherOutputPassesImpl(UMovieGraphEvaluatedConfig* InConfig, TArray<FMovieGraphRenderDataIdentifier>& OutExpectedPasses) const override;
	virtual int32 GetNumSceneViewsRenderedImpl() const { return CurrentInstances.Num(); }
    // ~UMovieGraphRenderPassNode Interface

	virtual TUniquePtr<UE::MovieGraph::Rendering::FMovieGraphImagePassBase> CreateInstance() const { return nullptr; }
};