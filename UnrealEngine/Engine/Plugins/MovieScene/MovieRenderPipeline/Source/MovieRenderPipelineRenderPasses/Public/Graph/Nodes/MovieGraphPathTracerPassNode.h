// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Graph/Nodes/MovieGraphImagePassBaseNode.h"

#include "MovieGraphPathTracerPassNode.generated.h"

/** A render node which uses the path tracer. */
UCLASS()
class MOVIERENDERPIPELINERENDERPASSES_API UMovieGraphPathTracerRenderPassNode : public UMovieGraphImagePassBaseNode
{
	GENERATED_BODY()

public:
	UMovieGraphPathTracerRenderPassNode();

#if WITH_EDITOR
	virtual FText GetNodeTitle(const bool bGetDescriptive = false) const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
#endif

	// UMovieGraphRenderPassNode Interface
	virtual void SetupImpl(const FMovieGraphRenderPassSetupData& InSetupData) override;
	virtual void TeardownImpl() override;
	// ~UMovieGraphRenderPassNode Interface

	// UMovieGraphImagePassBaseNode Interface
	virtual bool GetWriteAllSamples() const override;
	virtual TArray<FMoviePipelinePostProcessPass> GetAdditionalPostProcessMaterials() const override;
	virtual int32 GetNumSpatialSamples() const override;
	virtual int32 GetNumSpatialSamplesDuringWarmUp() const override;
	virtual bool GetDisableToneCurve() const override;
	virtual bool GetAllowOCIO() const override;
	virtual bool GetAllowDenoiser() const override;
	virtual TUniquePtr<UE::MovieGraph::Rendering::FMovieGraphImagePassBase> CreateInstance() const;
	// ~UMovieGraphImagePassBaseNode Interface

protected:
	// UMovieGraphRenderPassNode Interface
	virtual FString GetRendererNameImpl() const override;

	// ~UMovieGraphRenderPassNode Interface

	// UMovieGraphCoreRenderPassNode Interface
	virtual EViewModeIndex GetViewModeIndex() const override;
	// ~UMovieGraphCoreRenderPassNode Interface

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_SpatialSampleCount : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_bDenoiser : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_bDisableToneCurve : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_bAllowOCIO : 1;

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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (UIMin = 1, ClampMin = 1), Category = "Sampling", meta = (EditCondition = "bOverride_SpatialSampleCount"))
	int32 SpatialSampleCount;

	/** If true the resulting image will be denoised at the end of each set of Spatial Samples. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (UIMin = 1, ClampMin = 1), Category = "Sampling", meta = (EditCondition = "bOverride_bDenoiser"))
	bool bDenoiser;

	/**
	* Debug Feature. Not currently marked BlueprintReadWrite/EditAnywhere as it's not totally implemented on the Path Tracer right now.
	*/
	UPROPERTY(DisplayName = "Write All Samples (Debug)", meta = (EditCondition = "bOverride_bWriteAllsamples"))
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
	* An array of additional post-processing materials to run after the frame is rendered. Using this feature may add a notable amount of render time.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Post Process Materials", meta=(EditCondition="bOverride_AdditionalPostProcessMaterials"))
	TArray<FMoviePipelinePostProcessPass> AdditionalPostProcessMaterials;

private:
	/**
	 * The original value of the "r.PathTracing.ProgressDisplay" cvar before the render starts. The progress display
	 * will be hidden during the render.
	 */
	bool bOriginalProgressDisplayCvarValue = false;
};