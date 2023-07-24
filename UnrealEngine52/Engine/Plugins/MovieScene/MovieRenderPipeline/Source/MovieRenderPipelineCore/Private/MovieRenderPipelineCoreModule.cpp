// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieRenderPipelineCoreModule.h"
#include "Modules/ModuleInterface.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"
#include "MovieRenderPipelineDataTypes.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "MoviePipeline.h"

FName IMoviePipelineBurnInExtension::ModularFeatureName = "ModularFeature_MoviePipelineBurnInExt";

void FMovieRenderPipelineCoreModule::StartupModule()
{
#if WITH_EDITOR
	// Hack to ensure these assets (which are referenced only by code) get packaged
	{
		TArray<FString> Assets;
		Assets.Add(UMoviePipeline::DefaultDebugWidgetAsset);

		for (const FString& Asset : Assets)
		{
			TSoftObjectPtr<UObject> AssetRef = TSoftObjectPtr<UObject>(FSoftObjectPath(Asset));
			AssetRef.LoadSynchronous();
		}

	}
#endif

	// Look to see if they supplied arguments on the command line indicating they wish to render a movie.
	if (IsTryingToRenderMovieFromCommandLine(SequenceAssetValue, SettingsAssetValue, MoviePipelineLocalExecutorClassType, MoviePipelineClassType))
	{

		UE_LOG(LogMovieRenderPipeline, Log, TEXT("Detected that the user intends to render a movie. Waiting until engine loop init is complete to ensure "));

		// Register a hook to wait until the engine has finished loading to increase the likelihood that the desired classes are loaded.
		FCoreUObjectDelegates::PostLoadMapWithWorld.AddRaw(this, &FMovieRenderPipelineCoreModule::OnMapLoadFinished);
	}
}

void FMovieRenderPipelineCoreModule::OnMapLoadFinished(UWorld* InWorld)
{
	if (!InWorld)
	{
		return;
	}

	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);

	// We have to wait two ticks for Python classes to have a chance to be initialized too. Using a chain of function calls
	// instead of a timer to ensure it is guranteed to be two ticks regardless of how long the first frame takes.
	InWorld->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateRaw(this, &FMovieRenderPipelineCoreModule::QueueInitialize, InWorld));
}

void FMovieRenderPipelineCoreModule::QueueInitialize(UWorld* InWorld)
{
	InWorld->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateRaw(this, &FMovieRenderPipelineCoreModule::InitializeCommandLineMovieRender));
}

void FMovieRenderPipelineCoreModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FMovieRenderPipelineCoreModule, MovieRenderPipelineCore);
DEFINE_LOG_CATEGORY(LogMovieRenderPipeline); 
DEFINE_LOG_CATEGORY(LogMovieRenderPipelineIO);