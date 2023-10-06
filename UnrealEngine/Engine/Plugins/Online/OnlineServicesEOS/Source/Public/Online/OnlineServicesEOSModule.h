// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"

namespace UE::Online {

class FOnlineServicesEOSModule : public IModuleInterface
{
public:
	ONLINESERVICESEOS_API static int GetRegistryPriority();

private:
	// Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	// End IModuleInterface
};

} // namespace UE::Online