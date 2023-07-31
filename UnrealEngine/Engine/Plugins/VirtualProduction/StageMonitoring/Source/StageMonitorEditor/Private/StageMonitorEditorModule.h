// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Modules/ModuleManager.h"
#include "Modules/ModuleInterface.h"

DECLARE_LOG_CATEGORY_EXTERN(LogStageMonitorEditor, Log, All);

class FStageMonitorEditorModule : public IModuleInterface
{
public:	

	//~ Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~End IModuleInterface

private:
	/** Registers detail customization for stage monitoring types */
	void RegisterCustomizations();
	
	/** Unregisters registered customizations */
	void UnregisterCustomizations();
};
