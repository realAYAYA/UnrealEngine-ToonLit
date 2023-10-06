// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "MovieRenderPipelineDataTypes.h"
#include "MovieRenderPipelineCoreModule.h"
#include "MoviePipelineDeferredPasses.h"

class FMovieRenderPipelineRenderPassesModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
#if WITH_EDITOR
		// Hack to ensure these assets (which are referenced only by code) get packaged
		{
			TArray<FString> Assets;
			Assets.Add(UMoviePipelineDeferredPassBase::StencilLayerMaterialAsset);
			Assets.Add(UMoviePipelineDeferredPassBase::DefaultDepthAsset);
			Assets.Add(UMoviePipelineDeferredPassBase::DefaultMotionVectorsAsset);

			for (const FString& Asset : Assets)
			{
				TSoftObjectPtr<UObject> AssetRef = TSoftObjectPtr<UObject>(FSoftObjectPath(Asset));
				AssetRef.LoadSynchronous();
			}

		}
#endif
	}
};

IMPLEMENT_MODULE(FMovieRenderPipelineRenderPassesModule, MovieRenderPipelineRenderPasses);
