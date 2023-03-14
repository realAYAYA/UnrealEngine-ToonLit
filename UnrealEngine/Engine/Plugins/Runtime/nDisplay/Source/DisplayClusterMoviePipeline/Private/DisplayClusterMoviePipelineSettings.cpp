// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterMoviePipelineSettings.h"
#include "DisplayClusterConfigurationTypes.h"
#include "MovieRenderPipelineCoreModule.h"
#include "Modules/ModuleManager.h"

///////////////////////////////////////////////////////////////////////////////////////////
// UDisplayClusterMoviePipelineSettings
///////////////////////////////////////////////////////////////////////////////////////////
ADisplayClusterRootActor* UDisplayClusterMoviePipelineSettings::GetRootActor(const UWorld* InWorld) const
{
	if (InWorld)
	{
		for (const TWeakObjectPtr<ADisplayClusterRootActor> RootActorRef : TActorRange<ADisplayClusterRootActor>(InWorld))
		{
			if (ADisplayClusterRootActor* RootActorPtr = RootActorRef.Get())
			{
				if (!Configuration.DCRootActor.IsValid() || RootActorPtr->GetFName() == Configuration.DCRootActor->GetFName())
				{
					return RootActorPtr;
				}
			}
		}
	}

	return nullptr;
}

bool UDisplayClusterMoviePipelineSettings::GetViewports(const UWorld* InWorld, TArray<FString>& OutViewports, TArray<FIntPoint>& OutViewportResolutions) const
{
	// When DCRA not selected we get first suitable actor from scene, but always render all viewports
	const bool bForceRenderAllViewports = !Configuration.DCRootActor.IsValid() || Configuration.bRenderAllViewports;

	if (ADisplayClusterRootActor* RootActorPtr = GetRootActor(InWorld))
	{
		if (const UDisplayClusterConfigurationData* InConfigurationData = RootActorPtr->GetConfigData())
		{
			if (const UDisplayClusterConfigurationCluster* InClusterCfg =  InConfigurationData->Cluster)
			{
				for (const TPair<FString, TObjectPtr<UDisplayClusterConfigurationClusterNode>>& NodeIt : InClusterCfg->Nodes)
				{
					if (const UDisplayClusterConfigurationClusterNode* InConfigurationClusterNode = NodeIt.Value)
					{
						const FString& InClusterNodeId = NodeIt.Key;
						for (const TPair<FString, TObjectPtr<UDisplayClusterConfigurationViewport>>& InConfigurationViewportIt : InConfigurationClusterNode->Viewports)
						{
							if (const UDisplayClusterConfigurationViewport* InConfigurationViewport = InConfigurationViewportIt.Value)
							{
								if (InConfigurationViewport->bAllowRendering)
								{
									const FString& InViewportId = InConfigurationViewportIt.Key;
									if (bForceRenderAllViewports || Configuration.AllowedViewportNamesList.Find(InViewportId) != INDEX_NONE)
									{
										OutViewports.Add(InViewportId);

										if (Configuration.bUseViewportResolutions)
										{
											OutViewportResolutions.Add(InConfigurationViewportIt.Value->Region.ToRect().Size());
										}
									}
								}
							}
						}
					}
				}

				return OutViewports.Num() > 0;
			}
		}
	}

	return false;
}

IMPLEMENT_MODULE(FDefaultModuleImpl, DisplayClusterMoviePipeline);
