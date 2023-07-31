// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class FAssetTypeActions_InterchangeImportTestPlan;


#define INTERCHANGETESTEDITOR_MODULE_NAME TEXT("InterchangeTestEditor")

/**
 * Module for implementing the editor for Interchange automation tests
 */
class FInterchangeTestEditorModule : public IModuleInterface
{
public:
	static FInterchangeTestEditorModule& Get();
	static bool IsAvailable();

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	TSharedPtr<FAssetTypeActions_InterchangeImportTestPlan> AssetTypeActions;
};
