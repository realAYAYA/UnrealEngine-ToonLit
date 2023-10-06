// Copyright Epic Games, Inc. All Rights Reserved.
#include "DirectLinkModule.h"

#include "DirectLinkEndpoint.h"
#include "DirectLinkLog.h"

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FDirectLinkModule, DirectLink);


void FDirectLinkModule::StartupModule()
{
	DirectLink::ValidateCommunicationStatus();
}


