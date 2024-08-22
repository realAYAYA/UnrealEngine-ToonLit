// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogCustomEditor, Log, All);

class FToolBarBuilder;
class FMenuBuilder;

class FBlueprintToolbar;
class FLevelToolbar;
class FCustomFileWatcher;

class FCustomEditorModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:

	void RegisterMenus();

	void OnPostEngineInit();

	TSharedPtr<FLevelToolbar> LevelToolbar;
	TSharedPtr<FBlueprintToolbar> BlueprintToolbar;
	FDelegateHandle OnPostEngineInitHandle;

	TUniquePtr<FCustomFileWatcher> ProtocolFileWatcher; 
};
