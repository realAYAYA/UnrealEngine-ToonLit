// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Templates/SharedPointer.h"

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

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
