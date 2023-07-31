// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class FControlRigSplineModule : public IModuleInterface
{
	// IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

