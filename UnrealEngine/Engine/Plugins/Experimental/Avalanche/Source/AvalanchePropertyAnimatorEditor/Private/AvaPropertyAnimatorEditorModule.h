// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FAvaPropertyAnimatorEditorModule : public IModuleInterface
{
public:
	//~ Begin IModuleInterface
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
	//~ End IModuleInterface

protected:
	void RegisterOutlinerItems();
	void UnregisterOutlinerItems();

	FDelegateHandle OutlinerProxiesExtensionDelegateHandle;
	FDelegateHandle OutlinerContextDelegateHandle;
};
