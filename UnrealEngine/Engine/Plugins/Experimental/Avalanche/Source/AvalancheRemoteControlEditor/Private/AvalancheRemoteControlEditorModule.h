// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FAvalancheRemoteControlEditorModule : public IModuleInterface
{
	//~ Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface

	void RegisterCustomizations();
	void UnregisterCustomizations();

    FDelegateHandle OutlinerProxiesExtensionDelegateHandle;

	TArray<FName> CustomPropertyTypeLayouts;
};
