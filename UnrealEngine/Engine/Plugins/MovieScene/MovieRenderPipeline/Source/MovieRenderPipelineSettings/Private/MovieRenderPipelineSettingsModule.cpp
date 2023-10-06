// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "MoviePipelineBurnInSetting.h"

class FMovieRenderPipelineSettingsModule : public IModuleInterface
{
	virtual void StartupModule() override
	{
#if WITH_EDITOR
		// Hack to ensure these assets (which are referenced only by code) get packaged
		{
			TArray<FString> Assets;
			Assets.Add(UMoviePipelineBurnInSetting::DefaultBurnInWidgetAsset);

			for (const FString& Asset : Assets)
			{
				TSoftObjectPtr<UObject> AssetRef = TSoftObjectPtr<UObject>(FSoftObjectPath(Asset));
				AssetRef.LoadSynchronous();
			}
		}
#endif
	}

	virtual void ShutdownModule() override
	{

	}
};

IMPLEMENT_MODULE(FMovieRenderPipelineSettingsModule, MovieRenderPipelineSettings);
