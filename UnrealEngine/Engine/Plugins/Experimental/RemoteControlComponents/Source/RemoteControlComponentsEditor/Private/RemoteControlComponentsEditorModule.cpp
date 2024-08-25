// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "Styles/RemoteControlComponentsEditorStyle.h"

class FRemoteControlComponentsEditorModule : public IModuleInterface
{
public:
	//~ Begin IModuleInterface
	virtual void StartupModule() override
	{
		// Initialize the Style on Startup Module
		FRemoteControlComponentsEditorStyle::Get();
	}
	//~ End IModuleInterface
};

IMPLEMENT_MODULE(FRemoteControlComponentsEditorModule, RemoteControlComponentsEditor)
