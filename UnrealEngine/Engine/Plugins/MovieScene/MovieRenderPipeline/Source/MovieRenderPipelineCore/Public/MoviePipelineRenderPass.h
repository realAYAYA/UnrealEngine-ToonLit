// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UObject/Object.h"
#include "MovieRenderPipelineDataTypes.h"
#include "MoviePipelineSetting.h"
#include "ImageWriteStream.h"
#include "MoviePipelineRenderPass.generated.h"

UCLASS(Blueprintable, Abstract)
class MOVIERENDERPIPELINECORE_API UMoviePipelineRenderPass : public UMoviePipelineSetting
{
	GENERATED_BODY()
public:
	// UMoviePipelineSetting Interface
	virtual void ValidateStateImpl() override;
	// ~UMoviePipelineSetting Interface
	
	void Setup(const MoviePipeline::FMoviePipelineRenderPassInitSettings& InPassInitSettings)
	{
		SetupImpl(InPassInitSettings);
	}

	void Teardown()
	{
		WaitUntilTasksComplete();
		TeardownImpl();
	}

	/** An array of identifiers for the output buffers expected as a result of this render pass. */
	void GatherOutputPasses(TArray<FMoviePipelinePassIdentifier>& ExpectedRenderPasses)
	{
		GatherOutputPassesImpl(ExpectedRenderPasses);
	}

	/** This will called for each requested sample. */
	void RenderSample_GameThread(const FMoviePipelineRenderPassMetrics& InSampleState)
	{
		RenderSample_GameThreadImpl(InSampleState);
	}

	bool IsAlphaInTonemapperRequired() const
	{
		return IsAlphaInTonemapperRequiredImpl();
	}


protected:
	virtual bool IsValidOnShots() const override { return true; }
	virtual bool IsValidOnMaster() const override { return true; }
#if WITH_EDITOR
	virtual FText GetCategoryText() const override { return NSLOCTEXT("MovieRenderPipeline", "RenderingCategoryName_Text", "Rendering"); }
#endif
protected:
	virtual void SetupImpl(const MoviePipeline::FMoviePipelineRenderPassInitSettings& InPassInitSettings) {}
	virtual void WaitUntilTasksComplete() {}
	virtual void TeardownImpl() {}
	virtual void GatherOutputPassesImpl(TArray<FMoviePipelinePassIdentifier>& ExpectedRenderPasses) {}
	virtual void RenderSample_GameThreadImpl(const FMoviePipelineRenderPassMetrics& InSampleState) {}
	virtual bool IsAlphaInTonemapperRequiredImpl() const { return false; }
};