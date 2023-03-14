// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FGizmoEdModeModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void OnPostEngineInit();


	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
