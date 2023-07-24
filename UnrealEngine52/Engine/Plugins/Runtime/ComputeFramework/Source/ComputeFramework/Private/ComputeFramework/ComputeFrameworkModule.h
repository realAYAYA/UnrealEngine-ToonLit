// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ComputeFramework/IComputeFrameworkModule.h"

class FComputeFrameworkSystem;

/** ComputeFramework module implementation. */
class FComputeFrameworkModule : public IComputeFrameworkModule
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Get the instantiation of the compute system associated with the module. */
	static FComputeFrameworkSystem* GetComputeSystem() { return ComputeSystem; }

private:
	static FComputeFrameworkSystem* ComputeSystem;
};
