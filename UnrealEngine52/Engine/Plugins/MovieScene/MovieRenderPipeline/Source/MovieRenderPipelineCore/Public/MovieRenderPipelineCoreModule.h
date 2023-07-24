// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats2.h"
#include "Modules/ModuleInterface.h"
#include "Features/IModularFeature.h"

// Forward Declare
class UMoviePipelineExecutorBase;
class UMoviePipelineQueue;
class ULevelSequence;
class UMoviePipeline;

// Declare a stat-group for our performance stats to be counted under, readable in game by "stat MovieRenderPipeline".
DECLARE_STATS_GROUP(TEXT("MovieRenderPipeline"), STATGROUP_MoviePipeline, STATCAT_Advanced);

namespace MoviePipelineErrorCodes
{
	/** Everything completed as expected or we (unfortunately) couldn't detect the error. */
	constexpr uint8 Success = 0;
	/** Fallback for any generic critical failure. This should be used for "Unreal concepts aren't working as expected" severity errors. */
	constexpr uint8 Critical = 1;
	/** The specified level sequence asset could not be found. Check the logs for details on what it looked for. */
	constexpr uint8 NoAsset = 2;
	/** The specified pipeline configuration asset could not be found. Check the logs for details on what it looked for. */
	constexpr uint8 NoConfig = 3;

}


class MOVIERENDERPIPELINECORE_API FMovieRenderPipelineCoreModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	bool IsTryingToRenderMovieFromCommandLine(FString& OutSequenceAssetPath, FString& OutConfigAssetPath, FString& OutExecutorType, FString& OutPipelineType) const;
	void OnMapLoadFinished(class UWorld* InWorld);
	void QueueInitialize(class UWorld* InWorld);
	void InitializeCommandLineMovieRender();

	void OnCommandLineMovieRenderCompleted(UMoviePipelineExecutorBase* InExecutor, bool bSuccess);
	void OnCommandLineMovieRenderErrored(UMoviePipelineExecutorBase* InExecutor, UMoviePipeline* InPipelineWithError, bool bIsFatal, FText ErrorText);

	uint8 ParseMovieRenderData(const FString& InSequenceAssetPath, const FString& InConfigAssetPath, const FString& InExecutorType, const FString& InPipelineType,
		UMoviePipelineQueue*& OutQueue, UMoviePipelineExecutorBase*& OutExecutor) const;

private:
	FString MoviePipelineLocalExecutorClassType;
	FString MoviePipelineClassType;
	FString SequenceAssetValue;
	FString SettingsAssetValue;
};

class MOVIERENDERPIPELINECORE_API IMoviePipelineBurnInExtension : public IModularFeature
{
public:
	virtual ~IMoviePipelineBurnInExtension() {}
	static FName ModularFeatureName;

	virtual FText GetEngineChangelistLabel() const { return FText(); }
};


MOVIERENDERPIPELINECORE_API DECLARE_LOG_CATEGORY_EXTERN(LogMovieRenderPipeline, Log, All);
MOVIERENDERPIPELINECORE_API DECLARE_LOG_CATEGORY_EXTERN(LogMovieRenderPipelineIO, Log, All);