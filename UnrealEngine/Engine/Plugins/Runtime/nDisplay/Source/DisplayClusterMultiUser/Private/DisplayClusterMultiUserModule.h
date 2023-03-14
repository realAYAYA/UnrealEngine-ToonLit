// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DisplayClusterMultiUserManager.h"

#include "Modules/ModuleInterface.h"
#include "Templates/UniquePtr.h"


class FDisplayClusterMultiUserModule :
	public IModuleInterface
{
public:
	// Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	// ~ End IModuleInterface
private:
	// Manager for multi user connections.
	TUniquePtr<FDisplayClusterMultiUserManager> MultiUserManager;
};
