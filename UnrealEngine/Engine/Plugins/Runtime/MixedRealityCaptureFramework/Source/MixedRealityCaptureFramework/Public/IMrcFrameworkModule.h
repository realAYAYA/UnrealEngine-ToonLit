// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "HAL/IConsoleManager.h" // for TAutoConsoleVariable<>

class IConsoleVariable;

/**
 * 
 */
class IMrcFrameworkModule : public IModuleInterface
{
public:
	/** Virtual destructor. */
	virtual ~IMrcFrameworkModule() {}

	virtual class AMixedRealityCaptureActor* GetMixedRealityCaptureActor() { return nullptr; }
};

