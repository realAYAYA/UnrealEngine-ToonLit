// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDisplayClusterLightCardEditor.h"

/**
 * Display Cluster editor module
 */
class FDisplayClusterLightCardEditorModule :
	public IDisplayClusterLightCardEditor
{
public:
	//~ IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
	virtual void ShowLabels(const FLabelArgs& InArgs) override;
	virtual UDisplayClusterStageActorTemplate* GetDefaultLightCardTemplate() const override;
	
private:
	void RegisterSettings();
	void UnregisterSettings();

	void RegisterDetailCustomizations();
	void UnregisterDetailCustomizations();

	void RegisterOperatorApp();
	void UnregisterOperatorApp();
	
	void HandleModuleChanged(FName InModuleName, EModuleChangeReason InChangeReason);
	
private:
	FDelegateHandle ModuleChangedHandle;
	FDelegateHandle OperatorAppHandle;
};
