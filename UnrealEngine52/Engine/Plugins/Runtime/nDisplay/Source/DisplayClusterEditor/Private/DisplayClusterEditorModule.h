// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDisplayClusterEditor.h"


/**
 * Display Cluster editor module
 */
class FDisplayClusterEditorModule :
	public IDisplayClusterEditor
{
public:
	//~ IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

protected:
	void RegisterSettings();
	void UnregisterSettings();
};
