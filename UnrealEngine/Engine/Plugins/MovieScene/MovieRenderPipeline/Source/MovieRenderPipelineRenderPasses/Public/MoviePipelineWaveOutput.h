// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MoviePipelineOutputBase.h"
#include "Sound/SampleBufferIO.h"
#include "MoviePipelineWaveOutput.generated.h"

UCLASS(BlueprintType)
class MOVIERENDERPIPELINERENDERPASSES_API UMoviePipelineWaveOutput : public UMoviePipelineOutputBase
{
	GENERATED_BODY()

	UMoviePipelineWaveOutput()
	{
	}

public:
#if WITH_EDITOR
	virtual FText GetDisplayText() const override { return NSLOCTEXT("MovieRenderPipeline", "AudioSettingDisplayName", ".wav Audio"); }
#endif
	virtual void OnShotFinishedImpl(const UMoviePipelineExecutorShot* InShot, const bool bFlushToDisk) override;

protected:
	virtual void BeginFinalizeImpl() override;
	virtual bool HasFinishedProcessingImpl() override;
	virtual void ValidateStateImpl() override;
	virtual void BuildNewProcessCommandLineArgsImpl(TArray<FString>& InOutUnrealURLParams, TArray<FString>& InOutCommandLineArgs, TArray<FString>& InOutDeviceProfileCvars, TArray<FString>& InOutExecCmds) const override;

public:
	/* File name format string override. If specified it will override the FileNameFormat from the Output setting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "File Output")
	FString FileNameFormatOverride;

private:
	/** Kept alive during finalization because the writer writes async to disk but doesn't expect to fall out of scope */
	TArray<TUniquePtr<Audio::FSoundWavePCMWriter>> ActiveWriters;

	/** Keep track of segments that we've already written to disk to avoid re-writing them (and generating new Output Futures) */
	TSet<FGuid> AlreadyWrittenSegments;
};