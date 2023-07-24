// Copyright Epic Games, Inc. All Rights Reserved.

#include "UIFLocalSettings.h"

#include "Async/Async.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

UUIFrameworkLocalSettings::UUIFrameworkLocalSettings()
{
	ErrorResource = FSoftObjectPath(TEXT("/UIFramework/Materials/T_Icon_Warning_32.T_Icon_Warning_32"));
	LoadingResource = FSoftObjectPath(TEXT("/UIFramework/Materials/M_UI_DefaultLoading.M_UI_DefaultLoading"));
}

void UUIFrameworkLocalSettings::LoadResources() const
{
	Async(EAsyncExecution::TaskGraphMainThread, []()
		{
			const UUIFrameworkLocalSettings* Settings = GetDefault<UUIFrameworkLocalSettings>();
			TArray<FSoftObjectPath> ObjectsToLoad;
			ObjectsToLoad.Reserve(2);
			ObjectsToLoad.Add(Settings->ErrorResource.ToSoftObjectPath());
			ObjectsToLoad.Add(Settings->LoadingResource.ToSoftObjectPath());
			UAssetManager::GetStreamableManager().RequestAsyncLoad(
				ObjectsToLoad,
				[]()
				{
					if (UObjectInitialized() && !IsEngineExitRequested())
					{
						UUIFrameworkLocalSettings* Default = GetMutableDefault<UUIFrameworkLocalSettings>();
						Default->ErrorResourcePtr = Default->ErrorResource.Get();
						if (Default->ErrorResourcePtr)
						{
							Default->ErrorResourcePtr->AddToRoot();
						}

						Default->LoadingResourcePtr = Default->LoadingResource.Get();
						if (Default->LoadingResourcePtr)
						{
							Default->LoadingResourcePtr->AddToRoot();
						}
					}
				},
				FStreamableManager::DefaultAsyncLoadPriority);
		});
}


FName UUIFrameworkLocalSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}

#if WITH_EDITOR
void UUIFrameworkLocalSettings::PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);
	LoadResources();
}
#endif