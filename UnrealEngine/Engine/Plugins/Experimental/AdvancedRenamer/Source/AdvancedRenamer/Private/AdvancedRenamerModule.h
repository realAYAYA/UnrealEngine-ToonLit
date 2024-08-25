// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IAdvancedRenamerModule.h"
#include "Logging/LogMacros.h"

DECLARE_LOG_CATEGORY_EXTERN(LogARP, Log, All);

/** Advanced Rename Panel Plugin - Easily bulk rename stuff! */
class FAdvancedRenamerModule : public IAdvancedRenamerModule
{
	//~ Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface

	//~ Begin IAdvancedRenamerModule
	virtual void OpenAdvancedRenamer(const TSharedRef<IAdvancedRenamerProvider>& InRenameProvider, const TSharedPtr<SWidget>& InParentWidget) override;
	virtual void OpenAdvancedRenamer(const TSharedRef<IAdvancedRenamerProvider>& InRenameProvider, const TSharedPtr<IToolkitHost>& InToolkitHost) override;
	virtual void OpenAdvancedRenamerForActors(const TArray<AActor*>& InActors, const TSharedPtr<SWidget>& InParentWidget) override;
	virtual void OpenAdvancedRenamerForActors(const TArray<AActor*>& InActors, const TSharedPtr<IToolkitHost>& InToolkitHost) override;
	virtual TArray<AActor*> GetActorsSharingClassesInWorld(const TArray<AActor*>& InActors) override;
	//~ End IAdvancedRenamerModule
};
