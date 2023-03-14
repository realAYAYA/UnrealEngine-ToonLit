// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Modules/ModuleInterface.h"
#include "CoreMinimal.h"

#define INTERCHANGETESTS_MODULE_NAME TEXT("InterchangeTests")

/**
 * Module for implementing Interchange automation tests
 */
class INTERCHANGETESTS_API FInterchangeTestsModule : public IModuleInterface
{
public:
	static FInterchangeTestsModule& Get();
	static bool IsAvailable();

private:
	virtual void StartupModule() override;
};
