// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Modules/ModuleInterface.h"

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

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
