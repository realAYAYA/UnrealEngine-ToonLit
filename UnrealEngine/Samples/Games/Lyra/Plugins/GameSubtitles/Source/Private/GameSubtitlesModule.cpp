// Copyright Epic Games, Inc. All Rights Reserved.

#include "Containers/UnrealString.h"
#include "GameplayTagsManager.h"
#include "HAL/Platform.h"
#include "Misc/Paths.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class FGameSubtitlesModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

void FGameSubtitlesModule::StartupModule()
{
	UGameplayTagsManager::Get().AddTagIniSearchPath(FPaths::ProjectPluginsDir() / TEXT("GameSubtitles/Config/Tags"));
}

void FGameSubtitlesModule::ShutdownModule()
{
}
	
IMPLEMENT_MODULE(FGameSubtitlesModule, GameSubtitles)