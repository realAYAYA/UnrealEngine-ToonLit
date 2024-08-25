// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"

class UGizmoSettings;

class FGizmoSettingsModule : public IModuleInterface
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	void UpdateGizmos(const UGizmoSettings* InSettings);
};
