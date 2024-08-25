// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/IDelegateInstance.h"
#include "Modules/ModuleInterface.h"

class FAvaModifiersEditorModule : public IModuleInterface
{
public:
	//~ Begin IModuleInterface
    virtual void StartupModule() override;	
	virtual void ShutdownModule() override;
	//~ End IModuleInterface
	
private:
	void RegisterOutlinerItems();
	void UnregisterOutlinerItems();
	
	FDelegateHandle OutlinerProxiesExtensionDelegateHandle;
	FDelegateHandle OutlinerContextDelegateHandle;
	FDelegateHandle OutlinerDropHandlerDelegateHandle;
};