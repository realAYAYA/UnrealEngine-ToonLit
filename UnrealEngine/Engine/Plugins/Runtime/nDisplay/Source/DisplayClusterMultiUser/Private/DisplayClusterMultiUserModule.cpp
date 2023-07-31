// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterMultiUserModule.h"

#include "Modules/ModuleManager.h"


void FDisplayClusterMultiUserModule::StartupModule()
{
	MultiUserManager = MakeUnique<FDisplayClusterMultiUserManager>();
}

void FDisplayClusterMultiUserModule::ShutdownModule()
{
	MultiUserManager.Reset();
}

IMPLEMENT_MODULE(FDisplayClusterMultiUserModule, DisplayClusterMultiUser);
