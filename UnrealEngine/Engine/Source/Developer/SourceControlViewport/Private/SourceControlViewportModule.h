// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FSourceControlViewportOutlineMenu;
class FSourceControlViewportToolTips;

class FSourceControlViewportModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/**
	 * Gets a reference to the source control outline menu module instance.
	 *
	 * @return A reference to the source control outline menu module.
	 */
	static FSourceControlViewportModule& Get()
	{
		static FName SourceControlViewportModule("SourceControlViewport");
		return FModuleManager::LoadModuleChecked<FSourceControlViewportModule>(SourceControlViewportModule);
	}

	static FSourceControlViewportModule* TryGet()
	{
		static FName SourceControlViewportModule("SourceControlViewport");
		return FModuleManager::GetModulePtr<FSourceControlViewportModule>(SourceControlViewportModule);
	}

private:
	TSharedPtr<FSourceControlViewportOutlineMenu> ViewportOutlineMenu;
	TSharedPtr<FSourceControlViewportToolTips> ViewportToolTips;
};
