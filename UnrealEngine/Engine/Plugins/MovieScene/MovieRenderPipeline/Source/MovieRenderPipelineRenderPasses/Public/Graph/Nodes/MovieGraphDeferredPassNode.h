// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Graph/Nodes/MovieGraphImagePassBaseNode.h"
#include "MoviePipelineDeferredPasses.h"
#include "MovieGraphDeferredPassNode.generated.h"

/**
* A render node which uses the Deferred Renderer.
*/
UCLASS()
class MOVIERENDERPIPELINERENDERPASSES_API UMovieGraphDeferredRenderPassNode : public UMovieGraphImagePassBaseNode 
{
	GENERATED_BODY()

public:
	UMovieGraphDeferredRenderPassNode();

	virtual void GetFormatResolveArgs(FMovieGraphResolveArgs& OutMergedFormatArgs, const FMovieGraphRenderDataIdentifier& InRenderDataIdentifier) const override;

#if WITH_EDITOR
	virtual FText GetNodeTitle(const bool bGetDescriptive = false) const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
#endif

protected:
	// UMovieGraphRenderPassNode Interface
	virtual FString GetRendererNameImpl() const override;
	virtual TUniquePtr<UE::MovieGraph::Rendering::FMovieGraphImagePassBase> CreateInstance() const override;
	virtual bool GetWriteAllSamples() const override;
	virtual TArray<FMoviePipelinePostProcessPass> GetAdditionalPostProcessMaterials() const override;
	virtual int32 GetNumSpatialSamples() const override;
	virtual bool GetDisableToneCurve() const override;
	virtual bool GetAllowOCIO() const override;
	virtual EAntiAliasingMethod GetAntiAliasingMethod() const override;
	// ~UMovieGraphRenderPassNode Interface

	// UMovieGraphImagePassBaseNode Interface
	virtual EViewModeIndex GetViewModeIndex() const override;
	// ~UMovieGraphImagePassBaseNode Interface

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_SpatialSampleCount: 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_AntiAliasingMethod : 1;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_bDisableToneCurve : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_bAllowOCIO : 1;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_ViewModeIndex : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_bWriteAllSamples : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_AdditionalPostProcessMaterials : 1;

	/**
	* How many sub-pixel jitter renders should we do per temporal sample? This can be used to achieve high
	* sample counts without Temporal Sub-Sampling (allowing high sample counts without motion blur being enabled),
	* but we generally recommend using Temporal Sub-Samples when possible. It can also be combined with
	* temporal samples and you will get SpatialSampleCount many renders per temporal sample.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (UIMin = 1, ClampMin = 1), Category = "Sampling", meta=(EditCondition="bOverride_SpatialSampleCount"))
	int32 SpatialSampleCount;

	/**
	* Which anti-aliasing method should this render use. If this is set to None, then Movie Render Graph
	* will handle anti-aliasing by doing a sub-pixel jitter (one for each temporal/spatial sample). Some
	* rendering effects rely on TSR or TAA to reduce noise so we recommend leaving them enabled
	* where possible. All options work with Spatial and Temporal samples, but TSR/TAA may introduce minor
	* visual artifacts (such as ghosting). MSAA is not supported in the deferred renderer.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sampling", meta = (EditCondition = "bOverride_AntiAliasingMethod"))
	TEnumAsByte<EAntiAliasingMethod> AntiAliasingMethod;
	
	/**
	* Debug Feature. Can use this to write out each individual Temporal and Spatial sample rendered by this render pass,
	* which allows you to see which images are being accumulated together. Can be useful for debugging incorrect looking
	* frames to see which sub-frame evaluations were incorrect.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sampling", AdvancedDisplay, DisplayName="Write All Samples (Debug)", meta = (EditCondition = "bOverride_bWriteAllsamples"))
	bool bWriteAllSamples;

	/**
	* If true, the tone curve will be disabled for this render pass. This will result in values greater than 1.0 in final renders
	* and can optionally be combined with OCIO profiles on the file output nodes to convert from Linear Values in Working Color Space
	* (which is sRGB  (Rec. 709) by default, unless changed in the project settings).
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (EditCondition = "bOverride_bDisableToneCurve"))
	bool bDisableToneCurve;

	/**
	* Allow the output file OpenColorIO transform to be used on this render.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (EditCondition = "bOverride_bAllowOCIO"))
	bool bAllowOCIO;
	
	/** 
	* The view mode index that will be applied to renders. These mirror the View Modes you find in the Viewport,
	* but most view modes other than Lit are used for debugging so they may not do what you expect, or may
	* have to be used in combination with certain Show Flags to produce a result similar to what you see in
	* the viewport.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta=(EditCondition="bOverride_ViewModeIndex", InvalidEnumValues = "VMI_PathTracing"))
	TEnumAsByte<EViewModeIndex> ViewModeIndex;

	/**
	* An array of additional post-processing materials to run after the frame is rendered. Using this feature may add a notable amount of render time.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Post Process Materials", meta=(EditCondition="bOverride_AdditionalPostProcessMaterials"))
	TArray<FMoviePipelinePostProcessPass> AdditionalPostProcessMaterials;
};