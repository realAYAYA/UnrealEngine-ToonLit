// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MoviePipelineRenderPass.h"
#include "MovieRenderPipelineDataTypes.h"
#include "MoviePipelineWidgetRenderSetting.generated.h"

class FWidgetRenderer;
class UTextureRenderTarget2D;
class UMoviePipelineBurnInWidget;

namespace MoviePipeline { struct FMoviePipelineRenderPassInitSettings; }

UCLASS(Blueprintable)
class MOVIERENDERPIPELINESETTINGS_API UMoviePipelineWidgetRenderer : public UMoviePipelineRenderPass
{
	GENERATED_BODY()

protected:
	UMoviePipelineWidgetRenderer()
		: UMoviePipelineRenderPass()
		, bCompositeOntoFinalImage(true)
	{
	}

	// UMoviePipelineRenderPass Interface
	virtual void SetupImpl(const MoviePipeline::FMoviePipelineRenderPassInitSettings& InPassInitSettings) override;
	virtual void TeardownImpl() override;
	virtual void GatherOutputPassesImpl(TArray<FMoviePipelinePassIdentifier>& ExpectedRenderPasses) override;
	virtual void RenderSample_GameThreadImpl(const FMoviePipelineRenderPassMetrics& InSampleState) override;
	// ~UMoviePipelineRenderPass Interface

public:
	/** If true, the widget renderer image will be composited into the Final Image pass. Doesn't apply to multi-layer EXR files. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (MetaClass = "/Script/MovieRenderPipelineSettings.MoviePipelineBurnInWidget"), Category = "Widget Settings")
	bool bCompositeOntoFinalImage;

public:
#if WITH_EDITOR
	virtual FText GetDisplayText() const override { return NSLOCTEXT("MovieRenderPipeline", "WidgetRendererSettingDisplayName", "UI Renderer"); }
	virtual FText GetCategoryText() const { return NSLOCTEXT("MovieRenderPipeline", "WidgetRendererSettingCategoryName", "Rendering"); }
#endif
	virtual bool IsValidOnShots() const override { return true; }
	virtual bool IsValidOnMaster() const override { return true; }
private:
	TSharedPtr<FWidgetRenderer> WidgetRenderer;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;
};