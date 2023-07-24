// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MoviePipelineOutputBase.h"
#include "Async/Future.h"
#include "MoviePipelineImageSequenceOutput.generated.h"

// Forward Declare
class IImageWriteQueue;

UCLASS(Blueprintable, Abstract)
class MOVIERENDERPIPELINERENDERPASSES_API UMoviePipelineImageSequenceOutputBase : public UMoviePipelineOutputBase
{
	GENERATED_BODY()
public:
	UMoviePipelineImageSequenceOutputBase();

	virtual void OnReceiveImageDataImpl(FMoviePipelineMergerOutputFrame* InMergedOutputFrame) override;

protected:
	// UMovieRenderPipelineOutputContainer interface
	virtual void BeginFinalizeImpl() override;
	virtual bool HasFinishedProcessingImpl() override;
	virtual void OnShotFinishedImpl(const UMoviePipelineExecutorShot* InShot, const bool bFlushToDisk) override;
	// ~UMovieRenderPipelineOutputContainer interface

	virtual void GetFormatArguments(FMoviePipelineFormatArgs& InOutFormatArgs) const override;
	virtual bool IsAlphaAllowed() const { return false; }
protected:
	/** The format of the image to write out */
	EImageFormat OutputFormat;

	/** A pointer to the image write queue used for asynchronously writing images */
	IImageWriteQueue* ImageWriteQueue;
private:

	/** A fence to keep track of when the Image Write queue has fully flushed. */
	TFuture<void> FinalizeFence;
};

UCLASS()
class MOVIERENDERPIPELINERENDERPASSES_API UMoviePipelineImageSequenceOutput_BMP : public UMoviePipelineImageSequenceOutputBase
{
	GENERATED_BODY()
public:
#if WITH_EDITOR
	virtual FText GetDisplayText() const override { return NSLOCTEXT("MovieRenderPipeline", "ImgSequenceBMPSettingDisplayName", ".bmp Sequence [8bit]"); }
#endif
public:
	UMoviePipelineImageSequenceOutput_BMP()
	{
		OutputFormat = EImageFormat::BMP;
	}
};

UCLASS()
class MOVIERENDERPIPELINERENDERPASSES_API UMoviePipelineImageSequenceOutput_PNG : public UMoviePipelineImageSequenceOutputBase
{
	GENERATED_BODY()
public:
#if WITH_EDITOR
	virtual FText GetDisplayText() const override { return NSLOCTEXT("MovieRenderPipeline", "ImgSequencePNGSettingDisplayName", ".png Sequence [8bit]"); }
#endif
	virtual bool IsAlphaAllowed() const override { return bWriteAlpha; }

public:
	UMoviePipelineImageSequenceOutput_PNG()
	{
		OutputFormat = EImageFormat::PNG;
		bWriteAlpha = true;
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PNG")
	bool bWriteAlpha;
};

UCLASS()
class MOVIERENDERPIPELINERENDERPASSES_API UMoviePipelineImageSequenceOutput_JPG : public UMoviePipelineImageSequenceOutputBase
{
	GENERATED_BODY()
public:
#if WITH_EDITOR
	virtual FText GetDisplayText() const override { return NSLOCTEXT("MovieRenderPipeline", "ImgSequenceJPGSettingDisplayName", ".jpg Sequence [8bit]"); }
#endif
public:
	UMoviePipelineImageSequenceOutput_JPG()
	{
		OutputFormat = EImageFormat::JPEG;
	}
};
