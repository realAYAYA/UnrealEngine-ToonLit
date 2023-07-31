// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IOpenXREditorModule.h"
#include "Modules/ModuleInterface.h"

#define OPENXR_EDITOR_MODULE_NAME "OpenXREditor"

class FOpenXREditorModule : public IOpenXREditorModule
{
public:
	FOpenXREditorModule() {};

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
