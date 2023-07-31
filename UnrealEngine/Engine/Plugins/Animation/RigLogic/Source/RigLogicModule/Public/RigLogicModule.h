// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

DECLARE_LOG_CATEGORY_EXTERN(LogRigLogic, Log, All);

class RIGLOGICMODULE_API FRigLogicModule: public IModuleInterface
{
public:
	void StartupModule() override;
	void ShutdownModule() override;
};
