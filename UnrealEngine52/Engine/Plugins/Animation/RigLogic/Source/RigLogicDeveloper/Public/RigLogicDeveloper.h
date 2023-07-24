// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

DECLARE_LOG_CATEGORY_EXTERN(LogRigLogicDeveloper, Log, All);

class RIGLOGICDEVELOPER_API FRigLogicDeveloperModule : public IModuleInterface 
{
public:
	void StartupModule() override;
	void ShutdownModule() override;
};
