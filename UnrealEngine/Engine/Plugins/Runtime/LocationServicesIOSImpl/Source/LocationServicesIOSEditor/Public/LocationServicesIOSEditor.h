// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Modules/ModuleInterface.h"

class FLocationServicesIOSEditorModule :
	public IModuleInterface
{
private:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

