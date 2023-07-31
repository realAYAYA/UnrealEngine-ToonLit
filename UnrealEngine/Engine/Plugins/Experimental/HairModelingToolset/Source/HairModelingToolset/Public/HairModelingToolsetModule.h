// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "ModelingModeToolExtensions.h"

class FHairModelingToolsetModule : public IModuleInterface, public IModelingModeToolExtension
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** IModelingModeToolExtension implementation */
	virtual FText GetExtensionName() override;
	virtual FText GetToolSectionName() override;
	virtual void GetExtensionTools(const FExtensionToolQueryInfo& QueryInfo, TArray<FExtensionToolDescription>& ToolsOut) override;
};
