// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

GEOMETRYFLOWMESHPROCESSING_API DECLARE_LOG_CATEGORY_EXTERN(LogGeometryFlowMeshProcessing, Log, All);

class FGeometryFlowMeshProcessingModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
